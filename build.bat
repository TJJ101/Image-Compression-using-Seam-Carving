@echo off
setlocal EnableExtensions

rem Windows build script. Generates a Visual Studio 2026 project with CMake and builds it into the
rem GUI-only app build\seamcarve.exe (no command-line mode) and the benchmark suite build\seamcarve_bench.exe.
rem Usage: build.bat [build ^| run ^| open ^| clean ^| help]
rem Needs: git and Visual Studio 2026 with the "Desktop development with C++" workload.
cd /d "%~dp0"

rem When started by double-clicking, keep the window open at the end so messages can be read.
set "PAUSE_AT_END="
echo %cmdcmdline% | find /i "%~nx0" >nul && set "PAUSE_AT_END=1"
if defined NOPAUSE set "PAUSE_AT_END="

call :main %*
set "RC=%errorlevel%"
if defined PAUSE_AT_END echo.
if defined PAUSE_AT_END if not "%RC%"=="0" echo build.bat failed with exit code %RC%. Read the messages above.
if defined PAUSE_AT_END echo Press any key to close this window...
if defined PAUSE_AT_END pause >nul
exit /b %RC%

rem ---------------------------------------------------------------------------

:main
set "CMD=%~1"
if "%CMD%"=="" set "CMD=build"
set "ARGS="
for /f "tokens=1,* delims= " %%a in ("%*") do set "ARGS=%%b"

set "PRESET=windows-gui"
set "BUILD_DIR=build\cmake-windows"
set "SLN=%BUILD_DIR%\SeamCarve.sln"
set "EXE=build\seamcarve.exe"
set "BENCH_EXE=build\seamcarve_bench.exe"

rem Ignore the vcpkg that ships inside Visual Studio (a VS developer prompt points VCPKG_ROOT at it).
if defined VCPKG_ROOT echo %VCPKG_ROOT% | find /i "Microsoft Visual Studio" >nul && set "VCPKG_ROOT="

rem Use the vcpkg in %VCPKG_ROOT% if you set one, otherwise a repo-local .\vcpkg-win
rem (kept separate from the Linux/WSL .\vcpkg so the two do not clash).
set "OWN_VCPKG=1"
if not defined VCPKG_ROOT set "OWN_VCPKG="
if not defined VCPKG_ROOT set "VCPKG_ROOT=%~dp0vcpkg-win"
set "WANTED_VCPKG_ROOT=%VCPKG_ROOT%"

if /i "%CMD%"=="help"  goto :help
if /i "%CMD%"=="-h"    goto :help
if /i "%CMD%"=="build" goto :build
if /i "%CMD%"=="run"   goto :run
if /i "%CMD%"=="open"  goto :open
if /i "%CMD%"=="clean" goto :clean
echo Unknown command '%CMD%'.
echo.
call :show_help
exit /b 1

:help
call :show_help
exit /b 0

:build
call :ensure_built || exit /b 1
echo.
echo Built %EXE% and %BENCH_EXE%
echo Visual Studio project: %SLN%
exit /b 0

:run
call :ensure_built || exit /b 1
start "" "%EXE%" %ARGS%
exit /b 0

:open
call :ensure_configured || exit /b 1
start "" "%SLN%"
exit /b 0

:clean
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
if exist "%EXE%" del /q "%EXE%"
if defined OWN_VCPKG goto :clean_keep
if exist vcpkg-win rmdir /s /q vcpkg-win
exit /b 0
:clean_keep
echo Keeping %VCPKG_ROOT% (your own vcpkg, not the repo-local one)
exit /b 0

rem ---------------------------------------------------------------------------

:show_help
echo Usage: build.bat [command]
echo.
echo   build   Generate the Visual Studio project and build seamcarve.exe and seamcarve_bench.exe (default)
echo   run     Build if needed, then open the app (an image path may follow)
echo   open    Generate the project if needed, then open it in Visual Studio
echo   clean   Delete the Windows build output and the repo-local vcpkg-win
echo   help    Show this help
exit /b 0

rem Bootstrap vcpkg and generate the Visual Studio project. Each step is skipped if already done.
:ensure_configured
echo [1/3] Setting up Visual Studio...
call :setup_env || exit /b 1
rem The Visual Studio environment overwrites VCPKG_ROOT, so set ours again.
set "VCPKG_ROOT=%WANTED_VCPKG_ROOT%"
if exist "%VCPKG_ROOT%\vcpkg.exe" goto :vcpkg_ready
echo [2/3] Getting vcpkg into %VCPKG_ROOT% ...
if not exist "%VCPKG_ROOT%" git clone --depth 1 https://github.com/microsoft/vcpkg.git "%VCPKG_ROOT%" || exit /b 1
call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics || exit /b 1
:vcpkg_ready
rem The .sln only exists after a generate that succeeded (the cache file can exist after a failed one).
if exist "%SLN%" exit /b 0
echo [3/3] Generating the Visual Studio project (the first time this builds OpenCV, ImGui and GLFW - it can take a long while)...
"%CMAKE_EXE%" --preset %PRESET% || exit /b 1
exit /b 0

rem Build the generated Visual Studio project (MSBuild) into build\seamcarve.exe and build\seamcarve_bench.exe.
:ensure_built
call :ensure_configured || exit /b 1
echo Building...
"%CMAKE_EXE%" --build --preset %PRESET% || exit /b 1
exit /b 0

rem Find Visual Studio, use its CMake (the newest one, which knows the "Visual Studio 18 2026"
rem generator) and load the C++ compiler environment unless it is already loaded.
:setup_env
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" echo Visual Studio was not found. Install Visual Studio 2026 with the C++ workload. & exit /b 1
set "VS_PATH="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH echo Visual Studio C++ tools were not found. & exit /b 1
set "CMAKE_EXE=cmake"
if exist "%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "CMAKE_EXE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
where cl >nul 2>nul && exit /b 0
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 || exit /b 1
exit /b 0
