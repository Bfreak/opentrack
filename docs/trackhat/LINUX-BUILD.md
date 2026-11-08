# Linux build and smoke test (verified)

    sudo apt install ninja-build libproc2-dev libopencv-dev qt6-tools-dev qt6-l10n-tools \
        qt6-serialport-dev qt6-base-private-dev qt6-multimedia-dev libusb-1.0-0-dev \
        libsdl2-dev libevdev-dev liboscpack-dev
    # ONNX Runtime 1.17.3 linux-x64 tgz from https://github.com/microsoft/onnxruntime/releases -> $ORT
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DONNXRuntime_DIR=$ORT -DSDK_OSCPACK=/usr -S . -B ../build
    ninja -C ../build install        # configure with -DCMAKE_INSTALL_PREFIX=... to choose the install dir

Run as a **non-root** user. Headless GUI smoke test:

    Xvfb :99 -screen 0 1280x800x24 &
    DISPLAY=:99 <install>/bin/opentrack      # screenshot with `import -window root shot.png`

Verified: window opens in Trackhat compact mode, settings persist (`~/.config/trackhat-v3`), NN models load,
upstream NN unit tests pass. Not verified: real camera.
Release-build gotcha already fixed on the branch: `env.cpp` QFunctionPointer->void* error (upstream fix `f93040e6` and friends).
