@echo off
setlocal enabledelayedexpansion

:Check_prop
call adbdevice adb
for /f "delims=" %%i in ('adb shell "getprop sys.boot_completed" 2^>^&1') do (
    set "completed=%%i"
    if "!completed!"=="1" goto :Check_pm
)
busybox sleep 5
goto :Check_prop

:Check_pm
adb shell pm list packages >nul 2>&1
if %errorlevel% neq 0 (
    busybox sleep 5
    goto :Check_pm
)
busybox sleep 3
goto :eof