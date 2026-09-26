"""Walk compiled Control Basic programs the way the interpreter does, and check them.

    python tools/check_programs.py FILE...      .prog/.prg configuration files or raw rows

Each program is one 2,000-byte row of prg_code[] (bacnet/private/ptransfer.c), as
T3000 compiles and sends it. bacnet/private/decode.c runs it in place. It forms
pointers from offsets the bytecode carries: jump targets, local-variable and
time-table offsets, and the lengths in the header. It then reads and writes through
them. This tool walks every statement and every expression token the way
exec_program_code and veval_exp step over them, and checks that each such pointer
lands where it should.

It exists to test the interpreter's run-time bounds against real programs. Those
bounds fire only when a pointer would leave the program's own row, and the checks
here are stricter: a jump must land on a line, and a local offset must lie inside
the local table. A program that passes here cannot trip the firmware's bounds.
Anything this walker cannot follow is reported, not guessed at.

Opcode values come from bacnet/private/basic.h. Where decode.c steps in an
unexpected way, the walker copies it and says so in a comment.

The programs it has been run against use ASSIGN, IF, IF+, IF-, ELSE, GOTO,
THEN <line>, END, REM, START and STOP, and no FOR/NEXT, ON, GOSUB, WAIT, ALARM,
DALARM or ALARM-AT. The walk for those follows decode.c but has not met a real
program yet, so a FAIL in one of them may be the walker's.

Input files:
  * T3000 configuration files (55 FF, version 6 or 8). The program rows sit at a
    fixed offset; see T3000's SaveBacnetBinaryFile.
  * Anything else is scanned for byte runs that frame like a program. A run that
    only happens to frame like one fails the walk. It shows up as FAIL with the
    file offset, so look at it before believing it.
"""

import struct
import sys

ROW = 2000                      # sizeof(prg_code[0])
PROGRAM_CODE_AT = 33956         # program rows in a version 6/8 .prog (16 x 2000)

# ---- basic.h -----------------------------------------------------------------
LOCAL_VARIABLE, FLOAT_TYPE, LONG_TYPE, INTEGER_TYPE, BYTE_TYPE, STRING_TYPE = 0x82, 0x83, 0x84, 0x85, 0x86, 0x87
FLOAT_TYPE_ARRAY, LONG_TYPE_ARRAY, INTEGER_TYPE_ARRAY, BYTE_TYPE_ARRAY, STRING_TYPE_ARRAY = 0x88, 0x89, 0x8A, 0x8B, 0x8C
LOCAL_POINT_PRG, CONST_VALUE_PRG, REMOTE_POINT_PRG = 0x9C, 0x9D, 0x9E
POINT_SIZE, POINT_NET_SIZE = 2, 5       # sizeof(Point), sizeof(Point_Net), packed
ARRAY_POINT = 11 + 1                    # point_type - 1 == ARRAY (ud_str.h)

ASSIGNAR, ASSIGN, CLEARX, FOR, NEXT, IF = 0x08, 0x09, 0x0A, 0x0B, 0x0D, 0x0E
ELSE, IFP, IFM, GOTO, GOSUB, RETURN, ENDPRG, PRINT = 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x17, 0x18
REM, STARTPRG, STOP, WAIT, HANGUP = 0x1A, 0x1C, 0x1D, 0x1E, 0x1F
ALARM_AT, RUN_MACRO, ENABLEX, DISABLEX, SET_PRINTER = 0x21, 0x23, 0x25, 0x26, 0x28
ASSIGNARRAY_1, GOTOIF, ON_ALARM, ASSIGNARRAY_2, OPEN, CLOSE = 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E
ASSIGNARRAY, ON, ALARM, DALARM, DIM, CLEARPORT = 0x31, 0x40, 0x41, 0x43, 0x8A, 0x59
ICONS = (0xB4, 0xB5, 0xB6, 0xB7)        # ICON1-4, compiled in for ARM_TSTAT_WIFI (this build)
LT, GT = 0x6D, 0x6E

