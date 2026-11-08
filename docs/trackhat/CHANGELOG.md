# Changelog: Trackhat OpenTrack 3.2 -> 2026.1 port

The old Trackhat OpenTrack (3.1/3.2, branch `trackhat-sensor-v2`) is rebuilt on OpenTrack 2026.1.0.
Trackhat features are carried over, with upstream's improvements since then.

## What you'll notice
- Default head-pose model is now `head-pose-0.5-small` (was 0.2-small). 0.2-small still ships, so existing profiles keep working.
- Better tracking robustness: auto-centring, reports lost tracking, no pose output before the first detection (replaces Trackhat's own workaround).
- Roll computation uses upstream's fixed version (fixed upstream in 2023 and 2025).
- Lower idle CPU: ONNX thread pool no longer busy-waits between frames.
- Camera start-up: MJPEG mode set in the order upstream found fixes a bug (#2136), incl. a Windows-specific variant.
- Qt 6 build; update checker ported from `QRegExp` to `QRegularExpression`; working Linux build and MinGW Windows build.
- About eight upstream core/tracker fixes after 2026.1.0 cherry-picked.
- Version is Trackhat OpenTrack 3.3. The "Software update released" dialog now only appears when trackhat.org
  advertises a newer version than the one installed, not on every start.

## Unchanged from 3.2
- Compact UI, Trackhat branding/icons, 320x240 preview, 30 Hz UI refresh, Trackhat window title.
- Neural-net tracker and accela filter forced; selectors hidden.
- Fixed 60 deg FOV, fixed 640x480@60 MJPEG; "TrackHat sensor" camera preferred, other cameras selectable by name.
- Exposure presets Near/Medium/Far/Custom; camera settings only enabled for Custom; advanced options hidden.
- Windows 11 camera-bug workaround; no device path appended to DirectShow names; trackhat.org update checker; OSC output.

## Deliberate differences
- Forward head offset stays 200 mm (upstream default now -100); ONNX threads stay 1 (upstream now 2).
- Exposure enumerators renamed `range_near/far/medium` (Windows header macro clash); numeric values unchanged.
- Model picker UI not exposed. Personal todo commits / `todo.md` not carried over.

## Not taken from upstream
DBus interface, Linux V4L2 workaround, OpenCV 5 build fixes, upstream translation updates, other trackers, `accela-hamilton` as default filter.

## Not yet verified
Real sensor capture, exposure presets on hardware, tracking quality with 0.5-small, Windows zip on real Windows (only run under Wine).
