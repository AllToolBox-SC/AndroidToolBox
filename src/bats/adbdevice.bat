@echo off
if "%1"=="more" goto more
if "%1"=="twrp" goto check_twrp
if "%1"=="sideload" goto sideload
if "%1"=="noadb" goto noadb
if "%1"=="adb" goto adb
echo %error%内部错误:参数错误
:more
adb shell exit 2>&1 | findstr /C:"more than one device/emulator" >nul
if %errorlevel% equ 0 (
    echo %error%检测到多个ADB设备连接,无法继续运行。
    echo %info%当前连接设备：
    adb devices
    echo 按任意键重试...
    pause >nul
    goto more
)
exit /b 0

:check_twrp
adb shell twrp --version 2>&1 | findstr /i /C:"twrp" /C:"version" /C:"OrangeFox" 1>nul 2>nul
if %errorlevel% neq 0 (
    busybox sleep 5
    goto check_twrp
)
exit /b 0

:sideload
adb shell exit 2>&1 | findstr /i /C:"closed" 1>nul 2>nul
if %errorlevel% neq 0 (
    adb shell twrp sideload 1>nul 2>nul
    busybox sleep 5
    goto sideload
)
exit /b 0

:noadb
adb shell exit 2>&1 | findstr /i /C:"device" 1>nul 2>nul
if %errorlevel% neq 0 (
    busybox sleep 5
    goto noadb
)
exit /b 0

:adb
adb shell exit 2>&1 | findstr /i /C:"no" /C:"error" 1>nul 2>nul
if %errorlevel% equ 0 (
    busybox sleep 1
    goto adb
)
exit /b 0