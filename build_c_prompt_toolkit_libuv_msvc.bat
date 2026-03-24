@echo off
setlocal

if not exist bin mkdir bin
if not exist third_party mkdir third_party
if not exist build mkdir build

if not defined INCLUDE (
  set "_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  if exist "%_VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "_VSINSTALL=%%I"
  )
  if defined _VSINSTALL (
    call "%_VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
  )
)

if not defined INCLUDE (
  for %%P in (
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    "D:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  ) do (
    if exist "%%~fP" call "%%~fP" >nul 2>&1
    if defined INCLUDE goto :vcvars_done
  )
)

:vcvars_done

if not defined INCLUDE (
  for /f "usebackq delims=" %%I in (`powershell -NoProfile -Command "$cl=(Get-Command cl -ErrorAction SilentlyContinue).Source; if($cl){$p=Split-Path $cl -Parent; for($i=0;$i -lt 6;$i++){ $p=Split-Path $p -Parent }; $v=Join-Path $p 'Auxiliary\\Build\\vcvars64.bat'; if(Test-Path $v){$v}}"`) do set "_VCVARS=%%I"
  if defined _VCVARS (
    call "%_VCVARS%" >nul 2>&1
  )
)

where cl >nul 2>&1
if errorlevel 1 (
  echo cl.exe not found.
  exit /b 1
)

set "CMAKE_EXE="
where cmake >nul 2>&1
if not errorlevel 1 set "CMAKE_EXE=cmake"

if "%CMAKE_EXE%"=="" if defined _VSINSTALL (
  if exist "%_VSINSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
    set "CMAKE_EXE=%_VSINSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  )
)

if "%CMAKE_EXE%"=="" (
  for %%P in (
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    "D:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  ) do (
    if exist "%%~fP" (
      set "CMAKE_EXE=%%~fP"
      goto :cmake_found
    )
  )
)

:cmake_found

if "%CMAKE_EXE%"=="" (
  echo cmake not found.
  exit /b 1
)

if not exist third_party\libuv\CMakeLists.txt (
  git clone --depth 1 https://github.com/libuv/libuv third_party\libuv
  if errorlevel 1 (
    echo Failed to clone libuv.
    exit /b 1
  )
)

"%CMAKE_EXE%" -S third_party\libuv -B build\libuv -A x64 -DBUILD_TESTING=OFF -DBUILD_BENCH=OFF -DBUILD_SHARED_LIBS=OFF
if errorlevel 1 (
  echo Failed to configure libuv.
  exit /b 1
)

"%CMAKE_EXE%" --build build\libuv --config Release --target uv_a
if errorlevel 1 (
  echo Failed to build libuv.
  exit /b 1
)

set "LIBUV_INCLUDE=%CD%\third_party\libuv\include"
set "LIBUV_LIB="
set "LIBUV_LIBNAME="

if exist "%CD%\build\libuv\Release\uv_a.lib" (
  set "LIBUV_LIB=%CD%\build\libuv\Release"
  set "LIBUV_LIBNAME=uv_a.lib"
)
if "%LIBUV_LIB%"=="" if exist "%CD%\build\libuv\uv_a.lib" (
  set "LIBUV_LIB=%CD%\build\libuv"
  set "LIBUV_LIBNAME=uv_a.lib"
)
if "%LIBUV_LIB%"=="" if exist "%CD%\build\libuv\Release\uv.lib" (
  set "LIBUV_LIB=%CD%\build\libuv\Release"
  set "LIBUV_LIBNAME=uv.lib"
)
if "%LIBUV_LIB%"=="" if exist "%CD%\build\libuv\Release\libuv.lib" (
  set "LIBUV_LIB=%CD%\build\libuv\Release"
  set "LIBUV_LIBNAME=libuv.lib"
)

if "%LIBUV_LIB%"=="" (
  echo Unable to locate libuv library output.
  exit /b 1
)

set "CPTK_ENABLE_LIBUV=1"
set "CPTK_REQUIRE_LIBUV=1"

call build_c_prompt_toolkit_msvc.bat
if errorlevel 1 (
  exit /b 1
)

set "_OUT=%TEMP%\cptk_libuv_smoke_%RANDOM%.txt"
cmd /c "(echo hello&echo 5)|bin\c_prompt_toolkit_demo.exe" > "%_OUT%"
if errorlevel 1 (
  type "%_OUT%"
  del "%_OUT%" >nul 2>nul
  exit /b 1
)

findstr /c:"backend: libuv" "%_OUT%" >nul
if errorlevel 1 (
  echo backend is not libuv
  type "%_OUT%"
  del "%_OUT%" >nul 2>nul
  exit /b 1
)

echo libuv one-click build and verification passed

del "%_OUT%" >nul 2>nul
endlocal
