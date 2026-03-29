ECHO.%INFO%请连接ADB设备后开始操作%RESET%
device_check.exe adb&&ECHO.
adb shell content call --uri content://com.xtc.launcher.self.start --method METHOD_SELF_START --extra EXTRA_ENABLE:b:true --arg com.tencent.qqlite
adb shell content call --uri content://com.xtc.launcher.self.start --method METHOD_SELF_START --extra EXTRA_ENABLE:b:true --arg com.tencent.qqwatch
adb shell content call --uri content://com.xtc.launcher.self.start --method METHOD_SELF_START --extra EXTRA_ENABLE:b:true --arg com.tencent.wechatkids
ECHO.%GREEN%操作完成，按任意键返回主页面%RESET%
pause >nul