"""Run real and damaged programs through two versions of decode.c and compare them.

    python tools/decode_harness/run.py [--base REF] [--mutants N] FILE...

FILE is anything tools/check_programs.py reads: T3000 configuration files
(.prog/.prg) or raw program rows. Every program in them whose header is sound
is in the corpus, including ones check_programs.py fails. Each is also damaged N times (default 150): a few random bytes, a header
length, a jump target, a byte inserted or deleted, the WAIT resume offset, a
local-variable or time-table offset.

Both bacnet/private/decode.c as it is now and as it is at REF (default main) are
built with the harness as 32-bit x86 by Visual Studio's cl.exe (set VCVARS32 to
vcvars32.bat if it is not at the default path). Every row runs in three memory
layouts (see harness.c), and the report says:

  * how many corpus programs behave differently between the two versions. A
    change that only confines the interpreter should leave them all alone,
    unless the base version leaves the row on one of them;
  * how often each version reads or writes past the row it is given, or crashes;
  * damaged programs that behave differently when neither the base version left
    the row nor the new one abandoned the scan. That is a change nobody asked
    for, and each one needs looking at.

It exits 1 if the current version writes outside its row or the differences are
not all explained.
"""

import argparse
import collections
import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.dirname(HERE))
import check_programs as cp  # noqa: E402