# veval_exp tokens that are one byte and move prog no further
ONE_BYTE = {
    0x60, 0x61, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6B, 0x6C, 0x6D, 0x6E,          # GE LE POW MOD MUL DIV MINUSUNAR PLUS MINUS LT GT
    0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x79, 0x7A, 0x7B,                      # EQ NE XOR AND OR NOT BITAND BITOR INTDIV
    0x32, 0x4D, 0x4E, 0xD1, 0xD2, 0x39, 0x36, 0xA4, 0xA5, 0x3F,                # ABS LN LN_1 SIN COS SQR INT SENSOR_ON/OFF TIME_FORMAT
    0x47, 0x56, 0x57, 0x58, 0x3A, 0x40, 0x41,                                  # STATUS PIDPROP PIDDERIV PIDINT TBL WR_ON WR_OFF
    0x52, 0x35, 0x34, 0x53, 0x4C, 0x4B, 0x3B, 0x45, 0x46, 0x42, 0x51, 0x50,    # DOM DOW DOY MOY POWER_LOSS SCANS TIME USER_A/B UNACK INKEYD OUTPUTD
    0x07, 0x08, 0x02, 0x03, 0x04, 0x05, 0x06,                                  # SUN .. SAT
    0x29, 0x2C,                                                                # ASSIGNARRAY_1/2: empty cases in veval_exp
} | set(range(0xC1, 0xCD))                                                     # JAN .. DEC
COUNTED = {0x33, 0xCE, 0xCF, 0x37, 0x38}           # AVG MB_BW MB_BW_COIL MAX MIN: + 1 count byte
COM1 = 0x10     # counted too, but compiled in only for ARM_MINI and ASIX_MINI
TIME_ON, TIME_OFF, INTERVAL = 0x3C, 0x3D, 0x3E
DELIMITERS = (0x01, 0xFF, 0xFE)                    # isdelimit()

LOCAL_WIDTH = {FLOAT_TYPE: 4, LONG_TYPE: 4, INTEGER_TYPE: 2, BYTE_TYPE: 1}
ARRAY_WIDTH = {FLOAT_TYPE_ARRAY: 4, LONG_TYPE_ARRAY: 4, INTEGER_TYPE_ARRAY: 2, BYTE_TYPE_ARRAY: 1}


class Bad(Exception):
    """The walk found something the interpreter would mishandle, or could not follow."""


def u16(b, at):
    return struct.unpack_from("<H", b, at)[0]


