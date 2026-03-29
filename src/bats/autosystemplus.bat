ECHO.%INFO%正在自动激活，请稍后
busybox.exe sleep 10
run_cmd "adb shell input keyevent 4"
run_cmd "adb shell ""su -c am start -n com.huanli233.systemplus/.ActiveSelfActivity"""
device_check.exe adb&&ECHO.
adb shell input swipe 160 300 160 60 100
adb shell input swipe 160 300 160 60 100
adb shell input swipe 160 300 160 60 100
adb shell input swipe 160 300 160 60 100
adb shell input swipe 160 300 160 60 100
adb shell input swipe 160 300 160 60 100
adb shell input tap 200 150
adb shell input tap 200 200
adb shell input swipe 160 60 160 300 100
adb shell input swipe 160 60 160 300 100
adb shell input tap 200 150
adb shell input tap 200 200
adb shell input swipe 160 300 160 60 100
adb shell input swipe 160 300 160 60 100
adb shell input tap 200 100
adb shell input tap 200 150
goto xposed-check
:ROOT-Xposed
ECHO.%INFO%正在启动投屏！如手表端不方便操作，可在电脑端进行操作
ECHO.%INFO%提示：如果手表息屏，在投屏窗口单击右键即可
start scrcpy-noconsole.vbs
run_cmd "adb shell ""su -c am start -n com.huanli233.systemplus/.ActiveSelfActivity"""
ECHO.%INFO%请往下滑，找到自激活，然后点击激活SystemPlus与激活核心破解，然后按任意键继续
pause
:xposed-check
run_cmd "adb push systemplus.sh /sdcard/systemplus.sh"
ECHO.%INFO%开始检查SystemPlus激活状态...
call adbdevice adb
for /f "delims=" %%i in ('adb shell sh /sdcard/systemplus.sh') do set systemplus=%%i
if "%systemplus%"=="1" (
ECHO.%ERROR%未激活
ECHO.%ERROR%没有激活SystemPlus！按任意键重回上一步
pause
goto ROOT-Xposed
)
ECHO.%INFO%已激活
run_cmd "adb push toolkit.sh /sdcard/toolkit.sh"
ECHO.%INFO%开始检查核心破解激活状态...
call adbdevice adb
for /f "delims=" %%i in ('adb shell sh /sdcard/toolkit.sh') do set toolkit=%%i
if "%toolkit%"=="1" (
ECHO.%ERROR%未激活
ECHO.%ERROR%没有激活核心破解！按任意键重回上一步
pause
goto ROOT-Xposed
)
ECHO.%INFO%已激活