VCVARS32 = os.environ.get("VCVARS32", r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat")
OUT_OF_BOUNDS = 1 << 4          # generate_program_alarm(4, ...): the scan was abandoned for leaving the row


def build(tag, decode_src, basic_src, work):
    d = os.path.join(work, tag)
    os.makedirs(d)
    with open(os.path.join(d, "decode.c"), "wb") as f:
        f.write(decode_src)
    with open(os.path.join(d, "basic.h"), "wb") as f:
        f.write(basic_src)
    bat = os.path.join(d, "build.bat")
    with open(bat, "w") as f:
        f.write('@echo off\r\ncall "%s" >nul\r\ncd /d "%s"\r\n' % (VCVARS32, d))
        f.write('cl /nologo /W1 /O1 /I"%s" /Feharness.exe "%s" decode.c\r\n'
                % (os.path.join(HERE, "inc"), os.path.join(HERE, "harness.c")))
    r = subprocess.run(["cmd", "/c", bat], capture_output=True, text=True)
    exe = os.path.join(d, "harness.exe")
    if not os.path.exists(exe):
        sys.exit("build of %s failed:\n%s" % (tag, r.stdout + r.stderr))
    return exe


def git_show(ref, path):
    return subprocess.run(["git", "-C", ROOT, "show", "%s:%s" % (ref, path)], capture_output=True, check=True).stdout


def corpus(files):
    rows = []
    for path in files:
        for name, row in cp.programs_in(path):
            try:
                p = cp.Program(row)
            except (cp.Bad, IndexError, struct.error):
                continue
            try:
                p.walk()        # for the jumps and offsets to damage; COM1 stops it part way
            except (cp.Bad, IndexError, struct.error):
                pass
            rows.append(("%s %s" % (os.path.basename(path), name), bytes(row).ljust(2000, b"\0")[:2000], p))
    return rows


def damage(rows, n, seed=7):
    rnd = random.Random(seed)
    out = []
    for label, r, p in rows:
        for t in range(n):
            b = bytearray(r)
            kind = t % 6
            if kind == 0:
                for _ in range(rnd.randint(1, 4)):
                    b[rnd.randrange(2, p.end)] = rnd.randrange(256)
            elif kind == 1:
                at = [0, p.base_len + 5, p.local_at + p.local_len][rnd.randrange(3)]
                struct.pack_into("<H", b, at, rnd.choice([rnd.randrange(65536), rnd.randrange(2000),
                                                          0x8000 + rnd.randrange(0x8000), 1990 + rnd.randrange(20)]))
            elif kind == 2 and p.jumps:
                at, what, target = rnd.choice(p.jumps)
                for d in range(40):
                    if at + d + 2 <= 2000 and struct.unpack_from("<H", b, at + d)[0] == target:
                        struct.pack_into("<H", b, at + d, rnd.choice([rnd.randrange(65536), rnd.randrange(2000),
                                                                      1995 + rnd.randrange(10), 0, 1]))
                        break
            elif kind == 3:
                k = rnd.randrange(2, p.end)
                if rnd.random() < 0.5:
                    del b[k]
                    b.append(0)
                else:
                    b.insert(k, rnd.randrange(256))
                    del b[-1]
            elif kind == 4:
                struct.pack_into("<H", b, p.end + 1, rnd.choice([rnd.randrange(65536), rnd.randrange(2000),
                                                                 1996 + rnd.randrange(8)]))
            else:
                refs = [at + 1 for at, typ, off in p.locals] + [at + 1 for at, off in p.times]
                if refs:
                    struct.pack_into("<H", b, rnd.choice(refs), rnd.choice([rnd.randrange(65536),
                                                                            0x7FF0 + rnd.randrange(16),
                                                                            rnd.randrange(2000)]))
                else:
                    b[rnd.randrange(2, p.end)] = rnd.randrange(256)
            out.append(bytes(b))
    return out


def run(exe, rows_path, count, seed, scans, layout):
    """Run every row, starting again after any row that takes the process down."""
    results = {}
    first = 0
    while first < count:
        r = subprocess.run([exe, rows_path, str(seed), str(scans), str(layout), str(first)],
                           capture_output=True, text=True)
        for line in r.stdout.splitlines():
            f = line.split()
            if len(f) == 7:
                results[int(f[0])] = dict(rets=f[2], trace=f[3], alarms=int(f[4], 16),
                                          outside=int(f[5]), crashed=int(f[6]), write_fault=False)
        for line in r.stderr.splitlines():
            f = line.split()
            if len(f) == 3 and f[0] == "fault" and f[2] == "write":
                results[int(f[1])]["write_fault"] = True
        done = [i for i in results if i >= first]
        first = max(done) + 1 if done else first
        if first < count and r.returncode != 0 or (first < count and not done):
            results[first] = dict(rets="-", trace="-", alarms=0, outside=0, crashed=2, write_fault=True)
            first += 1
    return results


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--base", default="main")
    ap.add_argument("--mutants", type=int, default=150)
    ap.add_argument("files", nargs="+")
    args = ap.parse_args()

    rows = corpus(args.files)
    if not rows:
        sys.exit("no programs in those files")
    mutants = damage(rows, args.mutants)
    work = tempfile.mkdtemp(prefix="decode_harness_")
    try:
        with open(os.path.join(ROOT, "bacnet", "private", "decode.c"), "rb") as f:
            new_src = f.read()
        with open(os.path.join(ROOT, "bacnet", "private", "BASIC.H"), "rb") as f:     # tracked in capitals
            new_basic = f.read()
        exes = {"base": build("base", git_show(args.base, "bacnet/private/decode.c"),
                              git_show(args.base, "bacnet/private/BASIC.H"), work),
                "new": build("new", new_src, new_basic, work)}
        sets = {"corpus": [r for _, r, _ in rows], "damaged": mutants}
        res = {}
        for name, blobs in sets.items():
            path = os.path.join(work, name + ".bin")
            with open(path, "wb") as f:
                f.write(b"".join(blobs))
            for tag, exe in exes.items():
                for layout in (0, 1, 2):
                    res[name, tag, layout] = run(exe, path, len(blobs), 12345 if name == "corpus" else 999,
                                                 20 if name == "corpus" else 8, layout)
    finally:
        shutil.rmtree(work, ignore_errors=True)

    bad = False
    print("%d programs, %d damaged copies; base %s\n" % (len(rows), len(mutants), args.base))
    for name, blobs in (("corpus", rows), ("damaged", mutants)):
        c = collections.Counter()
        unexplained = []
        for i in range(len(blobs)):
            b0, n0 = res[name, "base", 0][i], res[name, "new", 0][i]
            base_left = any(res[name, "base", l][i]["crashed"] or res[name, "base", l][i]["outside"] for l in (0, 1, 2))
            new_writes = any(res[name, "new", l][i]["write_fault"] or res[name, "new", l][i]["outside"] for l in (0, 1, 2))
            new_reads = any(res[name, "new", l][i]["crashed"] for l in (0, 1, 2)) and not new_writes
            base_writes = any(res[name, "base", l][i]["write_fault"] or res[name, "base", l][i]["outside"] for l in (0, 1, 2))
            abandoned = bool(n0["alarms"] & OUT_OF_BOUNDS)
            same = (b0["rets"], b0["trace"], b0["alarms"]) == (n0["rets"], n0["trace"], n0["alarms"])
            c["base writes outside its row"] += base_writes
            c["base reads outside its row, no writes"] += base_left and not base_writes
            c["new writes outside its row"] += new_writes
            c["new reads outside its row, no writes"] += new_reads
            c["new abandons the scan: out of bounds"] += abandoned
            if same:
                c["behave the same"] += 1
            elif base_left or abandoned:
                c["differ: base left the row, or new abandoned"] += 1
            else:
                c["differ, unexplained"] += 1
                unexplained.append(i)
        print(name)
        for k in ("behave the same", "differ: base left the row, or new abandoned", "differ, unexplained",
                  "base writes outside its row", "base reads outside its row, no writes",
                  "new writes outside its row", "new reads outside its row, no writes",
                  "new abandons the scan: out of bounds"):
            print("  %-46s %5d" % (k, c[k]))
        if name == "corpus":
            for i in range(len(rows)):
                if (res[name, "base", 0][i]["trace"] != res[name, "new", 0][i]["trace"]
                        or res[name, "new", 0][i]["alarms"] & OUT_OF_BOUNDS):
                    print("    differs or abandoned: %s" % rows[i][0])
        if unexplained:
            print("    unexplained: " + " ".join(map(str, unexplained[:30])))
        print()
        bad = bad or c["new writes outside its row"] > 0 or c["differ, unexplained"] > 0
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
