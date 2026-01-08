@echo off
setlocal enabledelayedexpansion

REM FordPixelWizard - Package script for distribution
REM This script collects all DLL dependencies and creates a portable package

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "BUILD_TYPE=Release"
if not "%~1"=="" set "BUILD_TYPE=%~1"

set "BUILD_DIR=%ROOT%\build\vs2022_x64"
set "OUTPUT_DIR=%ROOT%\dist\FordPixelWizard"

echo.
echo === Packaging FordPixelWizard for Distribution ===
echo.

REM Check if build exists
set "EXE_PATH=%BUILD_DIR%\%BUILD_TYPE%\FordPixelWizard.exe"
if not exist "%EXE_PATH%" (
  echo [ERROR] Build not found at: %EXE_PATH%
  echo        Please run build.bat first.
  exit /b 1
)

echo [1/6] Creating distribution directory...
if exist "%OUTPUT_DIR%" rmdir /s /q "%OUTPUT_DIR%"
mkdir "%OUTPUT_DIR%"

echo [2/6] Copying executable and icon...
copy /y "%EXE_PATH%" "%OUTPUT_DIR%\" >nul
if exist "%ROOT%\icon.png" copy /y "%ROOT%\icon.png" "%OUTPUT_DIR%\" >nul
if exist "%ROOT%\icon.ico" copy /y "%ROOT%\icon.ico" "%OUTPUT_DIR%\" >nul

echo [3/6] Collecting OpenCV DLLs...
if "%OpenCV_DIR%"=="" (
  set "OPENCV_SEARCH=C:\vendor\opencv"
) else (
  REM Extract OpenCV root from OpenCV_DIR (usually .../build/x64/vc16/lib)
  set "OPENCV_SEARCH=%OpenCV_DIR%"
  set "OPENCV_SEARCH=!OPENCV_SEARCH:\build\=!"
  set "OPENCV_SEARCH=!OPENCV_SEARCH:\x64\=!"
  set "OPENCV_SEARCH=!OPENCV_SEARCH:\vc16\=!"
  set "OPENCV_SEARCH=!OPENCV_SEARCH:\lib\=!"
  set "OPENCV_SEARCH=!OPENCV_SEARCH:\lib=!"
)

REM Try to find OpenCV bin directory
set "OPENCV_BIN="
for %%P in ("%OpenCV_DIR%\..\bin" "%OpenCV_DIR%\..\..\bin" "%OPENCV_SEARCH%\build\x64\vc16\bin" "%OPENCV_SEARCH%\build\x64\vc17\bin" "%OPENCV_SEARCH%\x64\vc16\bin" "%OPENCV_SEARCH%\x64\vc17\bin") do (
  if exist "%%~P\opencv_world*.dll" (
    set "OPENCV_BIN=%%~P"
    goto :found_opencv_bin
  )
)

:found_opencv_bin
if not "%OPENCV_BIN%"=="" (
  echo        Found OpenCV at: %OPENCV_BIN%
  REM Copy OpenCV DLLs (opencv_world*.dll or opencv_*.dll)
  for %%F in ("%OPENCV_BIN%\opencv_world*.dll" "%OPENCV_BIN%\opencv_*.dll") do (
    if exist "%%F" (
      copy /y "%%F" "%OUTPUT_DIR%\" >nul
      echo        Copied: %%~nxF
    )
  )
) else (
  echo [WARN] Could not find OpenCV DLLs automatically.
  echo        Please manually copy OpenCV DLLs to: %OUTPUT_DIR%
  echo        Look for opencv_world*.dll or opencv_*.dll in your OpenCV installation.
)

echo [4/6] Collecting Visual C++ Runtime DLLs...
REM VC++ Runtime DLLs location (common paths)
set "VC_RUNTIME_FOUND=0"
for %%P in (
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC"
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC"
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional\VC\Redist\MSVC"
  "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC"
) do (
  if exist "%%P" (
    REM Find the latest version directory
    for /f "delims=" %%V in ('dir /b /ad /o-n "%%P" 2^>nul') do (
      if exist "%%P\%%V\x64\Microsoft.VC*.CRT" (
        echo        Found VC++ Runtime at: %%P\%%V\x64
        xcopy /y /q "%%P\%%V\x64\Microsoft.VC*.CRT\*.dll" "%OUTPUT_DIR%\" >nul 2>&1
        xcopy /y /q "%%P\%%V\x64\Microsoft.VC*.CRT\vcruntime*.dll" "%OUTPUT_DIR%\" >nul 2>&1
        set "VC_RUNTIME_FOUND=1"
        goto :found_vc_runtime
      )
    )
  )
)

:found_vc_runtime
if "%VC_RUNTIME_FOUND%"=="0" (
  echo [WARN] Could not find Visual C++ Runtime DLLs automatically.
  echo        Users may need to install VC++ Redistributable.
  echo        Download: https://aka.ms/vs/17/release/vc_redist.x64.exe
)

echo [5/6] Collecting GLFW DLL (if dynamically linked)...
REM GLFW is usually statically linked, but check anyway
if exist "%BUILD_DIR%\%BUILD_TYPE%\glfw3.dll" (
  copy /y "%BUILD_DIR%\%BUILD_TYPE%\glfw3.dll" "%OUTPUT_DIR%\" >nul
  echo        Copied glfw3.dll
) else (
  echo        GLFW appears to be statically linked (no DLL needed)
)

echo [6/6] Creating README for users...
(
  echo FordPixelWizard - Portable Distribution
  echo ========================================
  echo.
  echo This is a portable version that requires no installation.
  echo.
  echo Requirements:
  echo - Windows 10 or later
  echo - Visual C++ Redistributable 2015-2022 ^(if not already installed^)
  echo.
  echo Usage:
  echo 1. Double-click FordPixelWizard.exe to run
  echo 2. Click "Browse..." to select an image file
  echo 3. Adjust parameters and click "Pixelize"
  echo 4. Review the result in the preview
  echo 5. Click "Save" to save the pixel art result
  echo.
  echo If you get "missing DLL" errors:
  echo - Install Visual C++ Redistributable:
  echo   https://aka.ms/vs/17/release/vc_redist.x64.exe
  echo.
  echo All files in this folder must be kept together.
) > "%OUTPUT_DIR%\README.txt"

echo.
echo [OK] Packaging complete!
echo.
echo Distribution package created at:
echo   %OUTPUT_DIR%
echo.
echo Package contents:
dir /b "%OUTPUT_DIR%"
echo.
echo You can now distribute the entire "%OUTPUT_DIR%" folder.
echo Test it on a clean Windows machine to ensure all dependencies are included.
echo.
echo Note: If OpenCV DLLs are missing, you may need to manually copy them from:
if not "%OPENCV_BIN%"=="" echo   %OPENCV_BIN%
echo.

endlocal
exit /b 0

