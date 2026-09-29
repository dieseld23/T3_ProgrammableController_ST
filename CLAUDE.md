# Start here

Firmware for Temco's STM32 T3 controllers, worked on as a fork. The work targets
the **T3-OEM** (`Tstat10_wifi` target, `Modbus.mini_type == MINI_T10P`), which is
the unit on the bench. `README.md` is the full record: what changed and why,
every image, the bench checks and the to-do list. Read its Status and To do
before starting anything.

## Where things stand

Update this section at the end of each piece of work.

- As of 2026-09-29, `main` holds everything through PR #22. PRs are merged by Dan.
- The newest image is `arm/OBJ/Tstat10_arm_rev68VPF15.hex`, and it carries
  everything. `rev68VPF4` is the newest one confirmed on the unit.
- `rev68VPF5` to `rev68VPF15` have not been flashed. Their checks are open in the
  README's *On the bench*, and one flash of `rev68VPF15` covers them all.
- Waiting on Dan: flashing `rev68VPF15`. The one open decision is whether `ON`
  should jump for a whole selector (README To do 1 lists what that needs). On
  2026-09-29 Dan chose to fix only its fall-through (done in `rev68VPF15`).
  The download-time check is parked.
- In progress: the top-area centring `todo:`, which Dan started on 2026-09-29
  with the Fixed-2 layout (centred as if two digits wide), T3-OEM and °F/°C
  only. Mockups and measurements were made in a scratch copy of
  `tools/screenshot.py`.
- Waiting on test hardware: remote-panel expiry and the MS/TP scan's
  uninitialised count need an MS/TP network or a second panel.
- Items Dan flags with `todo:` live in Claude's project memory (`todo-list.md`),
  not in the repo. Don't start one until asked.

## Rules

- **Fork only.** Push, branch and PR on `dieseld23/T3_ProgrammableController_ST`.
  Never touch `temcocontrols` in any way. `gh pr create` in a fork clone defaults
  to the parent repo, so always pass `--repo dieseld23/T3_ProgrammableController_ST --base main`.
- **T3-OEM only.** Gate display and key changes on `ARROW_KEYS()` / `MINI_T10P`.
  Tstat10 behaviour, `mini_arm` and `CM5_arm` are out of scope unless asked.
- **The memory map is load-bearing.** `RW_IRAM1` stays based at `0x20002000`,
  above the 8 KB stack the linker cannot see. Run `tools/checkmap.py` before
  handing anyone a hex. The README's *Building* section has the history.

## Making a change

1. Branch from `main`, or from the open PR it depends on, and say so in the PR.
2. Build with a full rebuild:
   `"/c/Keil_v5/UV4/UV4.exe" -r "C:\Users\Dan\VsCode\T3-STM\arm\USER\Tstat10_wifi.uvprojx" -o <log>`.
   Expect 0 errors.
3. Check the build:
   - Compare warnings with the last commit's build log, since the build log is
     tracked. Strip tags from both with `sed 's/<[^>]*>//g'`, keep the `warning`
     lines, drop line numbers, and `comm` them.
   - Run `python tools/checkmap.py`.
   - Check stack depths in `arm/OBJ/Tstat10_arm_revxx.htm` ("Max Depth"). Task
     stacks are in words.
4. Interpreter changes (`bacnet/private/decode.c`) also get:
   - `python tools/check_programs.py FILES`;
   - `python tools/decode_harness/run.py --base main FILES`.

   FILES are T3000 `.prog` files. Where the real ones are is in project memory
   (`control-basic-corpus.md`).
5. Commit the build outputs with the source they came from: `arm/OBJ/Tstat10_arm_revxx.*`
   (`.axf`, `.build_log.htm`, `.hex`, `.htm`, `.map`) and `Tstat10_wifi_STM32F103.dep`.
   Don't commit the drift UV4 writes into `Tstat10_wifi.uvprojx`.
6. Anything meant for the unit gets the next image name:
   - copy the hex to `Tstat10_arm_rev68VPF<n>.hex`;
   - take its md5 on this Windows checkout (hex files are CRLF here);
   - add a row to the README Status table, marked as `main`;
   - add a check to *On the bench*.
7. Make two commits: the change with its build outputs, then the image and the
   README. Open a PR on the fork, and give the order for stacked PRs.

## Things that bite

- **Line endings.** C sources and `README.md` are CRLF in the working tree
  (`core.autocrlf=true`), while the blobs are LF, so `git show HEAD:file` gives
  LF. For scripted edits, read the bytes, convert, write back, and check that the
  CRLF and LF counts match.
- **Writing `\n` from the Bash tool.** In this environment a `\\n` inside a Bash
  heredoc can arrive as a real newline. Write scripts that emit C string escapes
  with the Write tool.
- `bacnet/private/BASIC.H` is tracked in capitals.
- Structs are byte-packed (`#pragma pack(1)` with no pop). Probe sizes with a
  temporary `typedef char probe[(sizeof(T)==N)?1:-1];`; never work them out by hand.
- The BACnet stack is a prebuilt `.lib`, so size edits in `bacnet.h` change
  nothing in the image.
- About 15 s of backlight blinking at power-on is the bootloader's ISP window,
  not a fault. There is no bootloader source in the repo.
