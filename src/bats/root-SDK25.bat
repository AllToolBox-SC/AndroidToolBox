:ROOT-SDK25

echo %YELLOW%═════════════════════════════%RESET%
ECHO.%ORANGE%请选择一种方案
menu.exe .\menu\root-SDK25.xml
set /p MENU=<menutmp.txt
if "%recorroot%"=="1" goto ROOT-SDK25-1&&set /p="%recorroot%" <nul > recorroot.txt
if "%recorroot%"=="2" goto ROOT-SDK25-2&&set /p="%recorroot%" <nul > recorroot.txt
ECHO %ERROR%输入错误，请重新输入！%RESET%
timeout /t 2 >nul
goto ROOT-SDK25

:ROOT-SDK25-1
ECHO.%INFO%重启您的手表至9008
adb reboot edl

call edlport
ECHO.%INFO%发送引导
call QSaharaServer.bat -p \\.\COM%chkdev__edl_port% -s 13:%cd%\EDL\msm8909w.mbn
busybox sleep 2
ECHO.%INFO%读取boot
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --sendxml=%cd%\EDL\rooting\boot.xml --convertprogram2read --noprompt
move /Y .\boot.img .\tmp\boot.img
if %errorlevel% neq 0 (
   echo %ERROR%移动boot.img文件失败
   ECHO %ERROR%这是一个致命问题，可能数据线连接不稳定，没有成功读取boot%RESET%
   pause
   exit /b
)

ECHO.%INFO%开始修补boot
call magiskpatch 21
if %errorlevel% neq 0 (
   echo %ERROR%修补boot失败，按任意键退出
   pause
   exit /b
)
ECHO.%INFO%解包boot
magiskboot unpack -h boot.img 1>nul 2>nul
ECHO.%INFO%替换adbd
magiskboot.exe cpio ramdisk.cpio "add 0750 sbin/adbd 711_adbd"  1>nul 2>nul
ECHO.%INFO%宽容selinux
patch_boot.exe | find "Suc" 1>nul 2>nul || ECHO %ERROR%patch_boot.exe无法运行,请尝试安装VC运行库合集&&pause&&exit
ECHO.%INFO%打包boot
magiskboot repack boot.img 1>nul 2>nul
ECHO.%INFO%BOOT处理完成!!!
copy /Y new-boot.img EDL\rooting\boot.img > nul
del /Q /F .\tmp\boot.img

ECHO.%INFO%刷入BOOT
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --search_path=EDL\rooting --sendxml=EDL\rooting\boot.xml --noprompt
ECHO.%INFO%boot刷入完毕
ECHO.%INFO%重启手表
call qfh_loader.bat --port=\\.\COM%chkdev__edl__port% --memoryname=EMMC --search_path=EDL\ --sendxml=reboot.xml --noprompt
ECHO.%INFO%坐和放宽，让我们等待您的手表一段时间
goto ROOT-SDK25-wait

:ROOT-SDK25-2
ECHO.%INFO%重启您的手表至9008
adb reboot edl

call edlport
ECHO.%INFO%发送引导
call QSaharaServer.bat -p \\.\COM%chkdev__edl_port% -s 13:%cd%\EDL\msm8909w.mbn
busybox sleep 2
ECHO.%INFO%读取boot
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --sendxml=%cd%\EDL\rooting\boot.xml --convertprogram2read --noprompt
move /Y .\boot.img .\tmp\boot.img
if %errorlevel% neq 0 (
   echo %ERROR%移动boot.img文件失败
   ECHO %ERROR%这是一个致命问题，可能数据线连接不稳定，没有成功读取boot%RESET%
   pause
   exit /b
)

