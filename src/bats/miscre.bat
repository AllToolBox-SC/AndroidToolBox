@echo off
:rebootp-wipe
CLS
call logo
echo %ORANGE%请选择型号[全部自带文件]%YELLOW%
menu.exe .\menu\miscre.xml
set /p MENU=<menutmp.txt
if "%MENU%"=="A" exit /b
if "%MENU%"=="a" exit /b
if "%MENU%"=="1" set innermodel=I12&&goto otherpash
if "%MENU%"=="2" set innermodel=IB&&goto otherpash
if "%MENU%"=="3" set innermodel=I13C&&goto otherpash
if "%MENU%"=="4" set innermodel=I13&&goto otherpash
if "%MENU%"=="5" set innermodel=I19&&goto otherpash
if "%MENU%"=="6" set innermodel=I18&&goto otherpash
if "%MENU%"=="7" set innermodel=I20&&goto v3pash
if "%MENU%"=="8" set innermodel=I25&&goto v3pash
if "%MENU%"=="9" set innermodel=I25C&&goto v3pash
if "%MENU%"=="10" set innermodel=I25D&&goto v3pash
if "%MENU%"=="11" set innermodel=I32&&goto v3pash
if "%MENU%"=="12" set innermodel=ND07&&goto v3pash
if "%MENU%"=="13" set innermodel=ND01&&goto v3pash
if "%MENU%"=="14" set innermodel=ND03&&goto z10
ECHO %ERROR%输入错误，请重新输入！%RESET%
timeout /t 2 >nul
goto rebootp-wipe

:v3pash
ECHO %INFO%请接入需要刷写的9008设备%RESET%
busybox timeout 10 cmd /c adb reboot edl 2>nul 1>nul
device_check.exe qcom_edl&&ECHO.
ECHO %INFO%拷贝文件到临时目录%RESET%
copy /Y .\EDL\misc\misc_%innermodel%.xml .\EDL\rooting\misc.xml
copy /Y .\wipe.img .\EDL\rooting\misc.img
ECHO %INFO%获取9008端口并执行引导%RESET%
call edlport
call QSaharaServer.bat -p \\.\COM%chkdev__edl__port% -s 13:%cd%\EDL\msm8937.mbn
ECHO %INFO%开始刷入misc%RESET%
call edlport >nul
call fh_loader.bat --port=\\.\COM%chkdev__edl__port% --memoryname=EMMC --search_path=EDL\rooting --sendxml=EDL\rooting\misc.xml --noprompt
ECHO %INFO%执行重启%RESET%
call qfh_loader.bat --port=\\.\COM%chkdev__edl__port% --memoryname=EMMC --search_path=EDL\ --sendxml=reboot.xml --noprompt
ECHO %INFO%清理临时数据%RESET%
del /Q /F ".\EDL\rooting\*.*"
ECHO %INFO%已进入QMMI%RESET%
exit /b

:otherpash
ECHO %INFO%请接入需要刷写的9008设备%RESET%
busybox timeout 10 cmd /c adb reboot edl 2>nul 1>nul
device_check.exe qcom_edl&&ECHO.
ECHO %INFO%拷贝文件到临时目录%RESET%
copy /Y .\EDL\%innermodel%.zip .\EDL\rooting\root.zip
ECHO %INFO%解压所需文件%RESET%
7z x EDL\rooting\root.zip -o.\EDL\rooting\ -aoa >nul 2>&1
copy /Y .\wipe.img .\EDL\rooting\misc.mbn
ECHO %INFO%获取9008端口并执行引导%RESET%
call edlport
call QSaharaServer.bat -p \\.\COM%chkdev__edl__port% -s 13:%cd%\EDL\msm8909w.mbn
ECHO %INFO%开始刷入misc%RESET%
call edlport >nul
call fh_loader.bat --port=\\.\COM%chkdev__edl__port% --memoryname=EMMC --search_path=EDL\rooting --sendxml=EDL\rooting\misc.xml --noprompt
ECHO %INFO%执行重启%RESET%
call qfh_loader.bat --port=\\.\COM%chkdev__edl__port% --memoryname=EMMC --search_path=EDL\ --sendxml=reboot.xml --noprompt
ECHO %INFO%清理临时数据%RESET%
del /Q /F ".\EDL\rooting\*.*"
ECHO %INFO%已进入QMMI%RESET%
exit /b

:z10
ECHO %INFO%请接入需要刷写的9008设备%RESET%
busybox timeout 10 cmd /c adb reboot edl 2>nul 1>nul
device_check.exe qcom_edl&&ECHO.
ECHO %INFO%拷贝文件到临时目录%RESET%
copy /Y .\EDL\misc\misc_ND03.xml .\EDL\rooting\misc.xml
copy /Y .\wipe.img .\EDL\rooting\misc.img
ECHO %INFO%获取9008端口并执行引导%RESET%
call edlport
call QSaharaServer.bat -p \\.\COM%chkdev__edl__port% -s 13:%cd%\EDL\prog_firehose_ddr.elf
ECHO %INFO%开始刷入misc%RESET%
call edlport >nul
call fh_loader.bat --port=\\.\COM%chkdev__edl__port% --memoryname=EMMC --search_path=EDL\rooting --sendxml=EDL\rooting\misc.xml --noprompt
ECHO %INFO%执行重启%RESET%
call qfh_loader.bat --port=\\.\COM%chkdev__edl__port% --memoryname=EMMC --search_path=EDL\ --sendxml=reboot.xml --noprompt
ECHO %INFO%清理临时数据%RESET%
del /Q /F ".\EDL\rooting\*.*"
ECHO %INFO%已进入QMMI%RESET%
exit /b
