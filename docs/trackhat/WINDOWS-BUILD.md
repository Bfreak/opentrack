# Windows build (MinGW-w64 cross-compile from Linux)

Produces a portable x64 Windows folder/zip. This is **not** the upstream MSVC build; for an MSVC build + installer use CI
(see `STATUS.md`). Everything below was done on Ubuntu 24.04 in the Claude sandbox (2 cores; OpenCV was the slowest dependency to build).
The helper files are in `scripts/windows-cross/`.

## Toolchain / dependencies (versions that worked)
| Item | Version / source | Notes |
|---|---|---|
| Compiler | Ubuntu `g++-mingw-w64-x86-64-posix` 13 (GCC, msvcrt, POSIX threads) | use the `-posix` variants (`x86_64-w64-mingw32-g++-posix`) |
| Qt | 6.8.3 via `aqtinstall`: Windows kit `win64_mingw` **and** the Linux host kit `linux_x64`/`gcc_64` | host Qt supplies moc/uic/rcc via `QT_HOST_PATH` |
| OpenCV | 4.10.0 (clone `opencv/opencv`, tag 4.10.0), built for MinGW | modules core, imgproc, imgcodecs, videoio (+ calib3d,features2d,flann,video,highgui); `BUILD_SHARED_LIBS=ON`, `WITH_DSHOW=ON`, MSMF off, no tests/perf tests, `BUILD_ZLIB/PNG/JPEG=ON` |
| ONNX Runtime | 1.17.3 `onnxruntime-win-x64` prebuilt (MSVC DLL) | headers used by MinGW; DLL loaded at runtime |
| oscpack | `RossBencina/oscpack` (commit 3e7e3a1), hand-built static `liboscpack.a` | compiled `osc/*.cpp` + `ip/IpEndpointName.cpp`, `ip/win32/NetworkingUtils.cpp`, `ip/win32/UdpSocket.cpp` with `-DOSC_HOST_LITTLE_ENDIAN=1`; headers copied to `include/oscpack/{ip,osc}` |

The exact aqtinstall / OpenCV / oscpack commands were not saved verbatim; the settings above are read back from the
CMake caches, so treat `scripts/windows-cross/build-windows.sh` as a faithful reconstruction.

## OpenTrack configure (recorded cache values)
    cmake -G Ninja -S <repo> -B build-win \
      -DCMAKE_TOOLCHAIN_FILE=scripts/windows-cross/mingw-toolchain.cmake \
      -DCMAKE_BUILD_TYPE=RELEASE \
      -DQT_HOST_PATH=$WORK/qt/6.8.3/gcc_64 -DCMAKE_PREFIX_PATH=$WORK/qt/6.8.3/mingw_64 \
      -DOpenCV_DIR=$WORK/cv-win/install/lib/cmake/opencv4 \
      -DONNXRuntime_DIR=$WORK/ort-win/onnxruntime-win-x64-1.17.3 \
      -DSDK_OSCPACK=$WORK/oscpack-win \
      -D__otr_compile_flags_set=TRUE \
      -DCMAKE_DISABLE_FIND_PACKAGE_WrapVulkanHeaders=TRUE -DVulkan_INCLUDE_DIR=NOTFOUND \
      -DCMAKE_C_FLAGS="-D_stdcall=__stdcall -D_cdecl=__cdecl" \
      -DCMAKE_CXX_FLAGS="-DOSC_HOST_LITTLE_ENDIAN=1 -D_stdcall=__stdcall -D_cdecl=__cdecl -include $PWD/scripts/windows-cross/sal-compat.h"
    ninja -j2 -C build-win
    cmake --install build-win --prefix install-win

## Packaging steps done after `cmake --install`
- Copy MinGW runtime DLLs next to `opentrack.exe` **and** into `modules/`: `libgcc_s_seh-1.dll`, `libstdc++-6.dll`,
  `libwinpthread-1.dll`, `libgomp-1.dll` (from `/usr/lib/gcc/x86_64-w64-mingw32/13-posix/` and `/usr/x86_64-w64-mingw32/lib/`).
- Copy Qt plugins: `tls/qschannelbackend.dll` (needed for HTTPS update check), `imageformats/{qico,qjpeg,qgif}.dll`,
  `styles/qmodernwindowsstyle.dll`. (`qsvg` needs Qt6Svg.dll; left out.) `platforms/qwindows.dll` is installed by the build.
- Check no unexpected DLL deps remain:
  `x86_64-w64-mingw32-objdump -p <file> | grep "DLL Name"` for exe + all dlls; everything left should be Windows system DLLs or
  `msvcp140/vcruntime140` (ORT, needs VC++ redistributable on the target PC).
- Add `README-TEST.txt`, zip the folder.

## Gotchas hit (in order) and fixes
1. `-march=native` forced by upstream CMake -> define `__otr_compile_flags_set` (and the toolchain sets no arch flags).
2. `/usr/include` leaking into the Windows build via Qt's `WrapVulkanHeaders` -> disable it (flags above).
3. `_stdcall` / `_cdecl` unknown under `-std=c++20` strict mode -> `-D_stdcall=__stdcall -D_cdecl=__cdecl`.
4. ORT headers use MSVC SAL macros (`_Frees_ptr_opt_` etc.) missing in MinGW -> `-include sal-compat.h`.
5. `Ort::Session` constructor wants wide paths on Windows; `convert()` was guarded by `_MSC_VER` -> now `_WIN32` (commit `29cb2940`).
6. `<windows.h>` `near`/`far` macros clash with the exposure enum -> renamed enumerators.
7. `rm -rf *` inside the OpenCV install dir was blocked by the sandbox safety check -> used a fresh build dir (`cv-build`); don't work around such blocks.
8. GitHub archive downloads (OpenCV) get 403 from the sandbox proxy -> attach the repo (`add_repo`, read) and `git clone`.

## Smoke test under Wine (what was verified)
    Xvfb :99 -screen 0 1280x800x24 &
    WINEPREFIX=/tmp/wineprefix DISPLAY=:99 wine install-win/opentrack.exe
Main window rendered correctly, profile dir `Documents/trackhat-v3` created, update check reached trackhat.org (TLS via schannel) and showed the update dialog.
Wine cannot test cameras or the neural-net run path; the Options/tracker dialog was not exercised.

## Known limitations
- MinGW (GCC) ABI: `onnxruntime.dll` is an MSVC DLL called through its C API only (fine). Don't link C++ MSVC libraries.
- MSMF capture is not built into the OpenCV; DirectShow is used (matches the original Trackhat behaviour).
- No Inno Setup installer. `freetrackclient*.dll`, `NPClient*.dll`, `TrackIR.exe` were taken from upstream's prebuilt install tree.
