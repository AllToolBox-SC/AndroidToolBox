:MagiskModule
CLS
device_check.exe adb&&ECHO.
call boot_completed.bat
for /f "delims=" %%i in ('adb shell getprop ro.build.version.release') do set androidversion=%%i
echo.%info%您的设备安卓版本为:%androidversion%
for /f "delims=" %%i in ('adb shell getprop ro.build.version.sdk') do set sdkversion=%%i
echo %INFO%SDK版本号:%sdkversion%
if "%sdkversion%"=="19" (
goto MagiskModule-1
)
if "%sdkversion%"=="25" (
goto MagiskModule-2
)
echo %INFO%不是安卓4.4.4，也不是安卓7.1.1，无法安装。
pause
exit /b

:MagiskModule-1
call instapp.bat apks\xpinstaller19.apk
call instmodule2.bat tmp\19xposed.zip
echo %INFO%重启手表
adb reboot
echo %INFO%Xposed安装成功！
echo %INFO%5秒后返回
busybox sleep 5
exit /b

:MagiskModule-2
call instapp.bat apks\toolkit.apk
call instapp.bat apks\xposed-magisk.apk
call instmodule2.bat tmp\xposed-magisk-1.zip
echo %INFO%重启手表
adb reboot
echo %INFO%首次刷入xp可能需要7-15分钟开机时间，请耐心等待
device_check.exe adb&&ECHO.
call boot_completed.bat
busybox sleep 10
call instmodule2.bat tmp\xposed-magisk-2.zip
echo %INFO%重启手表
adb reboot
echo %INFO%Xposed安装成功！
echo %INFO%5秒后返回
busybox sleep 5
exit /b