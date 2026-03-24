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
if not exist "bin\c_prompt_toolkit_replay.exe" (
  echo Missing bin\c_prompt_toolkit_replay.exe
  exit /b 1
)

set "OUT=%TEMP%\cptk_keys_replay_%RANDOM%.txt"
"bin\c_prompt_toolkit_replay.exe" "%TRACE%" > "%OUT%"
if errorlevel 1 (
  echo Replay execution failed
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

echo key replay test passed: %TRACE%
del "%OUT%" >nul 2>nul
endlocal
