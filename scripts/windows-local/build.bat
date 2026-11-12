@echo off
rem Local MSVC build of Trackhat OpenTrack on the owner's Windows PC.
rem Dependencies live next to the repo in ..\deps (see docs/trackhat/WINDOWS-LOCAL-BUILD.md):
rem   deps\qt\6.8.3\msvc2022_64           (aqtinstall)
rem   deps\opencv\build                    (official opencv-4.10.0-windows.exe)
rem   deps\onnxruntime-win-x64-1.17.3      (GitHub release zip)
rem Output: ..\build-local\install\opentrack.exe
rem Usage: scripts\windows-local\build.bat [reconfigure]

setlocal
set "REPO=%~dp0..\.."
for %%I in ("%REPO%") do set "REPO=%%~fI"
for %%I in ("%REPO%\..") do set "ROOT=%%~fI"
set "DEPS=%ROOT%\deps"
set "BUILD=%ROOT%\build-local"

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

if "%1"=="reconfigure" if exist "%BUILD%\CMakeCache.txt" del "%BUILD%\CMakeCache.txt"

if not exist "%BUILD%\CMakeCache.txt" (
  cmake -G Ninja -S "%REPO%" -B "%BUILD%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
    "-DQt6_DIR=%DEPS%\qt\6.8.3\msvc2022_64\lib\cmake\Qt6" ^
    "-DCMAKE_PREFIX_PATH=%DEPS%\qt\6.8.3\msvc2022_64" ^
    "-DOpenCV_DIR=%DEPS%\opencv\build" ^
    "-DONNXRuntime_DIR=%DEPS%\onnxruntime-win-x64-1.17.3" || exit /b 1
)

cmake --build "%BUILD%" --target install || exit /b 1

rem Runtime DLLs the install step doesn't copy for this local layout.
copy /y "%DEPS%\opencv\build\x64\vc16\bin\opencv_world4100.dll" "%BUILD%\install\" >nul
set "QP=%DEPS%\qt\6.8.3\msvc2022_64\plugins"
for %%D in (tls imageformats styles) do if not exist "%BUILD%\install\%%D" mkdir "%BUILD%\install\%%D"
copy /y "%QP%\tls\qschannelbackend.dll" "%BUILD%\install\tls\" >nul
copy /y "%QP%\imageformats\qico.dll" "%BUILD%\install\imageformats\" >nul
copy /y "%QP%\imageformats\qjpeg.dll" "%BUILD%\install\imageformats\" >nul
copy /y "%QP%\styles\qmodernwindowsstyle.dll" "%BUILD%\install\styles\" >nul
echo Built: %BUILD%\install\opentrack.exe
