#!/usr/bin/env bash
# Reconstructed Windows (MinGW) cross-build for Trackhat OpenTrack. See docs/trackhat/WINDOWS-BUILD.md.
# Assumes deps are already built under $WORK (qt, cv-win/install, oscpack-win, ort-win). Not run end-to-end as a script.
set -euo pipefail
REPO=${REPO:-$PWD}
WORK=${WORK:-$HOME/work}
HERE=$(cd "$(dirname "$0")" && pwd)
# mingw-toolchain.cmake has hardcoded /home/claude/work paths in CMAKE_FIND_ROOT_PATH; sed them to $WORK:
TC=$WORK/mingw-toolchain.cmake
sed "s#/home/claude/work#$WORK#g" "$HERE/mingw-toolchain.cmake" > "$TC"
cmake -G Ninja -S "$REPO" -B "$WORK/build-win" \
  -DCMAKE_TOOLCHAIN_FILE="$TC" -DCMAKE_BUILD_TYPE=RELEASE \
  -DQT_HOST_PATH="$WORK/qt/6.8.3/gcc_64" -DCMAKE_PREFIX_PATH="$WORK/qt/6.8.3/mingw_64" \
  -DOpenCV_DIR="$WORK/cv-win/install/lib/cmake/opencv4" \
  -DONNXRuntime_DIR="$WORK/ort-win/onnxruntime-win-x64-1.17.3" \
  -DSDK_OSCPACK="$WORK/oscpack-win" -D__otr_compile_flags_set=TRUE \
  -DCMAKE_DISABLE_FIND_PACKAGE_WrapVulkanHeaders=TRUE -DVulkan_INCLUDE_DIR=NOTFOUND \
  -DCMAKE_C_FLAGS="-D_stdcall=__stdcall -D_cdecl=__cdecl" \
  -DCMAKE_CXX_FLAGS="-DOSC_HOST_LITTLE_ENDIAN=1 -D_stdcall=__stdcall -D_cdecl=__cdecl -include $HERE/sal-compat.h"
ninja -C "$WORK/build-win" -j"$(nproc)"
rm -rf "$WORK/install-win"
cmake --install "$WORK/build-win" --prefix "$WORK/install-win"
G=/usr/lib/gcc/x86_64-w64-mingw32/13-posix
for l in libgcc_s_seh-1 libstdc++-6 libgomp-1; do cp "$G/$l.dll" "$WORK/install-win/"; cp "$G/$l.dll" "$WORK/install-win/modules/"; done
cp /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll "$WORK/install-win/"; cp /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll "$WORK/install-win/modules/"
Q=$WORK/qt/6.8.3/mingw_64/plugins
mkdir -p "$WORK/install-win"/{tls,imageformats,styles}
cp "$Q"/tls/qschannelbackend.dll "$WORK/install-win/tls/"
cp "$Q"/imageformats/{qico,qjpeg,qgif}.dll "$WORK/install-win/imageformats/"
cp "$Q"/styles/qmodernwindowsstyle.dll "$WORK/install-win/styles/"
echo "Done: $WORK/install-win  (zip it, add README-TEST.txt)"
