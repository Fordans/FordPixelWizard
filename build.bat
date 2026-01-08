@echo off
setlocal enabledelayedexpansion

REM Set FPW_DEBUG=1 to echo each command for debugging batch parsing issues.
if "%FPW_DEBUG%"=="1" echo on

REM FordPixelWizard - Windows build script (Batch)
REM Requirements:
REM - CMake >= 3.20
REM - MSVC Build Tools / Visual Studio (recommended) OR MinGW
REM - OpenCV (C++) installed, with OpenCVConfig.cmake available
REM
REM Usage:
REM   build.bat              -> configure + build (Release)
REM   build.bat Debug        -> configure + build (Debug)
REM
REM Notes:
REM - For OpenCV, set OpenCV_DIR to the folder containing OpenCVConfig.cmake, e.g.:
REM     setx OpenCV_DIR "C:\vendor\opencv\build\x64\vc16\lib"
REM   Or pass -DOpenCV_DIR=... by editing this script.

set "BUILD_TYPE=%~1"
if "%BUILD_TYPE%"=="" set "BUILD_TYPE=Release"

where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] CMake not found in PATH. Install CMake and reopen the terminal.
  exit /b 1
)

REM --- Try auto-detect OpenCV when OpenCV_DIR isn't set ---
REM Many OpenCV Windows layouts look like:
REM   C:\vendor\opencv\build\x64\vc16\lib\OpenCVConfig.cmake
REM   C:\vendor\opencv\build\OpenCVConfig.cmake
REM So we search under a root folder and set OpenCV_DIR to the directory containing OpenCVConfig.cmake.
if "%OpenCV_DIR%"=="" (
  set "OPENCV_ROOT=C:\vendor\opencv"
  REM Optional 3rd argument: override OpenCV root for auto-detect
  REM   build.bat Release [toolset] [opencv_root]
  if not "%~3"=="" set "OPENCV_ROOT=%~3"

  if exist "%OPENCV_ROOT%" (
    set "FOUND_OPENCV_CONFIG="
    for /r "%OPENCV_ROOT%" %%F in (OpenCVConfig.cmake) do (
      if "!FOUND_OPENCV_CONFIG!"=="" (
        set "FOUND_OPENCV_CONFIG=%%~dpF"
      )
    )
    if "!FOUND_OPENCV_CONFIG!"=="" (
      for /r "%OPENCV_ROOT%" %%F in (opencv-config.cmake) do (
        if "!FOUND_OPENCV_CONFIG!"=="" (
          set "FOUND_OPENCV_CONFIG=%%~dpF"
        )
      )
    )
    if not "!FOUND_OPENCV_CONFIG!"=="" (
      REM Trim trailing backslash if present
      set "OpenCV_DIR=!FOUND_OPENCV_CONFIG!"
      if "!OpenCV_DIR:~-1!"=="\" set OpenCV_DIR=!OpenCV_DIR:~0,-1!
      echo [OK] Auto-detected OpenCV_DIR="!OpenCV_DIR!"
    )
  )
)

if "%OpenCV_DIR%"=="" (
  echo [WARN] OpenCV_DIR is not set and auto-detect failed.
  echo        CMake will likely fail to find OpenCV.
  echo        Please set OpenCV_DIR to the directory containing OpenCVConfig.cmake, e.g.:
  echo        setx OpenCV_DIR "C:\vendor\opencv\build\x64\vc16\lib"
  echo.
)

set "ROOT=%~dp0"
REM %~dp0 ends with a trailing backslash. When passed inside quotes to some programs,
REM a trailing backslash before the closing quote can escape the quote and break argument parsing.
REM So we trim the trailing "\" to keep command lines well-formed.
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

REM --- Visual Studio generator settings (reliable defaults on Windows) ---
set "CMAKE_GENERATOR=Visual Studio 17 2022"
set "CMAKE_ARCH=x64"
REM Optional toolset override:
REM   build.bat Release v142
REM If you're using an OpenCV build under "...\\vc16\\...", v142 is recommended,
REM but it requires installing the "MSVC v142" toolset in Visual Studio.
set "CMAKE_TOOLSET=%~2"
set "OPENCV_LOOKS_VC16="
REM Detect "vc16" in path (avoid fragile echo( parsing)
echo.%OpenCV_DIR%| findstr /i "\\vc16\\" >nul
if not errorlevel 1 set "OPENCV_LOOKS_VC16=1"

REM Use a build folder that encodes generator/arch/toolset to avoid stale cache conflicts.
set "BUILD_DIR=%ROOT%\build\vs2022_%CMAKE_ARCH%"
if not "%CMAKE_TOOLSET%"=="" set BUILD_DIR=%BUILD_DIR%_%CMAKE_TOOLSET%

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo.
echo === Configuring (BUILD_TYPE=%BUILD_TYPE%) ===
REM IMPORTANT: keep this on ONE line (batch line-continuation with ^ is fragile if trailing spaces appear).
if "%OPENCV_LOOKS_VC16%"=="1" echo [INFO] OpenCV_DIR path suggests vc16 (VS2019). If you hit link/toolset issues, install v142 or run:
if "%OPENCV_LOOKS_VC16%"=="1" echo        build.bat %BUILD_TYPE% v142

if not "%CMAKE_TOOLSET%"=="" goto :fpw_config_with_toolset
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "%CMAKE_GENERATOR%" -A %CMAKE_ARCH% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DOpenCV_DIR="%OpenCV_DIR%"
goto :fpw_config_done

:fpw_config_with_toolset
echo [INFO] Using toolset override: %CMAKE_TOOLSET%
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "%CMAKE_GENERATOR%" -A %CMAKE_ARCH% -T %CMAKE_TOOLSET% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DOpenCV_DIR="%OpenCV_DIR%"

:fpw_config_done
if errorlevel 1 (
  echo [ERROR] CMake configure failed.
  exit /b 1
)

echo.
echo === Building ===
REM Build the app target explicitly (so we don't stop after only building imgui_impl).
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel --target FordPixelWizard
if errorlevel 1 (
  echo [ERROR] Build failed.
  exit /b 1
)

echo.
echo [OK] Build finished.
echo      Binary should be under:
echo      - Visual Studio generator: %BUILD_DIR%\%BUILD_TYPE%\FordPixelWizard.exe

endlocal
exit /b 0