class Program:
    def __init__(self, row):
        self.b = bytearray(row.ljust(ROW, b"\0")[:ROW])
        b = self.b
        self.base_len = u16(b, 0)
        if self.base_len == 0:
            raise Bad("empty")
        # exec_program_code: prog = row + base_len + 2 + 3 holds the local table length
        at = self.base_len + 5
        if at + 2 > ROW:
            raise Bad("code length %d leaves no room for the tables" % self.base_len)
        self.local_len = u16(b, at)
        self.local_at = at + 2
        at = self.local_at + self.local_len
        if at + 2 > ROW:
            raise Bad("local table (%d bytes) runs past the row" % self.local_len)
        self.time_len = u16(b, at)
        self.time_at = at + 2
        self.total = self.time_at + self.time_len
        if self.total > ROW:
            raise Bad("time table (%d bytes) runs past the row" % self.time_len)
        self.end = 2 + self.base_len                   # the 0xFE after the last line
        if b[self.end] != 0xFE:
            raise Bad("no 0xFE after %d bytes of code" % self.base_len)
        # WAIT's resume offset, from p_buf (the row + 2); 0 unless a WAIT is pending
        self.resume = u16(b, self.end + 1)
        if 2 + self.resume > self.end:
            raise Bad("WAIT resume offset %d is past the code" % self.resume)
        self.lines = {}             # row offset of each 0x01 -> line number
        self.jumps = []             # (row offset of the jump, what, target offset)
        self.locals = []            # (row offset, type, offset into the local table)
        self.times = []             # (row offset, offset into the time table)
        self.max_local_end = 0
        self.opcodes = set()

    # -- expressions: veval_exp and operand ------------------------------------
    def operand(self, p):
        b = self.b
        t = b[p]
        if LOCAL_VARIABLE <= t <= BYTE_TYPE:
            self.local(p, t, u16(b, p + 1))
            return p + 3
        if t == LOCAL_POINT_PRG:
            if b[p + 2] == ARRAY_POINT:                # get_ay_elem: the index follows the point
                return self.expr(p + 1 + POINT_SIZE)
            return p + 1 + POINT_SIZE
        if t == REMOTE_POINT_PRG:
            if b[p + 2] == ARRAY_POINT:
                return self.expr(p + 1 + POINT_NET_SIZE)
            return p + 1 + POINT_NET_SIZE
        if t == CONST_VALUE_PRG:
            return p + 5
        # operand() returns without moving prog, so veval_exp would spin on this byte until
        # isdelimit's 2,000-byte counter gives up.
        raise Bad("expression byte 0x%02X at %d is not an operand the interpreter knows" % (t, p))

    def expr(self, p):
        b = self.b
        if LOCAL_VARIABLE <= b[p] <= REMOTE_POINT_PRG:
            p = self.operand(p)
        while b[p] not in DELIMITERS:
            if p >= ROW - 1:
                raise Bad("expression runs off the row")
            t = b[p]
            p += 1
            if t in ONE_BYTE:
                continue
            if t in COUNTED:
                p += 1
            elif t in (TIME_ON, TIME_OFF):
                self.times.append((p - 1, u16(b, p)))
                p += 2
            elif t == INTERVAL:
                p += 4
            elif t == COM1:
                # Not in the T3-OEM build, so veval_exp reaches operand(), which cannot
                # step over it either; and 0x10 is ELSE to the statement loop.
                raise Bad("COM1 at %d: this firmware has no COM1 and misreads what follows" % (p - 1))
            elif t == ASSIGNARRAY:                      # pushes a local array's offset
                if LOCAL_VARIABLE <= b[p] <= STRING_TYPE_ARRAY:
                    self.local(p, b[p], u16(b, p + 1))
                p += 3
            else:
                p = self.operand(p - 1)
        if b[p] == 0xFF:
            p += 1
        return p

    # -- what the checks look at -----------------------------------------------
    def local(self, at, typ, off):
        self.locals.append((at, typ, off))
        width = LOCAL_WIDTH.get(typ, ARRAY_WIDTH.get(typ, 1))
        if typ in ARRAY_WIDTH:
            # put_local_array: rows at local[off-4], columns at local[off-2], elements from off
            if off < 4 or off > self.local_len:
                raise Bad("array offset %d at %d is outside the %d-byte local table" % (off, at, self.local_len))
            rows = struct.unpack_from("<h", self.b, self.local_at + off - 4)[0]
            cols = struct.unpack_from("<h", self.b, self.local_at + off - 2)[0]
            width *= max(rows, 1) * max(cols, 0)
        if off + width > self.local_len:
            raise Bad("local offset %d (+%d) at %d is outside the %d-byte local table"
                      % (off, width, at, self.local_len))
        self.max_local_end = max(self.max_local_end, off + width)

    def jump(self, at, what, target):
        """target is an offset from the start of the row: prog = p_buf + i - 2."""
        self.jumps.append((at, what, target))

    def target_var(self, p):
        """The point or local an assignment-type statement names (exec_program_code)."""
        b = self.b
        t = b[p]
        if LOCAL_VARIABLE <= t <= STRING_TYPE_ARRAY:
            self.local(p, t, u16(b, p + 1))
            return p + 3
        if t == LOCAL_POINT_PRG:
            return p + 1 + POINT_SIZE
        if t == REMOTE_POINT_PRG:
            return p + 1 + POINT_NET_SIZE
        return p                                        # nothing: the interpreter does not move either

    # -- statements: exec_program_code ------------------------------------------
    def statement(self, p):
        b = self.b
        op = b[p]
        self.opcodes.add(op)
        p += 1
        if op in (ASSIGN, ASSIGNAR, ASSIGNARRAY_1, ASSIGNARRAY_2, STARTPRG, OPEN, ENABLEX, STOP, CLOSE, DISABLEX):
            p = self.target_var(p)
            n = {ASSIGN: 1, ASSIGNAR: 2, ASSIGNARRAY_1: 2, ASSIGNARRAY_2: 3}.get(op, 0)
            for _ in range(n):
                p = self.expr(p)
            return p
        if op in (REM, DIM, INTEGER_TYPE, BYTE_TYPE, STRING_TYPE, LONG_TYPE, FLOAT_TYPE):
            return p + 1 + b[p]
        if op in (PRINT, CLEARX, CLEARPORT, ENDPRG, RETURN, HANGUP, SET_PRINTER, RUN_MACRO):
            return p
        if op == ON:
            p = self.expr(p)
            n = b[p + 1]                                # b[p] says GOSUB or GOTO
            for k in range(n):
                self.jump(p, "ON", u16(b, p + 2 + 2 * k))
            return p + 2 + 2 * n
        if op in (GOSUB, GOTO, ON_ALARM):
            self.jump(p - 1, {GOSUB: "GOSUB", GOTO: "GOTO", ON_ALARM: "ON-ALARM"}[op], u16(b, p))
            return p + 2
        if op == GOTOIF:
            # the interpreter jumps at once; T3000 writes one more byte, which disasm skips
            self.jump(p - 1, "THEN n", u16(b, p))
            return p + 3
        if op == ALARM:
            # The interpreter patches the first LT (else GT) in the next 30 bytes to 0xFF and
            # evaluates three expressions; that byte is where the first one stops.
            window = bytes(b[p:p + 30])
            k = window.find(bytes([LT]))
            if k < 0:
                k = window.find(bytes([GT]))
            if k < 0:
                raise Bad("ALARM at %d has no < or > in reach" % (p - 1))
            patch = p + k
            saved = b[patch]
            b[patch] = 0xFF
            try:
                for _ in range(3):
                    p = self.expr(p)
            finally:
                b[patch] = saved
            n = b[p]
            return p + 1 + n + 1                        # message, then the state byte
        if op == ALARM_AT:
            if b[p] == 0xFF:
                return p + 1
            while p < ROW and b[p]:
                p += 1
            return p + 1
        if op == DALARM:
            p = self.expr(p) + 4                        # condition, delay
            n = b[p]
            return p + 1 + n + 4                        # message, counter
        if op == FOR:
            self.local(p, b[p], u16(b, p + 1))
            p += 3
            for _ in range(3):
                p = self.expr(p)
            self.jump(p, "FOR exit", u16(b, p))
            return p + 2
        if op == NEXT:
            target = u16(b, p)
            self.jump(p - 1, "NEXT", target)
            if not (target + 4 < ROW and b[target] == 0x01 and b[target + 3] == FOR):
                raise Bad("NEXT at %d does not point at a FOR line" % (p - 1))
            return p + 2
        if op in (IF, IFP, IFM):
            p = self.expr(p)
            if op != IF:
                p += 1                                  # edge state
            self.jump(p, "IF", u16(b, p))
            return p + 2
        if op == ELSE:
            self.jump(p, "ELSE", u16(b, p + 1))
            return p + 3
        if op == WAIT:
            if b[p] == 0xA1:
                p += 5
            else:
                p = self.expr(p)
            return p + 4
        if op in ICONS:
            return self.expr(p) + 4
        raise Bad("statement byte 0x%02X at %d is not one the interpreter handles" % (op, p - 1))

    def walk(self):
        b = self.b
        # the time table: entries of  expression 0xFF state counter[4]
        p = self.time_at
        while p < self.total:
            p = self.expr(p)
            if p + 5 > self.total:
                raise Bad("time table entry at %d runs past the table" % p)
            p += 5
        # the code: lines of  0x01 line# statement...
        p = 2
        while p < self.end:
            if b[p] != 0x01:
                raise Bad("expected a line at %d, found 0x%02X" % (p, b[p]))
            self.lines[p] = u16(b, p + 1)
            p += 3
            while p < self.end and b[p] != 0x01:
                if b[p] == 0xFF:                        # a clause's trailing 0xFF: the default case skips it
                    p += 1
                    continue
                p = self.statement(p)
        if p != self.end:
            raise Bad("the last statement runs %d bytes past the code" % (p - self.end))
        clause_starts = self.clause_starts()
        for at, what, target in self.jumps:
            if not (2 <= target <= self.end):
                raise Bad("%s at %d jumps to %d, outside the code (2..%d)" % (what, at, target, self.end))
            if target in self.lines or target == self.end:
                continue
            if what in ("IF", "ELSE") and target in clause_starts:
                continue
            raise Bad("%s at %d jumps to %d, which is not a line" % (what, at, target))
        for at, off in self.times:
            if not (1 <= off and off + 4 <= self.time_len):
                raise Bad("TIME_ON/OFF at %d names time offset %d of %d" % (at, off, self.time_len))

    def clause_starts(self):
        """Where an ELSE clause starts: the byte after ELSE's offset."""
        out = set()
        for at, what, target in self.jumps:
            if what == "ELSE":
                out.add(at + 3)
        return out