ECHO.%INFO%开始修补boot
call magiskpatch 21
if %errorlevel% neq 0 (
   echo %ERROR%修补boot失败，按任意键退出
   pause
   exit /b
)
ECHO.%INFO%解包boot
magiskboot unpack -h boot.img 1>nul 2>nul
ECHO.%INFO%替换adbd
magiskboot.exe cpio ramdisk.cpio "add 0750 sbin/adbd 711_adbd"  1>nul 2>nul
ECHO.%INFO%宽容selinux
patch_boot.exe | find "Suc" 1>nul 2>nul || ECHO %ERROR%patch_boot.exe无法运行,请尝试安装VC运行库合集&&pause&&exit
ECHO.%INFO%打包boot
magiskboot repack boot.img 1>nul 2>nul
ECHO.%INFO%BOOT处理完成!!!
copy /Y new-boot.img EDL\rooting\recovery.img > nul
del /Q /F .\tmp\boot.img

ECHO.%INFO%刷入BOOT至Recovery分区
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --search_path=EDL\rooting --sendxml=EDL\rooting\recovery.xml --noprompt
ECHO.%INFO%刷入misc
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --search_path=EDL\rooting --sendxml=EDL\rooting\misc.xml --noprompt
ECHO.%INFO%重启手表
call qfh_loader.bat --port=\\.\COM%chkdev__edl__port% --memoryname=EMMC --search_path=EDL\ --sendxml=reboot.xml --noprompt
ECHO.%INFO%坐和放宽，让我们等待您的手表一段时间

:ROOT-SDK25-wait

call boot_completed.bat
ECHO.%INFO%安装管理器
call instapp .\EDL\rooting\manager.apk
ECHO.%INFO%启动管理器[等待五秒]
busybox sleep 5
ECHO.
run_cmd "adb shell am start com.topjohnwu.magisk/a.c"
call adbdevice adb
run_cmd "adb push EDL\rooting\xtcpatch /sdcard/"
call adbdevice adb
run_cmd "adb push EDL\rooting\magiskfile /sdcard/"
ECHO.%INFO%复制运行环境及刷入模块
run_cmd "adb push 2100.sh /sdcard/"
run_cmd "adb shell ""su -c sh /sdcard/2100.sh"""
call instmodule2.bat tmp\xtcpatch.zip

ECHO.%INFO%安装第三方应用商店
call instapp.bat .\apks\appstore.apk
call instapp.bat .\apks\appstore2.apk
ECHO.%INFO%安装第三方安装器
call instapp.bat .\apks\MoyeInstaller.apk
ECHO.%INFO%提示:如果需要在手表上安装应用，请在手表端选择弦-安装器，点击始终
if exist .\recorroot.txt set /p recorroot=<recorroot.txt >nul
if "%recorroot%"=="1" goto ROOT-SDK25-F
ECHO.%INFO%重启您的手表至9008
call adbdevice adb
adb reboot edl

call edlport
ECHO.%INFO%发送引导
call QSaharaServer.bat -p \\.\COM%chkdev__edl_port% -s 13:%cd%\EDL\msm8909w.mbn
ECHO.%INFO%刷入BOOT至Recovery分区
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --search_path=EDL\rooting --sendxml=EDL\rooting\recovery.xml --noprompt
ECHO.%INFO%刷入misc
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --search_path=EDL\rooting --sendxml=EDL\rooting\misc.xml --noprompt
ECHO.%INFO%重启手表
call qfh_loader.bat --port=\\.\COM%chkdev__edl__port% --memoryname=EMMC --search_path=EDL\ --sendxml=reboot.xml --noprompt
:ROOT-SDK25-F

device_check.exe adb qcom_edl&&ECHO.
call boot_completed.bat
ECHO.%INFO%重启手表
adb reboot
device_check.exe adb qcom_edl&&ECHO.
call boot_completed.bat
adb shell magisk -v | find "MAGISK" 1>nul 2>nul || ECHO %ERROR%ROOT失败！发生错误，无法获取magisk，请尝试换方案再次root&&ECHO.%INFO%按任意键返回&&pause&&exit /b
ECHO.%INFO%您的手表已ROOT完毕
del /Q /F .\roottmp.txt >nul 2>nul
ECHO.%INFO%按任意键返回
pause
exit /b