"""Check the linker map against the stack the startup code assumes.

    python tools/checkmap.py                 check the Tstat10 map
    python tools/checkmap.py path/to.map

Run this before handing anyone a hex. It exists because of a device that would
not boot.

arm/CORE/startup_stm32f10x_hd.s builds with DATA_IN_ExtSRAM EQU 1, and on that
branch it does not use the stack the linker placed. It writes an absolute
address instead:

    __initial_sp EQU 0X20000000 + Stack_Size    ; 0x20002000

Nothing then references Stack_Mem, so the linker discards the section --
"Removing startup_stm32f10x_hd.o(STACK)" appears in the map -- and no longer
believes anything occupies internal SRAM. The stack is real, 8 KB of it growing
down from 0x20002000, but it is invisible to the tool deciding where to put
everything else.

For years that was harmless, because RW_RAM1 was declared as 8 MB against a
512 KB part and swallowed every byte of RW and ZI, leaving internal SRAM empty by
accident. Declaring RW_RAM1 at its real size ends the accident: the linker starts
filling from 0x20000000 straight through the stack, and __main's ZI sweep then
zeroes live stack frames before main() is reached. The device faults before it
runs a single line of application code, and stays in the bootloader.

So the invariant this file checks is: nothing the linker places may sit below
__initial_sp. It also reports how full each region is, since the whole reason the
sizes were corrected is that an overflow of a mis-declared region corrupts at run
time instead of failing the link.
"""
import re
import sys

DEFAULT = 'arm/OBJ/Tstat10_arm_revxx.map'

REGION = re.compile(r'Execution Region (\w+) \(Exec base: (0x[0-9a-f]+), '
                    r'Load base: 0x[0-9a-f]+, Size: (0x[0-9a-f]+), Max: (0x[0-9a-f]+)')
# a placement line: address, an optional COMPRESSED, the size, then the object
SYMBOL = re.compile(r'^\s+(0x[0-9a-f]{8})\s+(?:COMPRESSED\s+)?0x[0-9a-f]+\s+\w+'
                    r'\s+\w+\s+\d+\s+\S+\s+(\S+)', re.M)


def check(path):
    text = open(path, encoding='utf-8', errors='replace').read()
    bad = []

    m = re.search(r'__initial_sp\s+(0x[0-9a-f]+)', text)
    if not m:
        print('no __initial_sp in %s -- is this a Keil map?' % path)
        return 1
    sp = int(m.group(1), 16)
    print('__initial_sp  0x%08X' % sp)

    if 'Removing startup_stm32f10x_hd.o(STACK)' in text:
        print('              the STACK section was discarded, so the linker does not'
              ' know the stack exists')

    print()
    for name, base, size, mx in REGION.findall(text):
        base, size, mx = int(base, 16), int(size, 16), int(mx, 16)
        pct = 100.0 * size / mx if mx else 0.0
        note = ''
        # the cheap check that would have caught the device that would not boot:
        # a RAM region based below the stack top will be filled through the stack
        if 0x20000000 <= base < sp and size:
            note = '  <-- BASED BELOW THE STACK'
            bad.append('%s is based at 0x%08X, below __initial_sp' % (name, base))
        elif pct >= 95.0:
            note = '  <-- over 95 percent full'
            bad.append('%s is %.1f%% full' % (name, pct))
        print('%-10s 0x%08X  %7d of %7d  %5.1f%%%s' % (name, base, size, mx, pct, note))

    under = sorted(set((int(a, 16), obj) for a, obj in SYMBOL.findall(text)
                       if 0x20000000 <= int(a, 16) < sp))
    print()
    if under:
        bad.append('%d symbols are linked below the stack top' % len(under))
        print('LINKED BELOW THE STACK -- this image will not boot:')
        for a, n in under[:10]:
            print('   0x%08X  %s' % (a, n))
        if len(under) > 10:
            print('   ... and %d more' % (len(under) - 10))
    else:
        print('nothing is linked below the stack top')

    print()
    if bad:
        print('FAIL: ' + '; '.join(bad))
        return 1
    print('PASS: safe to flash as far as the map can tell')
    return 0


if __name__ == '__main__':
    sys.exit(check(sys.argv[1] if len(sys.argv) > 1 else DEFAULT))
