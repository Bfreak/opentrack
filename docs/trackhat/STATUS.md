# Status (as of 2026-11-08)

## Goal
Port Trackhat's custom OpenTrack (sensor v2 / v3, 2023-24) onto the latest OpenTrack release (`opentrack-2026.1.0`)
with the same feature set plus upstream improvements, fork it to the owner's GitHub, and deliver builds the owner can test.
The owner can't compile, so builds/tests are done by Claude.

## Done
- Branch `trackhat-2026.1` = 2026.1.0 tag + real 3-way merge of `trackhat-sensor-v2` + cherry-picked upstream fixes
  (88 port commits on the 2026.1.0 tag up to `29cb2940`, then docs/CI commits). See `PORTING-NOTES.md`.
- **Linux**: builds, GUI smoke-tested under Xvfb, settings persist, NN models load, built-in NN unit tests pass.
- **Windows (MinGW-w64 cross-compile, x64)**: builds, packages as a portable zip, GUI opens and update check works under Wine 9.
  Zip delivered to owner: `TrackHat-OpenTrack-2026.1-win64-test.zip` (86 MB, 142 files). Rebuild recipe: `WINDOWS-BUILD.md`.
- Changelog written: `CHANGELOG.md`.
- **Moved to Claude Code on the owner's Windows PC** (2026-11-08): repo at `Documents\TrackHatOpentrackProject\opentrack-trackhat`,
  restored from the handoff bundle. Fork: https://github.com/Bfreak/opentrack (remote `fork`);
  branch `trackhat-2026.1` and tag `opentrack-2026.1.0` pushed. `origin` is still upstream `opentrack/opentrack`.
- **GitHub Actions MSVC build** (2026-11-08): `.github/workflows/trackhat-windows.yml` runs on pushes to `trackhat-*`
  branches/tags, PRs and manually. MSVC 2022 + Qt 6.8.3 (msvc2022_64) + static OpenCV 4.10.0
  (built and cached in the workflow, IPP off) + ORT 1.17.3 (download retried, cached) + oscpack (built with `cl`) + `opentrack-depends` (vJoy/FSUIPC/SimConnect SDKs).
  Outputs two artifacts: Inno Setup installer (`installer/opentrack-installer.iss`, ~85 MB) and a portable zip.
  Bundles the MSVC runtime DLLs app-locally (no VC++ redist install needed) and the Qt TLS/imageformats/styles plugins.
  Upstream's generic `cmake.yml` is now manual-only (it doesn't build the Trackhat variant and failed on every push).
  Latest green run: https://github.com/Bfreak/opentrack/actions/runs/42476892319 (installer
  `opentrack-2026.1.0-97-g0554956c-win32-setup.exe`, 86 MB). Installer is unsigned, so expect a SmartScreen warning.
  Earlier runs failed on flaky GitHub release downloads (Ninja, ORT), which is why nothing is downloaded from release pages uncached.

- **Head-tracking test window** (2026-11-08, branch `trackhat-head-test`): "Test head tracking" button opens a
  3D Cockpit / Race car / Soldier view driven by the mapped pose. See `HEAD-TEST.md`. Built and checked locally
  with screenshots of every scene from six head poses; CI installer:
  https://github.com/Bfreak/opentrack/actions/runs/42499336005. **Not yet tried with live tracking.** Not merged into `trackhat-2026.1`.
- **Local Windows build** on the owner's PC (2026-11-08): `scripts/windows-local/build.bat`, deps in `../deps`. See `WINDOWS-LOCAL-BUILD.md`.

## Blocked / pending
1. **Owner testing of the CI installer on real Windows with a real sensor**: camera detection, exposure presets,
   tracking, update popup. Nothing from the MSVC build has been run yet, not even launched.

## Suggested next steps (in order)
1. ~~Push the branch to the owner's fork.~~ Done 2026-11-08.
2. ~~Add GitHub Actions CI with an MSVC build and Inno Setup installer.~~ Done 2026-11-08 (Windows only; no Linux CI job yet).
3. ~~Fix the update popup.~~ Done 2026-11-08. This release is **trackhat-opentrack-3.3** (owner's choice).
   Version string = `git describe --match 'trackhat-opentrack-*'`; tag `trackhat-opentrack-3.3-rc1` is pushed
   and built: https://github.com/Bfreak/opentrack/actions/runs/42494580279 (artifacts `trackhat-opentrack-3.3-rc1-*`).
   The check now shows the dialog only when the site's version number is higher than the running one
   (`opentrack/software-update-dialog.cpp`). The site can stay at `trackhat-opentrack-3.2` while 3.3 is in testing.
   **When 3.3 ships:** tag the release commit `trackhat-opentrack-3.3`, push the tag (CI builds the installer),
   publish it, then change the website line to `SOFTWARE-UPDATE-V3: trackhat-opentrack-3.3`. Old 3.2 installs
   still use exact string comparison, so they get notified as soon as the line changes.
   The installer name contains "win32" (from the old script) although the build is x64; see item 4.
4. Review the Trackhat Inno Setup script against what is now installed (it builds in CI but its contents are unreviewed).
   It has no `ArchitecturesInstallIn64BitMode`, so the x64 build installs to `Program Files (x86)`; check where 3.2 installed
   before changing it, so an upgrade replaces the old copy instead of leaving two.
5. ~~Bundle VC++ redist~~: done app-locally in CI. Not checked on a clean PC without the redist.
6. Hardware validation: exposure presets Near/Medium/Far/Custom on a real sensor; compare 0.5-small vs 0.2-small tracking.
7. Optionally consider upstream commits not taken (DBus, V4L2 ioctl workaround, OpenCV 5 fixes) if Linux users matter.
8. Cleanup: `variant/default/_variant.cmake` and `todo.md` are still in the tree (re-added by the merge) although the
   notes say they were dropped. Neither is used by the build.

## Task list (current)
- Port + merge + Linux build/test: completed
- Fork and push: completed 2026-11-08 (Bfreak/opentrack)
- Windows build: MinGW zip (Wine-tested) + MSVC installer/zip from CI (untested on Windows)
