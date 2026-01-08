@echo off
setlocal enabledelayedexpansion

REM Check dependencies of FordPixelWizard.exe using dumpbin (if available)

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "EXE_PATH=%ROOT%\build\vs2022_x64\Release\FordPixelWizard.exe"
if not exist "%EXE_PATH%" (
  echo [ERROR] FordPixelWizard.exe not found at: %EXE_PATH%
  echo        Please build the project first.
  exit /b 1
)

echo Checking dependencies for: %EXE_PATH%
echo.

REM Try to find dumpbin
set "DUMPBIN="
for %%P in (
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC"
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC"
) do (
  if exist "%%P" (
    for /f "delims=" %%V in ('dir /b /ad /o-n "%%P" 2^>nul') do (
      if exist "%%P\%%V\bin\Hostx64\x64\dumpbin.exe" (
        set "DUMPBIN=%%P\%%V\bin\Hostx64\x64\dumpbin.exe"
        goto :found_dumpbin
      )
    )
  )
)

:found_dumpbin
if "%DUMPBIN%"=="" (
  echo [WARN] dumpbin.exe not found. Cannot automatically check dependencies.
  echo        You can manually inspect DLL dependencies using:
  echo        - Dependencies tool: https://github.com/lucasg/Dependencies
  echo        - Or check the package.bat output
  exit /b 0
)

echo Using: %DUMPBIN%
echo.
echo === Dependencies ===
"%DUMPBIN%" /dependents "%EXE_PATH%"
echo.
echo === Summary ===
echo.
echo Required DLLs to package:
echo - OpenCV DLLs (opencv_world*.dll or opencv_*.dll)
echo - Visual C++ Runtime (vcruntime*.dll, msvcp*.dll)
echo - GLFW DLL (if dynamically linked - usually static)
echo.
echo Run package.bat to automatically collect these DLLs.
echo.

endlocal
exit /b 0

