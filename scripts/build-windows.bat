@echo off
rem Builds Emberline Spectralis (Standalone + VST3) on Windows.
rem Usage: scripts\build-windows.bat [run]     ("run" launches the standalone afterwards)
setlocal

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo Could not find Visual Studio with the C++ tools. Install the VS 2022 Build Tools.
    exit /b 1
)

call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0.."

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build --target EmberlineSpectralis_Standalone EmberlineSpectralis_VST3 || exit /b 1

echo.
echo Standalone: %CD%\build\EmberlineSpectralis_artefacts\Release\Standalone\Emberline Spectralis.exe
echo VST3:       %CD%\build\EmberlineSpectralis_artefacts\Release\VST3\Emberline Spectralis.vst3

if /i "%~1"=="run" start "" "%CD%\build\EmberlineSpectralis_artefacts\Release\Standalone\Emberline Spectralis.exe"
endlocal
