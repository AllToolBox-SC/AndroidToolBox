@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%~1"=="" (
  echo Usage: %~nx0 ^<trace-file^> ^<expect-file^>
  exit /b 1
)
if "%~2"=="" (
  echo Usage: %~nx0 ^<trace-file^> ^<expect-file^>
  exit /b 1
)

set "TRACE=%~1"
set "EXPECT=%~2"
if not exist "%TRACE%" (
  echo Trace file not found: %TRACE%
  exit /b 1
)
if not exist "%EXPECT%" (
  echo Expect file not found: %EXPECT%
  exit /b 1
)
if not exist "bin\c_prompt_toolkit_demo.exe" (
  echo Missing bin\c_prompt_toolkit_demo.exe
  exit /b 1
)

set "IN1="
set "IN2="
for /f "usebackq delims=" %%L in ("%TRACE%") do (
  set "L=%%L"
  if defined L if not "!L:~0,1!"=="#" (
    if not defined IN1 (
      set "IN1=!L!"
    ) else if not defined IN2 (
      set "IN2=!L!"
    )
  )
)

if not defined IN1 set "IN1="
if not defined IN2 set "IN2="

set "OUT=%TEMP%\cptk_replay_%RANDOM%.txt"
cmd /c "(echo !IN1!&echo !IN2!)|bin\c_prompt_toolkit_demo.exe" > "%OUT%"
if errorlevel 1 (
  echo Demo execution failed
  type "%OUT%"
  del "%OUT%" >nul 2>nul
  exit /b 1
)

for /f "usebackq delims=" %%E in ("%EXPECT%") do (
  set "E=%%E"
  if defined E if not "!E:~0,1!"=="#" (
    findstr /c:"!E!" "%OUT%" >nul
    if errorlevel 1 (
      echo Missing expected fragment: !E!
      type "%OUT%"
      del "%OUT%" >nul 2>nul
      exit /b 1
    )
  )
)

echo replay test passed: %TRACE%
del "%OUT%" >nul 2>nul
endlocal
