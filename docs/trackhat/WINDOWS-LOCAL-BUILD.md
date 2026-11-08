# Local Windows build (owner's PC)

The repo lives in `Documents\TrackHatOpentrackProject\opentrack-trackhat`. Its dependencies sit next to it in `..\deps`
(about 3 GB, installed 2026-11-08, nothing system-wide):

| Dependency | Where | Source |
|---|---|---|
| MSVC 14.44 | Visual Studio 2022 Build Tools (already installed) | same toolset as CI |
| Qt 6.8.3 msvc2022_64 (+qtimageformats, qtshadertools) | `deps\qt\6.8.3\msvc2022_64` | `deps\venv\Scripts\aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -O deps\qt -m qtimageformats qtshadertools` |
| OpenCV 4.10.0 (official prebuilt, `opencv_world`) | `deps\opencv\build` | `opencv-4.10.0-windows.exe` from the GitHub release, sources folder deleted |
| ONNX Runtime 1.17.3 | `deps\onnxruntime-win-x64-1.17.3` | GitHub release zip |
| Python venv (aqtinstall, Pillow) | `deps\venv` | for Qt installs and `head-test/tools/fetch_assets.py` |

Build (about 3 minutes from scratch; output in `..\build-local\install`):

    scripts\windows-local\build.bat              (incremental)
    scripts\windows-local\build.bat reconfigure  (re-run CMake)

Differences from the CI build: OpenCV is the shared prebuilt (`opencv_world4100.dll`) instead of static, there is no oscpack
(so no OSC output), and the MSVC runtime isn't bundled (Build Tools provide it). Use CI for anything shipped to users.
