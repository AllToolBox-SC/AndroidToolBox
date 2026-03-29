:twrp-re
CLS
call logo
echo %ORANGE%请选择型号%YELLOW%
menu.exe .\menu\pashtwrppro.xml
set /p MENU=<menutmp.txt
if "%MENU%"=="A" exit /b
if "%MENU%"=="1" set innermodel=I12 & goto otherpash
if "%MENU%"=="2" set innermodel=IB & goto otherpash
if "%MENU%"=="3" set innermodel=I13C & goto otherpash
if "%MENU%"=="4" set innermodel=I13 & goto otherpash
if "%MENU%"=="5" set innermodel=I19 & goto otherpash
if "%MENU%"=="6" set innermodel=I18 & goto otherpash
if "%MENU%"=="7" set innermodel=I20 & goto v3pash
if "%MENU%"=="8" set innermodel=I25 & goto v3pash
if "%MENU%"=="9" set innermodel=I25C & goto otherpash
if "%MENU%"=="10" set innermodel=I25D & goto v3pash
if "%MENU%"=="11" set innermodel=I32 & goto v3pash
if "%MENU%"=="12" set innermodel=ND07 & goto v3pash
if "%MENU%"=="13" set innermodel=ND01 & goto v3pash
ECHO %ERROR%输入错误，请重新输入！%RESET%
timeout /t 2 >nul
goto twrp-re

:otherpash
ECHO 请前往下载
ECHO 按任意键打开下载链接
pause >nul
start https://www.123865.com/s/Q5JfTd-udbWH
ECHO 请确保你已经下载对应机型的TWRP文件
ECHO 按任意键去选择文件
pause >nul
call sel file s .. [img]
ECHO %INFO%请接入需要导入刷写TWRP脚本的adb设备%RESET%
busybox timeout 10 cmd /c adb reboot edl 2>nul 1>nul
device_check.exe adb&&ECHO.
ECHO %INFO%拷贝文件到临时目录%RESET%
copy /Y .\EDL\%innermodel%.zip .\EDL\rooting\root.zip
ECHO %INFO%解压所需文件%RESET%
7z x EDL\rooting\root.zip -o.\EDL\rooting\ -aoa >nul 2>&1
copy "%sel__file_path%" ".\EDL\rooting\recovery.img"
ECHO %INFO%开始导入脚本%RESET%
adb push .\EDL\rooting\recovery.img /sdcard/
adb push rec.sh /sdcard/
adb shell "su -c cp /sdcard/recovery.img /data/rec.img"
adb shell "su -c cp /sdcard/rec.sh /data/adb/service.d/rec.sh"
adb shell "chmod 755 -R /data/adb/service.d/rec.sh"
ECHO %INFO%清理临时数据%RESET%
del /Q /F ".\EDL\rooting\*.*"
ECHO %INFO%刷入完成，按任意键返回%RESET%
pause >nul
exit /b

:v3pash
ECHO 请前往下载
ECHO 按任意键打开下载链接
pause >nul
start https://www.123865.com/s/Q5JfTd-udbWH
ECHO 请确保你已经下载对应机型的TWRP文件
ECHO 按任意键去选择文件
pause >nul
call sel file s .. [img]
ECHO %INFO%请接入需要导入刷写TWRP脚本的adb设备%RESET%
busybox timeout 10 cmd /c adb reboot edl 2>nul 1>nul
device_check.exe adb&&ECHO.
ECHO %INFO%拷贝文件到临时目录%RESET%
copy /Y .\EDL\%innermodel%.zip .\EDL\rooting\root.zip
ECHO %INFO%解压所需文件%RESET%
7z x EDL\rooting\root.zip -o.\EDL\rooting\ -aoa >nul 2>&1
copy "%sel__file_path%" ".\EDL\rooting\recovery.img"
ECHO %INFO%开始导入脚本%RESET%
adb push .\EDL\rooting\recovery.img /sdcard/
adb push rec.sh /sdcard/
adb shell "su -c cp /sdcard/recovery.img /data/rec.img"
adb shell "su -c cp /sdcard/rec.sh /data/adb/service.d/rec.sh"
adb shell "chmod 755 -R /data/adb/service.d/rec.sh"
ECHO %INFO%清理临时数据%RESET%
del /Q /F ".\EDL\rooting\*.*"
ECHO %INFO%完成，按任意键返回%RESET%
pause >nul
exit /b