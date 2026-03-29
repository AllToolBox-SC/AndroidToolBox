CLS
echo %INFO%请确保手表端已经开启了自动响应%RESET%
device_check.exe adb&&ECHO.
for /f "delims=" %%i in ('adb shell getprop ro.product.innermodel') do set innermodel=%%i
call einfo 您的设备innermodel为:%innermodel%
for /f "delims=" %%i in ('adb shell getprop ro.build.version.release') do set androidversion=%%i
call einfo 您的设备安卓版本为:%androidversion%
if "%androidversion%"=="7.1.1" (
    call instmodule2.bat tmp\xtcpatch.zip
    goto XTCPatch-DONE
)
if "%androidversion%"=="4.4.4" (
    call instmodule2.bat tmp\xtcpatch.zip
    goto XTCPatch-DONE
)

call instmodule.bat tmp\xtcpatch.zip
:XTCPatch-DONE
echo %INFO%重启手表%RESET%
adb reboot
echo %GREEN%XTCPatch安装完成%RESET%
pause