def programs_in(path):
    data = open(path, "rb").read()
    if data[:2] == b"\x55\xff" and data[2] in (6, 8) and len(data) >= PROGRAM_CODE_AT + 16 * ROW:
        for i in range(16):
            at = PROGRAM_CODE_AT + i * ROW
            yield "PRG%d" % (i + 1), data[at:at + ROW]
        return
    # anything else: byte runs that frame like a program
    for i in range(len(data) - 12):
        n = u16(data, i)
        if not (4 <= n < ROW) or data[i + 2] != 0x01 or i + 2 + n + 7 >= len(data) or data[i + 2 + n] != 0xFE:
            continue
        yield "@%d" % i, data[i:i + ROW]


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    failed = 0
    checked = 0
    for path in argv:
        for name, row in programs_in(path):
            try:
                prog = Program(row)
            except Bad as e:
                if str(e) == "empty":
                    continue
                print("FAIL %s %s: %s" % (path, name, e))
                failed += 1
                continue
            try:
                prog.walk()
            except (Bad, IndexError, struct.error) as e:
                print("FAIL %s %s: %s" % (path, name, e))
                failed += 1
                continue
            checked += 1
            print("ok   %s %s: code %d, locals %d (used to %d), time %d, total %d, %d lines, %d jumps"
                  % (path, name, prog.base_len, prog.local_len, prog.max_local_end, prog.time_len,
                     prog.total, len(prog.lines), len(prog.jumps)))
    print("%d programs walked, %d failed" % (checked, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
