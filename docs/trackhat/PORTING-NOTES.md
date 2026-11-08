# TrackHat fork of OpenTrack: porting notes

Branch `trackhat-2026.1` = **OpenTrack 2026.1.0** + the TrackHat v3 ("sensor v2") feature set
+ a curated set of upstream fixes made after the 2026.1.0 release.

## Where the TrackHat features came from

Upstream `opentrack/opentrack` holds three generations of TrackHat work:

| Generation | Refs | Notes |
|---|---|---|
| Clip & cap (2015-18) | branch `trackhat` | PS3 Eye + point tracker, wizard UI. Not ported. |
| Sensor v1 (2021-22) | `trackhat-camera`, tags `trackhat-opentrack-2.2/2.2b` | `tracker-trackhat`, already upstream. Not ported separately. |
| **Sensor v2 / v3 (2023-24)** | `trackhat-sensor-v2`, tags `trackhat-opentrack-3.1/3.2` | **This is what was ported.** |

The branch was merged (real 3-way merge, history preserved) into the 2026.1.0 release tag.

## Feature set carried over

- Compact UI, TrackHat branding/assets/icons, 320x240 preview, window title, 30 Hz UI refresh
  (`opentrack/defs.hpp` `UI_*` switches, `main-window.*`, `pose-widget`)
- TrackHat-only build: neural-net tracker + accela filter are forced and their selectors hidden
  (`cmake/opentrack-variant.cmake` is where the module list now lives; upstream removed `variant/`)
- Neural-net tracker: fixed 60 degree lens FOV, fixed 640x480@60 MJPEG capture, "TrackHat sensor"
  camera preferred (any other camera selectable by name), exposure presets
  (Near/Medium/Far/Custom), "Camera settings" only enabled for Custom, advanced options hidden
- `video-opencv`: exposure presets + camera property set, device path not appended to DShow names
- Software-update checker against trackhat.org (ported from `QRegExp` to `QRegularExpression`)
- OSC output (needs `-DSDK_OSCPACK=<path>`; identical to upstream)
- Installer script changes

## Deliberate differences from the 2023-24 TrackHat code

- **Neural-net model**: default is now upstream `head-pose-0.5-small.onnx` (was 0.2-small).
  `head-pose-0.2-small.onnx` is still installed so existing profiles keep working.
- **Roll computation, "no pose before first detection", model picker, FPS/resolution code** are now
  upstream's implementations (upstream fixed roll in 2023 and 2025; `tracking_valid_` supersedes
  TrackHat's `optional<Affine>` workaround). The model picker UI is not exposed.
- **Defaults kept at TrackHat's values** where upstream changed them for tuning reasons:
  forward head offset 200 mm (upstream now -100), ONNX threads 1 (upstream now 2).
- `exposure_preset` enumerators renamed `range_near/range_far/range_medium` (the old `near`/`far`
  names are macros in `<windows.h>`). Numeric values are unchanged, so saved profiles stay valid.
- ps3eye submodule and MSVC `sdk-paths-*` files use upstream's versions (not used by the TrackHat build).
- `todo.md` and the personal todo commits were not carried over.

## Upstream fixes after 2026.1.0 that were cherry-picked

Linux/macOS NN build: `f93040e6 9fc56ac8 8497be40 0779d3ce 4efe501b`
Camera: `6e733b5b dfe82d8f` (MJPEG ordering, incl. the Windows-specific variant)
Tracker: `03a0e69b 3adbb90f` (new models), `a9a60fb2` (robustness), `fd8f1039` (auto-centering,
reports lost tracking), `99815e8a` (ONNX thread-pool spinning), `dede44d3`, `13130d1b` (partial)
Core: `095112e3 570ab7af 0dcdded3(partial) 93693018 8c42983c bae736f6 7eb4ab61 095a65a3`

Not taken: DBus interface, Linux V4L2 ioctl workaround, OpenCV 5 build fixes, translations,
other trackers, `accela-hamilton` as default filter.

## Build (Linux, verified)

    sudo apt install ninja-build libproc2-dev libopencv-dev qt6-tools-dev qt6-l10n-tools \
        qt6-serialport-dev qt6-base-private-dev qt6-multimedia-dev libusb-1.0-0-dev \
        libsdl2-dev libevdev-dev liboscpack-dev
    # ONNX Runtime 1.17.3: https://github.com/microsoft/onnxruntime/releases (linux-x64 tgz)
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DONNXRuntime_DIR=<ort dir> -DSDK_OSCPACK=/usr \
        -S . -B ../build && ninja -C ../build install

OpenTrack refuses to run as root.

## Not verified

- **Windows build/installer**: nothing here was compiled with MSVC. The DirectShow code path in
  `compat/camera-names.cpp` was edited by hand.
- Real camera capture, exposure presets on a real TrackHat sensor, and tracking quality of the
  0.5-small model have not been tested (no camera in the test environment).
