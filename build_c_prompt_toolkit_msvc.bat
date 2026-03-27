@echo off
setlocal EnableDelayedExpansion

if not exist bin mkdir bin

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
  echo cl.exe not found. Please install Visual Studio C++ Build Tools.
  exit /b 1
)

if not defined INCLUDE (
  echo INCLUDE is not configured. Please run in Developer Command Prompt or install VS Build Tools.
  exit /b 1
)

echo Building c_prompt_toolkit.dll ...
set "CPTK_DEFS=/DCPTK_BUILD_DLL"
set "CPTK_LIBS="

if "%CPTK_ENABLE_LIBUV%"=="1" (
  if "%LIBUV_INCLUDE%"=="" (
    echo LIBUV_INCLUDE is required when CPTK_ENABLE_LIBUV=1
    exit /b 1
  )
  if "%LIBUV_LIB%"=="" (
    echo LIBUV_LIB is required when CPTK_ENABLE_LIBUV=1
    exit /b 1
  )
  if "%LIBUV_LIBNAME%"=="" set "LIBUV_LIBNAME=uv.lib"
  set "CPTK_DEFS=%CPTK_DEFS% /DCPTK_ENABLE_LIBUV"
  if "%CPTK_REQUIRE_LIBUV%"=="1" set "CPTK_DEFS=!CPTK_DEFS! /DCPTK_REQUIRE_LIBUV"
  set "CPTK_LIBS=/LIBPATH:""%LIBUV_LIB%"" %LIBUV_LIBNAME% ws2_32.lib iphlpapi.lib userenv.lib psapi.lib advapi32.lib user32.lib shell32.lib ole32.lib dbghelp.lib"
  set "CPTK_INC=/I src /I ""%LIBUV_INCLUDE%"""
) else (
  set "CPTK_INC=/I src"
)

set "CPTK_SRCS=src\c_prompt_toolkit.c src\c_prompt_toolkit_loop.c src\c_prompt_toolkit_vt100.c src\c_prompt_toolkit_buffer.c src\c_prompt_toolkit_keymap.c src\menu.c"

cl /nologo /W3 /MD /O2 /utf-8 !CPTK_DEFS! /LD %CPTK_INC% /Fe:bin\c_prompt_toolkit.dll %CPTK_SRCS% /link /SUBSYSTEM:CONSOLE %CPTK_LIBS%
if errorlevel 1 (
  echo Failed to build c_prompt_toolkit.dll
  exit /b 1
)

echo Building c_prompt_toolkit_demo.exe ...
cl /nologo /W3 /MD /O2 /utf-8 !CPTK_DEFS! %CPTK_INC% /Fe:bin\c_prompt_toolkit_demo.exe src\c_prompt_toolkit_demo.c %CPTK_SRCS% /link /SUBSYSTEM:CONSOLE %CPTK_LIBS%
if errorlevel 1 (
  echo Failed to build c_prompt_toolkit_demo.exe
  exit /b 1
)

echo Building menu.exe ...
cl /nologo /W3 /MD /O2 /utf-8 !CPTK_DEFS! %CPTK_INC% /Fe:bin\menu.exe src\menu_main.c %CPTK_SRCS% /link /SUBSYSTEM:CONSOLE %CPTK_LIBS%
if errorlevel 1 (
  echo Failed to build menu.exe
  exit /b 1
)

echo Building pause.exe ...
cl /nologo /W3 /MD /O2 /utf-8 /Fe:bin\pause.exe src\pause_main.c src\pause.c /link /SUBSYSTEM:CONSOLE
if errorlevel 1 (
  echo Failed to build pause.exe
  exit /b 1
)

echo Building c_prompt_toolkit_replay.exe ...
cl /nologo /W3 /MD /O2 /utf-8 !CPTK_DEFS! %CPTK_INC% /Fe:bin\c_prompt_toolkit_replay.exe src\c_prompt_toolkit_replay.c src\c_prompt_toolkit_buffer.c src\c_prompt_toolkit_keymap.c /link /SUBSYSTEM:CONSOLE %CPTK_LIBS%
if errorlevel 1 (
  echo Failed to build c_prompt_toolkit_replay.exe
  exit /b 1
)

echo Build succeeded.
endlocal
