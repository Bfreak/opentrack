# Status (as of 2026-11-12)

Read this first, then `CLAUDE.md`. Everything below is the true state at the end of the last session.

## Goal
Trackhat's custom OpenTrack, rebuilt on OpenTrack 2026.1.0 and released as **Trackhat OpenTrack 3.3**.
The owner (James) is not a C++ developer: Claude builds, tests and hands back something runnable.

## Where things are
- **Repo:** `Documents\TrackHatOpentrackProject\opentrack-trackhat` on the owner's Windows 11 PC.
  Remote `fork` = https://github.com/Bfreak/opentrack (push here). Remote `origin` = upstream opentrack (never push).
  Never open a PR with `gh pr create` without `--repo Bfreak/opentrack`: the default base would be upstream.
- **Branch:** `trackhat-2026.1` is the release branch and holds everything. `trackhat-head-test` and `trackhat-ui-theme`
  are fully merged into it (fast-forward) and can be deleted, locally and on the fork.
- **Release:** tag `trackhat-opentrack-3.3` at `abb0eb8e`. Installer `trackhat-opentrack-3.3-win64-setup.exe`
  (165 MB, sha256 starts `25d9399d`) is in `Documents\TrackHatOpentrackProject\release\` and in the artifacts of
  https://github.com/Bfreak/opentrack/actions/runs/42810744050. Claude tagged it final without asking first, so the
  installer carries the release name; the owner hasn't confirmed 3.3 as final.
- **Not published.** trackhat.org still advertises `trackhat-opentrack-3.2`. To publish: upload the installer, then set
  the website line to `SOFTWARE-UPDATE-V3: trackhat-opentrack-3.3` (3.2 installs are notified as soon as it changes).
- **Local build:** `scripts\windows-local\build.bat` → `..\build-local\install\opentrack.exe` (deps in `..\deps`,
  see `WINDOWS-LOCAL-BUILD.md`). It has no vJoy and no OSC; those only build in CI.
- **CI:** `.github/workflows/trackhat-windows.yml` builds on every push to `trackhat-*` branches and tags
  (about 10 minutes, installer + portable zip as artifacts). All runs up to `b25f1277` are green.
- **Workflow:** from 2026-11-12 the owner works in **VS Code** (Claude Code extension). Tasks are in `.vscode/tasks.json`
  (Terminal > Run Task): build, run, UI self-test, scene screenshots, CI status.

## Open requests from the owner (2026-11-11) - NOT STARTED
The owner listed these as bugs/issues, adding that some may be misunderstandings of the current configuration.
The request was interrupted before any work or investigation, so check current behaviour first.
1. **Camera should change immediately** when selected in the tracker's camera list, even while tracking is running.
   Start at `tracker-neuralnet/ftnoir_tracker_neuralnet.cpp`: `settings_.camera_name` is read when the camera is opened
   (around line 624) and tied to `ui_.cameraName` (around line 816).
2. **A detected TrackHat sensor should always be the default camera.** Today an empty name or "TrackHat sensor" selects it
   (same file, around line 627); check what happens when the sensor is absent, or present under another name or index.
3. **New exposure preset "Auto", as the default**, which sets the camera's exposure to automatic.
   Presets are `exposure_preset` in `video-opencv/settings.hpp` (keep the existing numeric values: profiles depend on them),
   applied in `video-opencv/impl-camera.cpp` `cam::set_exposure`, listed in the tracker dialog (around line 826).
   Enumerator names must not be `near`/`far` (Windows macros).
4. **Pose preview:** make the head and the reticle about **60% of their current size**, and **80% opaque**.
   In `pose-widget/pose-widget.cpp` `paintEvent`: `unit = height() * .21f`, reticle radius `unit * 1.50f`; the head layer is
   drawn with `p.drawImage(rect(), layer)` and the reticle in `draw_reticle`.

After these: rebuild, check with the self-test, then decide with the owner whether to move the `3.3` tag or tag `3.3.1`.

## What 3.3 contains (all merged)
- **Port:** 2026.1.0 + 3-way merge of `trackhat-sensor-v2` + cherry-picked upstream fixes. See `PORTING-NOTES.md`.
- **Update check:** version comes from `trackhat-opentrack-*` tags; the dialog only appears when trackhat.org advertises a
  higher version (`opentrack/software-update-dialog.cpp`). Pre-releases (`-rcN`) are offered the matching final.
- **Head-tracking test window** (`head-test/`, `HEAD-TEST.md`): Cockpit (FlightGear C172P, GPL-2.0+), Race car
  (Khronos CarConcept, CC-BY 4.0), Soldier (Poly Haven CC0 props); PBR renderer with shadows; 77 MB of assets.
- **Midnight theme** (`gui/themes/midnight.qss`, default; `OPENTRACK_THEME=none` for the old look). Chosen by the owner
  from three prototypes; the other two were removed.
- **Main window:** Profile is a full-width row with a "+" button for new profiles; no Output box.
- **Output to all protocols at once** (`logic/multi-protocol.*`): default is everything except mouse emulation, stored per
  profile as `protocol-enabled-<module>` in the `modules` section (the old `protocol-dll` key is ignored). A protocol
  that can't start is skipped and reported, never blocking tracking. vJoy's "driver problem" message box was removed.
- **Options > Protocols** (third tab, `gui/options-dialog.cpp`): checkbox, status and Settings per protocol; a warning
  before mouse emulation is enabled, listing missing pause/stop shortcuts. Changes apply at the next tracking start.
- **Pose preview** (`pose-widget/`): vector perspective grid; mannequin head without a neck; 3D crosshair hidden where the
  head covers it; reticle ring with two markers circling every 8 s, which stop and turn red when paused
  (`pipeline::is_enabled()`); when stopped, "tracking stopped" in Kinishinai NBP (CC BY-SA 3.0, `pose-widget/fonts`).
- **Installer:** Inno Setup, MSVC runtime bundled app-locally, Qt TLS/image/style plugins added, named `<version>-win64-setup.exe`.

## Verified, and how
- Local build on the owner's PC (RTX 3090, Windows 11), reviewed through screenshots:
  - `OPENTRACK_UI_SNAPSHOT=<dir>` saves the main, Options, Protocols, Mapping and test windows, the mouse warning, and the
    pose preview in every state (`head-*.png` at real size, `big-*.png` enlarged), then quits.
  - Add `OPENTRACK_UI_SNAPSHOT_START=1` to start tracking for 3 s first (needs the sensor).
  - `OPENTRACK_HEAD_TEST_SNAPSHOT=<dir>` renders each test scene from six poses and writes frame times to `log.txt`.
- 2026-11-10 self-test with the real sensor: tracking started, the head followed the live pose; FlightGear, freetrack
  and UDP "Active", SimConnect "Not available: Install FSX/Prepar3D SimConnect SDK", mouse "Off"; the warning appeared.
- The owner ran the first version of the test window with live tracking: "works beautifully" (before the scene upgrade).

## Not verified (say so in reports)
- The **3.3 installer has not been run** by anyone, on any PC. Nothing was tested on a clean PC without the VC++ redist.
- No **game** has been tried with several protocols on at once. vJoy and OSC only compile (CI); never run.
- The red paused markers were only seen in a rendered frame, not while pausing live.
- The upgraded test scenes, the Midnight theme and the final head have not been confirmed by the owner in the running app.
- Exposure presets on real hardware; tracking quality of `head-pose-0.5-small` vs `0.2-small`.
- Performance of the test window on integrated graphics; machines without OpenGL 3.3; the Linux build of the new modules.

## Known rough edges (noticed, not fixed)
- Pose preview, front view: the eye sockets read as one dark band across the face.
- While tracking, the "Preview" checkbox sits near the reticle's lower marker.
- Protocol names in the tab come from the modules as they are ("mouse emulation", "freetrack").
- The installer has no `ArchitecturesInstallIn64BitMode`, so the 64-bit build installs to `Program Files (x86)`. Left alone
  because 3.2 used the same script; confirm where 3.2 installs before changing it, so an upgrade replaces the old copy.
- The installer is unsigned (SmartScreen warning).
- `variant/default/_variant.cmake` and `todo.md` are still in the tree (unused; the notes say they were dropped).
- GitHub downloads and pushes were flaky at times ("Could not resolve host"); retrying worked.

## Later ideas
- Linux CI job; upstream commits not taken (DBus, V4L2 ioctl workaround, OpenCV 5 fixes).
- Apply protocol changes without restarting tracking.

## History
- 2026-11-08: moved from a cloud chat to Claude Code on this PC; fork created; MSVC CI; update-check fix; `3.3-rc1`;
  test window built and upgraded; Midnight theme chosen.
- 2026-11-10: all-protocols output, Protocols tab, new pose preview; merged; tagged `3.3`; installer built.
- 2026-11-11: owner listed four fixes (above); interrupted before work started.
- 2026-11-12: status rewritten; VS Code workflow set up.
