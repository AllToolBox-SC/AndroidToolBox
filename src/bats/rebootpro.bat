@echo off
:rebootP
CLS
call logo.bat
ECHO %ORANGE%高级重启%YELLOW%
menu.exe .\menu\rebootpro.xml
set /p MENU=<menutmp.txt
if "%MENU%"=="A" exit /b
if "%MENU%"=="a" exit /b
if "%MENU%"=="1" goto rebootP-reboot
if "%MENU%"=="2" goto rebootP-bl
if "%MENU%"=="3" goto rebootP-re
if "%MENU%"=="4" goto rebootP-edl
ECHO %ERROR%输入错误，请重新输入！%RESET%
timeout /t 2 >nul
goto rebootP

:rebootP-reboot
ECHO %INFO%请插入adb,9008设备%RESET%
device_check.exe adb qcom_edl | findstr "ADB" >nul && goto rebootP-reboot-adb || goto rebootP-reboot-edl
:rebootP-reboot-adb
adb reboot 1>nul 2>nul
ECHO %INFO%完成！，按任意键继续%RESET%
pause >nul
goto rebootP

:rebootP-reboot-edl
CLS
call logo.bat
ECHO %ORANGE%选择该如何引导?%YELLOW%
menu.exe .\menu\rebootpro_edl.xml
set /p MENU=<menutmp.txt
if "%mbnMENU%"=="A" goto rebootP
if "%mbnMENU%"=="1" set whatmbn=msm8937.mbn & goto rebootP-reboot-edl-run
if "%mbnMENU%"=="2" set whatmbn=msm8909w.mbn & goto rebootP-reboot-edl-run
if "%mbnMENU%"=="3" goto noQS
ECHO %ERROR%输入错误，请重新输入！%RESET%
timeout /t 2 >nul
goto rebootP-reboot-edl
:rebootP-reboot-edl-run
call edlport
QSaharaServer.bat -p \\.\COM%chkdev__edl__port% -s 13:%cd%\EDL\%whatmbn%
:noQS
call edlport
call qfh_loader.bat --port=\\.\COM%chkdev__edl__port% --memoryname=EMMC --search_path=EDL\ --sendxml=reboot.xml --noprompt
ECHO %INFO%完成！，按任意键继续%RESET%
pause >nul
goto rebootP

:rebootP-bl
ECHO %INFO%%YELLOW%请插入adb设备%RESET%
device_check.exe adb&&ECHO.
adb reboot bootloader 2>nul
ECHO %INFO%%YELLOW%完成！，按任意键继续%RESET%
pause >nul
goto rebootP

:rebootP-re
ECHO %INFO%%YELLOW%请插入adb设备%RESET%
device_check.exe adb&&ECHO.
adb reboot recovery 2>nul 1>nul
ECHO %INFO%%YELLOW%完成！，按任意键继续%RESET%
pause >nul
goto rebootP

:rebootP-edl
ECHO %INFO%%YELLOW%请插入adb设备%RESET%
device_check.exe adb&&ECHO.
adb reboot edl 2>nul 1>nul
ECHO %INFO%%YELLOW%完成！，按任意键继续%RESET%
pause >nul
goto rebootP
