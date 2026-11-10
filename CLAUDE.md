# CLAUDE.md: Trackhat OpenTrack (2026.1 port)

This repo is **OpenTrack 2026.1.0 + the Trackhat "sensor v2 / v3" feature set** (branch `trackhat-2026.1`).
It replaces the old Trackhat build (`trackhat-sensor-v2`, tags `trackhat-opentrack-3.1/3.2`, 2023-24).
The owner (James, trackhat.org) is not a C++ developer: **you do the compiling, building and testing**
and hand back something runnable. Prefer finished, tested artifacts over explanations.

## Start here
1. `docs/trackhat/STATUS.md`: what is done, what is blocked, what to do next. **Read first.**
2. `docs/trackhat/PORTING-NOTES.md`: what Trackhat features were carried over, deliberate differences,
   cherry-picked upstream commits. (Same file as `TRACKHAT-NOTES.md` in the branch.)
3. `docs/trackhat/WINDOWS-BUILD.md`: cross-compiling the Windows build from Linux (MinGW), incl. every gotcha hit.
4. `docs/trackhat/LINUX-BUILD.md`: Linux build + headless smoke test.
5. `docs/trackhat/CHANGELOG.md`: user-facing changelog, old Trackhat 3.2 -> this port.
6. `docs/trackhat/GITHUB-AND-ENVIRONMENT.md`: GitHub/fork situation and sandbox constraints.

## What makes this a "Trackhat" build (don't undo)
- `cmake/opentrack-variant.cmake`: Trackhat-only module list, ident `trackhat-v3`
  (settings dir: `~/.config/trackhat-v3` on Linux, `Documents\trackhat-v3` on Windows).
  Upstream removed the old `variant/` mechanism; this file replaces it.
- `opentrack/defs.hpp`: `UI_FORCED_TRACKER "neuralnet"`, `UI_FORCED_FILTER "accela"`, `UI_NO_*`, `UI_COMPACT_VIDEO_FEED`.
- `tracker-neuralnet/`: fixed 60 deg FOV, fixed 640x480@60 MJPEG, prefers camera named "TrackHat sensor",
  exposure presets (Near/Medium/Far/Custom), advanced options hidden, defaults: forward offset 200 mm,
  1 ONNX thread, model `head-pose-0.5-small.onnx` (0.2-small still shipped).
- `video-opencv/`: exposure presets, DirectShow camera settings; `compat/camera-names.cpp`: no device path appended to DShow names.
- `opentrack/software-update-dialog.cpp`: checks trackhat.org for `SOFTWARE-UPDATE-V3: <version>`.
- `logic/multi-protocol.*`: output goes to **all enabled protocols at once** (default: all except mouse emulation);
  managed in Options > Protocols (`gui/options-dialog.cpp`). There is no protocol dropdown on the main window.
- `gui/themes/midnight.qss` (default theme), `pose-widget/` (mannequin head, crosshair, reticle, brand font),
  `head-test/` (3D test window). See `docs/trackhat/STATUS.md` and `HEAD-TEST.md`.
- `exposure_preset` enumerators are `range_near/range_far/range_medium` (NOT `near`/`far`: `<windows.h>` macros). Keep numeric values; profiles depend on them.

## Hard rules / gotchas
- OpenTrack **refuses to run as root**. Run it as a normal user (the sandbox used user `otr`).
- Upstream's top-level CMake forces `-march=native` for GCC unless `__otr_compile_flags_set` is defined.
  **Cross builds must pass `-D__otr_compile_flags_set=TRUE`** and set their own flags.
- ONNX Runtime must be **1.17.3**. On Windows it is loaded dynamically (`ORT_API_MANUAL_INIT` + QLibrary); on Linux/macOS it is linked.
- Windows MinGW builds need: `-D_stdcall=__stdcall -D_cdecl=__cdecl` and `-include scripts/windows-cross/sal-compat.h`
  (ORT headers use MSVC SAL annotations). `WrapVulkanHeaders` must be disabled to stop `/usr/include` leaking in.
- `tracker-neuralnet` model path helper `convert()` must return `std::wstring` on **all** `_WIN32` builds (not just `_MSC_VER`).
- Never `git checkout --theirs/--ours` blindly on header files while merging; it discarded edits once.
- Don't route around permission/policy denials (GitHub proxy, sandbox network allowlist). Report them and ask.

## Not verified yet (be honest about this in reports)
- Nothing has run on **real Windows** or with a **real Trackhat sensor**. Windows build only smoke-tested under Wine (GUI opens, update check works).
- Exposure presets on real hardware, and tracking quality of the 0.5-small model, are untested.
- The MSVC build and Inno Setup installer from CI (`.github/workflows/trackhat-windows.yml`) compile and package,
  but haven't been launched on real Windows yet.

## Building on this PC
- `scripts/windows-local/build.bat` builds and installs to `../build-local/install` (deps in `../deps`,
  see `docs/trackhat/WINDOWS-LOCAL-BUILD.md`). Use it to iterate; use CI for what users get.
- Head-tracking test window: `docs/trackhat/HEAD-TEST.md`. To check scenes visually, run with
  `OPENTRACK_HEAD_TEST_SNAPSHOT=<dir>` and look at the PNGs.

## Windows builds now come from CI
- Push to `fork` (https://github.com/Bfreak/opentrack), branch `trackhat-2026.1`. The workflow
  `Trackhat Windows (MSVC)` uploads the installer and a portable zip as artifacts
  (`gh run list -R Bfreak/opentrack`, `gh run download <id> -R Bfreak/opentrack`).
- This machine is the owner's Windows 11 PC. It has `gh` signed in as `Bfreak`, VS 2022 Build Tools
  (MSVC 14.44), an RTX 3090, and the local deps described above.

## Conventions
- Commit messages: imperative, scoped like upstream (`tracker-neuralnet: ...`, `video/opencv: ...`).
- Keep Trackhat-specific changes isolated and listed in `docs/trackhat/PORTING-NOTES.md`; prefer upstream's implementation when behaviour is equivalent.
- Update `docs/trackhat/STATUS.md` whenever state changes.
