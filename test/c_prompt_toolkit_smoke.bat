@echo off
setlocal

if not exist bin\c_prompt_toolkit_demo.exe (
  echo Missing bin\c_prompt_toolkit_demo.exe
  exit /b 1
)

set "OUT=%TEMP%\cptk_smoke_%RANDOM%.txt"
cmd /c "(echo hello&echo 10)|bin\c_prompt_toolkit_demo.exe" > "%OUT%"
if errorlevel 1 (
  echo Demo execution failed
  del "%OUT%" >nul 2>nul
  exit /b 1
)

findstr /c:"=== c_prompt_toolkit PoC ===" "%OUT%" >nul || (
  echo Missing banner in output
  type "%OUT%"
  del "%OUT%" >nul 2>nul
  exit /b 1
)

findstr /c:"You typed: hello" "%OUT%" >nul || (
  echo Missing typed-line output
  type "%OUT%"
  del "%OUT%" >nul 2>nul
  exit /b 1
)

findstr /c:"Selected: 10." "%OUT%" >nul || (
  echo Missing selected output
  type "%OUT%"
  del "%OUT%" >nul 2>nul
  exit /b 1
)

echo c_prompt_toolkit smoke test passed

del "%OUT%" >nul 2>nul
endlocal
