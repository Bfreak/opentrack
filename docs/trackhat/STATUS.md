# Status (as of 2026-11-08)

## Goal
Port Trackhat's custom OpenTrack (sensor v2 / v3, 2023-24) onto the latest OpenTrack release (`opentrack-2026.1.0`)
with the same feature set plus upstream improvements, fork it to the owner's GitHub, and deliver builds the owner can test.
The owner can't compile, so builds/tests are done by Claude.

## Done
- Branch `trackhat-2026.1` = 2026.1.0 tag + real 3-way merge of `trackhat-sensor-v2` + cherry-picked upstream fixes
  (88 commits ahead of the 2026.1.0 tag; HEAD `29cb2940`). Tree clean. See `PORTING-NOTES.md`.
- **Linux**: builds, GUI smoke-tested under Xvfb, settings persist, NN models load, built-in NN unit tests pass.
- **Windows (MinGW-w64 cross-compile, x64)**: builds, packages as a portable zip, GUI opens and update check works under Wine 9.
  Zip delivered to owner: `TrackHat-OpenTrack-2026.1-win64-test.zip` (86 MB, 142 files). Rebuild recipe: `WINDOWS-BUILD.md`.
- Changelog written: `CHANGELOG.md`.

- **Moved to Claude Code on the owner's Windows PC** (2026-11-08): repo at `Documents\TrackHatOpentrackProject\opentrack-trackhat`,
  restored from the handoff bundle. Fork created: https://github.com/Bfreak/opentrack (remote `fork`);
  branch `trackhat-2026.1` and tag `opentrack-2026.1.0` pushed. `origin` is still upstream `opentrack/opentrack`.

## Blocked / pending
1. **Owner testing of the Windows zip with a real sensor**: report back camera detection, exposure presets, tracking, popup behaviour.

## Suggested next steps (in order)
1. ~~Push the branch to the owner's fork.~~ Done 2026-11-08.
2. Add GitHub Actions CI: Linux build + (optional) MSVC Windows build using upstream's own Windows CI as a template
   (`.github/workflows` / `ci/` in the repo). This gives an MSVC build and an Inno Setup installer without the sandbox.
3. Fix the **update popup**: the app's version string is `opentrack-2026.1.0-88-g<hash>`; trackhat.org advertises
   `trackhat-opentrack-3.2`, so every start shows "Software update released". Decide a Trackhat version scheme
   (e.g. `trackhat-opentrack-4.0`), bake it into the version string, and update the website text
   `SOFTWARE-UPDATE-V3: ...` accordingly.
4. Port the Trackhat Inno Setup installer script changes (listed as carried over in notes but not rebuilt/verified for Windows).
5. Bundle VC++ redist dependency or document it (ORT's `onnxruntime.dll` needs msvcp140/vcruntime140).
6. Hardware validation: exposure presets Near/Medium/Far/Custom on a real sensor; compare 0.5-small vs 0.2-small tracking.
7. Optionally consider upstream commits not taken (DBus, V4L2 ioctl workaround, OpenCV 5 fixes) if Linux users matter.

## Task list from the chat (final state)
- Port + merge + Linux build/test: completed
- Fork and push: completed 2026-11-08 (Bfreak/opentrack)
- Windows build: completed (MinGW zip delivered)
