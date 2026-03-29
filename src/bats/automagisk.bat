ECHO.%INFO%正在自动打开自动响应，请稍后
run_cmd "adb shell am start -n com.topjohnwu.magisk/.ui.MainActivity"
busybox.exe sleep 10
run_cmd "adb shell input keyevent 4"
run_cmd "adb shell am start -n com.topjohnwu.magisk/.ui.MainActivity"
device_check.exe adb&&ECHO.
adb shell input tap 304 26
adb shell input swipe 160 300 160 60 100
adb shell input swipe 160 300 160 60 100
adb shell input swipe 160 300 160 60 100
adb shell input swipe 160 300 160 60 100
adb shell input swipe 160 300 160 60 100
adb shell input tap 200 100
adb shell input tap 200 230
adb shell input tap 200 300
adb shell input tap 200 140
adb shell "su -c magisk -v" || echo.%ERROR%自动授予出错及手动授予权限&&goto magisk
exit /b

:magisk
ECHO.%INFO%正在启动投屏！如手表端不方便操作，可在电脑端进行操作
ECHO.%INFO%提示：如果手表息屏，在投屏窗口单击右键即可
start scrcpy-noconsole.vbs
ECHO.%INFO%请打开Magisk右上角设置，往下滑，找到自动响应，修改为允许，然后找到超级用户通知，修改为无
ECHO.%INFO%然后在主页点击超级用户，将所有开关打开
ECHO.%INFO%操作完成后请按任意键继续
pause
adb shell "su -c magisk -v" || echo.%ERROR%授予出错，请重新授予&&goto magisk