if "%1"=="DCIM" goto DCIM
if "%1"=="EDL" goto EDL
if "%1"=="9008" goto EDL


:MAIN_MENU
CLS
call logo.bat
ECHO %ORANGE%备份与恢复菜单%YELLOW%
menu.exe .\menu\backup.xml
set /p MENU=<menutmp.txt
if "%MENU%"=="A" exit /b
if "%MENU%"=="a" exit /b
if "%MENU%"=="1" goto DCIM-backup
if "%MENU%"=="2" goto DCIM-recover
if "%MENU%"=="3" goto 9008-backup
if "%MENU%"=="4" goto 9008-recover
ECHO %ERROR%输入错误，请重新输入！%RESET%
timeout /t 2 >nul
goto MAIN_MENU

:EDL
if "%2"=="recover" goto 9008-recover
if "%2"=="backup" goto 9008-backup
exit /b

:DCIM
if "%2"=="recover" goto DCIM-recover
if "%2"=="backup" goto DCIM-backup
exit /b

:DCIM-recover
if "%3"=="noask" goto DCIM-recover-noask
echo %INFO%你要从哪个文件夹进行恢复？%RESET%
call sel folder s %cd%\backup
echo.%info%正在恢复相册...
call adbdevice adb
adb push %sel__folder_path%\*.* /sdcard/DCIM/
if %errorlevel%==0 (
    echo.%info%恢复成功
) else (
    echo.%error%恢复失败
)
pause
exit /b
:DCIM-recover-noask
echo.%info%正在恢复相册...
call adbdevice adb
adb push .\backup\%backupname%\DCIM\* /sdcard/DCIM/
if %errorlevel%==0 (
    echo.%info%恢复成功
) else (
    echo.%error%恢复失败
)
exit /b
:DCIM-backup
if "%3"=="noask" goto DCIM-backup-noask
echo %INFO%你要将相册备份到哪个文件夹内？%RESET%
call sel folder s %cd%\backup
echo.%info%正在备份相册...
call adbdevice adb
adb pull /sdcard/DCIM %sel__folder_path%\
if %errorlevel%==0 (
    echo.%info%备份成功
) else (
    echo.%error%备份失败
)
pause
exit /b
:DCIM-backup-noask
echo.%info%正在备份相册...
call adbdevice adb
for /f "delims=" %%i in ('adb shell getprop ro.product.model') do set model=%%i
set backupname=DCIM_%model%_%RANDOM%%RANDOM%
mkdir .\backup\%backupname%
adb pull /sdcard/DCIM .\backup\%backupname%\
if %errorlevel%==0 (
    echo.%info%备份成功
) else (
    echo.%error%备份失败
)
exit /b


:9008-recover
echo.%info%请选择恢复文件
call sel file s . [zip]
copy /y %sel__file_path% .\backup\tmp.zip
7z x .\backup\tmp.zip -o.\backup\tmp\ -aoa -bsp1
set /p v3=<.\backup\tmp\v3.txt
set /p backupname=<.\backup\tmp\backupname.txt
call edlport
echo.%info%发送引导
if "%v3%"=="1" call QSaharaServer.bat -p \\.\COM%chkdev__edl_port% -s 13:%cd%\EDL\msm8937.mbn else call QSaharaServer.bat -p \\.\COM%chkdev__edl_port% -s 13:%cd%\EDL\msm8909w.mbn
busybox sleep 2
echo.%info%正在恢复...
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --sendxml=%cd%\backup\tmp\rawprogram0.xml --noprompt
rd /Q /S .\backup\tmp >nul 2>nul
del /Q /F .\backup\tmp.zip >nul 2>nul
echo.%info%恢复完成
pause
exit /b

:9008-backup
CLS
call logo
echo %ORANGE%请选择手表型号[暂时仅支持以下型号]%YELLOW%
menu.exe .\menu\backup_9008.xml
set /p MENU=<menutmp.txt
if "%MENU%"=="A" goto MAIN_MENU
if "%MENU%"=="1" set innermodel=I13C&&set v3=0&&goto 9008-backup-run
if "%MENU%"=="2" set innermodel=I13&&set v3=0&&goto 9008-backup-run
if "%MENU%"=="3" set innermodel=I19&&set v3=0&&goto 9008-backup-run
if "%MENU%"=="4" set innermodel=I20&&set v3=1&&goto 9008-backup-run
if "%MENU%"=="5" set innermodel=I25&&set v3=1&&goto 9008-backup-run
if "%MENU%"=="6" set innermodel=I25D&&set v3=1&&goto 9008-backup-run
if "%MENU%"=="7" set innermodel=I32&&set v3=1&&goto 9008-backup-run
if "%MENU%"=="8" set innermodel=ND07&&set v3=1&&goto 9008-backup-run
if "%MENU%"=="9" set innermodel=ND01&&set v3=1&&goto 9008-backup-run
ECHO %ERROR%输入错误，请重新输入！%RESET%
timeout /t 2 >nul
goto 9008-backup

:9008-backup-run
set backupname=EDL_%innermodel%_%RANDOM%%RANDOM%
md .\backup\%backupname%
del /Q /F .\*.img
call edlport
echo.%info%发送引导
if "%v3%"=="1" call QSaharaServer.bat -p \\.\COM%chkdev__edl_port% -s 13:%cd%\EDL\msm8937.mbn else call QSaharaServer.bat -p \\.\COM%chkdev__edl_port% -s 13:%cd%\EDL\msm8909w.mbn
busybox sleep 2
echo.%info%正在备份...
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --sendxml=%cd%\EDL\allxml\%innermodel%.xml --convertprogram2read --noprompt --mainoutputdir="%cd%\backup\%backupname%\"
ECHO.%INFO%重启手表
call qfh_loader.bat --port=\\.\COM%chkdev__edl__port% --memoryname=EMMC --search_path=EDL\ --sendxml=reboot.xml --noprompt
ECHO.%INFO%构建属性文件
copy /y .\*.img .\backup\%backupname%\
set /p="%backupname%" <nul > .\backup\%backupname%\backupname.txt
set /p="%v3%" <nul > .\backup\%backupname%\v3.txt
copy /y .\EDL\allxml\%innermodel%.xml .\backup\%backupname%\rawprogram0.xml
ECHO.%INFO%压缩文件
7z a -tzip -y .\backup\%backupname%.zip .\backup\%backupname%\
rd /Q /S .\backup\%backupname% >nul 2>nul
ECHO.%INFO%备份完成，文件存于bin\backup\%backupname%.zip
pause
exit /b