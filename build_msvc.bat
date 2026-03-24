@echo off
rem Build start.exe using MSVC (run this from "x64 Native Tools Command Prompt for VS")
setlocal
if not exist bin mkdir bin
where cl >nul 2>&1
if errorlevel 1 (
  echo cl.exe not found. Please run this from a Developer Command Prompt for Visual Studio.
  exit /b 1
)

echo Compiling src\start.c ...
cl /nologo /W3 /MD /O2 /utf-8 /Fe:bin\start.exe src\start.c /link Ws2_32.lib wininet.lib bin\c_prompt_toolkit.lib /SUBSYSTEM:CONSOLE
if errorlevel 1 (
  echo Build failed.
  exit /b 1
)
echo Build succeeded: bin\start.exe
endlocal
