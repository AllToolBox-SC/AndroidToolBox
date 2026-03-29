@echo off
:run
adb shell exit 2>&1 | findstr /C:"more than one device/emulator" >nul
if %errorlevel% equ 0 (
    echo %error%检测到多个ADB设备连接,无法继续运行。
    echo %info%当前连接设备：
    adb devices
    echo 按任意键重试...
    pause >nul
    goto run
)