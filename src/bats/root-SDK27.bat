set /p DCIMyn=%YELLOW%要备份相册吗？[在10秒内输入y备份]%RESET%
del /Q /F .\DCIMyn.txt >nul 2>nul
del /Q /F .\backupname.txt >nul 2>nul
if "%DCIMyn%"=="y" call backup DCIM backup noask&&set /p="%backupname%" <nul > backupname.txt&&set /p="%DCIMyn%" <nul > DCIMyn.txt
if "%DCIMyn%"=="yes" call backup DCIM backup noask&&set /p="%backupname%" <nul > backupname.txt&&set /p="%DCIMyn%" <nul > DCIMyn.txt

ECHO.%INFO%重启您的手表至9008
adb reboot edl
call edlport

ECHO.%INFO%发送引导
call QSaharaServer.bat -p \\.\COM%chkdev__edl_port% -s 13:%cd%\EDL\msm8937.mbn
busybox sleep 2
ECHO.%INFO%读取boot
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --sendxml=%cd%\EDL\rooting\boot.xml --convertprogram2read --noprompt
move /Y .\boot.img .\tmp\boot.img 1>nul 2>nul
if %errorlevel% neq 0 (
   echo %ERROR%移动boot.img文件失败
   ECHO %ERROR%这是一个致命问题，可能数据线连接不稳定，没有成功读取boot%RESET%
   pause
   exit /b
)

busybox sleep 1
ECHO.%INFO%开始修补boot
if exist .\innermodel.txt set /p innermodel=<innermodel.txt >nul
call magiskpatch 25
if %errorlevel% neq 0 (
   echo %ERROR%修补boot失败，按任意键退出
   pause
   exit /b
)
ECHO.%INFO%解包boot
magiskboot unpack -h boot.img 1>nul 2>nul
ECHO.%INFO%替换adbd
magiskboot.exe cpio ramdisk.cpio "add 0750 sbin/adbd 810_adbd"  1>nul 2>nul
magiskboot.exe cpio ramdisk.cpio "add 0750 overlay.d/xse.rc xse.rc"  1>nul 2>nul
ECHO.%INFO%宽容selinux
patch_boot.exe | find "Suc" 1>nul 2>nul || ECHO %ERROR%patch_boot.exe无法运行,请尝试安装VC运行库合集&&pause&&exit
ECHO.%INFO%打包boot
magiskboot repack boot.img 1>nul 2>nul
ECHO.%INFO%BOOT处理完成!!!
copy /Y new-boot.img EDL\rooting\sboot.img > nul
del /Q /F .\tmp\boot.img
if exist .\smodel.txt set /p smodel=<smodel.txt >nul
if "%smodel%"=="1" (
ECHO.%INFO%刷入recovery
copy EDL\rooting\sboot.img EDL\rooting\recovery.img > nul
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --search_path=EDL\rooting --sendxml=EDL\rooting\recovery.xml --noprompt
ECHO.%INFO%刷入boot，aboot，userdata，misc
) else (
ECHO.%INFO%刷入recovery，aboot
)
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --search_path=EDL\rooting --sendxml=EDL\rooting\rawprogram0.xml --noprompt

if exist .\nouserdata.txt set /p nouserdata=<nouserdata.txt >nul
if "%nouserdata%"=="1" (
ECHO.%INFO%重启手表
call qfh_loader.bat --port=\\.\COM%chkdev__edl__port% --memoryname=EMMC --search_path=EDL\ --sendxml=reboot.xml --noprompt
ECHO.%INFO%你选择了不刷userdata，不再继续
ECHO.按任意键返回...
pause >nul
exit /b
)
if exist .\smodel.txt set /p smodel=<smodel.txt >nul
if "%smodel%"=="1" (
ECHO.%INFO%重启手表
call qfh_loader.bat --port=\\.\COM%chkdev__edl__port% --memoryname=EMMC --search_path=EDL\ --sendxml=reboot.xml --noprompt
goto ROOT-SDK27-WAIT
)
ECHO.%INFO%擦除boot
copy /Y tmp\eboot.img tmp\boot.img > nul
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --search_path=tmp --sendxml=EDL\rooting\boot.xml --noprompt
ECHO.%INFO%重启手表
call qfh_loader.bat --port=\\.\COM%chkdev__edl__port% --memoryname=EMMC --search_path=EDL\ --sendxml=reboot.xml --noprompt
ECHO.%INFO%等待开机

device_check.exe fastboot&&ECHO.
ECHO.%WARN%你的手表没有变砖!
ECHO.%WARN%你的手表没有变砖!
ECHO.%WARN%你的手表没有变砖!
ECHO.%WARN%不是进入fastboot就是变砖！
ECHO.%INFO%刷入boot
run_cmd "fastboot flash boot new-boot.img"
ECHO.%INFO%刷入userdata
run_cmd "fastboot flash userdata tmp\userdata.img"
echo ffbm-02 > misc.bin
run_cmd "fastboot flash misc misc.bin"
run_cmd "fastboot reboot"
ECHO.%INFO%坐和放宽，让我们等待您的手表一段时间
device_check.exe adb fastboot&&ECHO.
for /f "delims=" %%i in ('type tmp.txt') do set devicestatus=%%i
if not "%devicestatus%"=="adb" (
ECHO.%ERROR%您的设备可能触发了Xse限制！请重新进行root
ECHO.%ERROR%按任意键返回...
pause > nul
exit /b
)
ECHO.%INFO%稍等片刻，即将开始
call boot_completed.bat
ECHO.%WARN%工具未做出提示不要在手表上点任何内容！！
ECHO.%WARN%请 不要 点击重启-重启并进入正常启动模式
ECHO.%WARN%请 不要 点击重启-重启并进入正常启动模式

:ROOT-SDK27-WAIT
if exist .\smodel.txt set /p smodel=<smodel.txt >nul
if "%smodel%"=="1" (
ECHO.%INFO%坐和放宽，让我们等待您的手表一段时间
device_check.exe adb qcom_edl&&ECHO.
ECHO.%INFO%稍等片刻，即将开始
call boot_completed.bat
busybox sleep 15
call instapp .\apks\54850.apk
)
adb reboot
device_check.exe adb qcom_edl fastboot&&ECHO.
for /f "delims=" %%i in ('type tmp.txt') do set devicestatus=%%i
if not "%devicestatus%"=="adb" (
ECHO.%ERROR%您的设备可能触发了Xse限制！请重新进行root
ECHO.%ERROR%按任意键返回...
pause > nul
exit /b
)

call boot_completed.bat
adb shell pm path com.android.systemui > nul 2> nul
if %errorlevel%==0 (
    set havesystemui=1
    del /Q /F .\havesystemui.txt >nul 2>nul
    set /p="1" <nul > havesystemui.txt
    ECHO %GREEN%系统存在SystemUI
) else (
    set havesystemui=0
    del /Q /F .\havesystemui.txt >nul 2>nul
    set /p="0" <nul > havesystemui.txt
    ECHO %GREEN%系统不存在SystemUI
)
ECHO.%WARN%请一定要根据工具的提示来，root未完成前禁止联网，禁止重复绑定！
run_cmd "adb shell setprop persist.sys.charge.usable true"
ECHO.%INFO%充电可用已开启
run_cmd "adb shell dumpsys battery unplug"
ECHO.%INFO%已模拟未充电状态
run_cmd "adb shell svc wifi disable"
run_cmd "adb shell wm density 200"
call automagisk.bat
:Edxposed
ECHO.%INFO%正在自动打开Edxposed Installer，请稍后
device_check.exe adb qcom_edl&&ECHO.
run_cmd "adb shell am start -n com.solohsu.android.edxp.manager/de.robv.android.xposed.installer.WelcomeActivity"
busybox sleep 7
call autosystemplus.bat
call adbdevice adb
adb shell "dumpsys package com.solohsu.android.edxp.manager | grep userId=" >useridtmp
call number useridtmp chown

ECHO.%INFO%正在修改文件/data/user_de/0/com.solohsu.android.edxp.manager/conf/enabled_modules.list的所有者
adb shell "su -c chown %chown% /data/user_de/0/com.solohsu.android.edxp.manager/conf/enabled_modules.list"

ECHO.%INFO%正在修改文件/data/user_de/0/com.solohsu.android.edxp.manager/conf/modules.list的所有者
adb shell "su -c chown %chown% /data/user_de/0/com.solohsu.android.edxp.manager/conf/modules.list"
ECHO.%INFO%稍等片刻，即将开始
CLS

call logo
ECHO.%ORANGE%--------------------------------------------------------------------
ECHO.%PINK%-把时间交给我们-
ECHO.%INFO%开始安装XTC Patch模块
adb shell setprop persist.sys.ez true
adb push tmp\xtcpatch.zip /sdcard/xtcpatch.zip
adb shell setprop persist.sys.rooting true
adb shell "su -c magisk --install-module /sdcard/xtcpatch.zip"
run_cmd "adb shell setprop persist.sys.rooting false"
run_cmd "adb shell ""rm -rf /sdcard/xtcpatch.zip"""
ECHO.%INFO%安装XTC Patch模块成功
run_cmd "adb shell wm density reset"
run_cmd "adb shell pm clear com.android.packageinstaller"
if exist .\havesystemui.txt set /p havesystemui=<havesystemui.txt >nul
if "%havesystemui%"=="1" (
  ECHO.%INFO%开始安装XTC Patch_SystemUI模块
  adb push tmp\systemui.zip /sdcard/systemui.zip
  adb shell "su -c magisk --install-module /sdcard/systemui.zip"
  adb shell rm -rf /sdcard/systemui.zip
  ECHO.%INFO%开始安装XTC Patch_SystemUI模块
)
ECHO.%INFO%重启手表
run_cmd "adb reboot"

device_check.exe adb qcom_edl&&ECHO.
ECHO.%INFO%坐和放宽，让我们等待您的手表一段时间
call boot_completed.bat
if exist .\smodel.txt set /p smodel=<smodel.txt >nul
if "%smodel%"=="1" (
busybox sleep 5
adb shell "su -c sh /data/adb/modules/XTCPatch/active_module.sh com.huanli233.systemplus"
adb shell "su -c sh /data/adb/modules/XTCPatch/active_module.sh com.zcg.xtcpatch"
adb reboot
device_check.exe adb qcom_edl&&ECHO.
call boot_completed.bat
busybox sleep 5
)
busybox sleep 10
ECHO.%INFO%稍等片刻...
adb reboot
call boot_completed.bat
ECHO.%INFO%开始安装系统应用[请勿跳过]
if exist .\havesystemui.txt set /p havesystemui=<havesystemui.txt >nul
if exist .\isv3.txt set /p isv3=<isv3.txt >nul
if "%isv3%"=="1" (
    if "%havesystemui%"=="1" (
        call instapp.bat .\apks\130510.apk
    ) else (
        call instapp.bat .\apks\121750.apk
    )
) else (
    call instapp.bat .\apks\116100.apk
)
ECHO.%INFO%系统应用安装完成
if exist .\havesystemui.txt set /p havesystemui=<havesystemui.txt >nul
if exist .\smodel.txt set /p smodel=<smodel.txt >nul
if "%smodel%"=="1" (
ECHO.%INFO%重启手表
run_cmd "adb reboot"
) else (
    if "%havesystemui%"=="1" run_cmd "adb shell pm enable com.android.systemui"
    ECHO.%INFO%擦除misc并重启
    run_cmd "adb reboot bootloader"
    device_check.exe adb fastboot&&ECHO.
    for /f "delims=" %%i in ('type tmp.txt') do set devicestatus=%%i
    if "!devicestatus!"=="adb" run_cmd "adb reboot bootloader"
    run_cmd "fastboot erase misc"
    run_cmd "fastboot reboot"
)

device_check.exe adb qcom_edl&&ECHO.
call boot_completed.bat
busybox sleep 5
ECHO.%INFO%开始安装重要预装应用,共计6个[请勿跳过]
call instapp.bat .\apks\selftest.apk
call instapp.bat .\apks\settings.apk
call instapp.bat .\apks\wxzf.apk
call instapp.bat .\apks\MoyeInstaller.apk
call instapp.bat .\apks\appsettings.apk
call instapp.bat .\apks\personalcenter.apk
ECHO.%INFO%开始安装预装应用,共计7个
set /p noapp=%YELLOW%如需跳过在10秒内输入no[不推荐]:%RESET%
if "%noapp%"=="no" goto ROOT-SDK27-noapp
call instapp.bat .\apks\wcp2.apk
call instapp.bat .\apks\appstore.apk
call instapp.bat .\apks\appstore2.apk
call instapp.bat .\apks\appstore3.apk
call instapp.bat .\apks\appmanager.apk
call instapp.bat .\apks\weichat.apk
ECHO.%INFO%预装应用安装完成
:ROOT-SDK27-noapp
ECHO.%INFO%使用提示:当手表进入长续航模式、睡眠模式等禁用模式时，可下滑点击手电筒打开小天才启动器，即可绕过禁用模式
ECHO.%INFO%使用提示:你可以在/sdcard/hidden_app_list.txt中填写包名以实现隐藏应用
ECHO.%INFO%正在执行提前编译，可能需要一些时间
run_cmd "adb shell cmd package compile -m everything-profile -f com.xtc.i3launcher"
run_cmd "adb shell cmd package compile -m everything-profile -f com.xtc.setting"
ECHO.%WARN%请永远不要卸载SystemPlus和XTCPatch，否则手表无法开机
ECHO.%WARN%请永远不要卸载SystemPlus和XTCPatch，否则手表无法开机
ECHO.%WARN%请永远不要卸载SystemPlus和XTCPatch，否则手表无法开机
ECHO.%GRAY%-跨越山海 终见曙光-
ECHO.%INFO%提示:如果需要在手表上安装应用，请在手表端选择弦-安装器，点击始终
ECHO.%INFO%您的手表已ROOT完毕
if exist .\backupnametxt set /p backupname=<backupnametxt
if exist .\DCIMyn.txt set /p DCIMyn=<DCIMyn.txt
if "%DCIMyn%"=="y" call backup DCIM recover noask
if "%DCIMyn%"=="yes" call backup DCIM recover noask
ECHO.%YELLOW%是否进行预装优化[包括模块和应用，期间需要多次选择]？
set /p rootpro=%YELLOW%输入y进行优化，按任意键退出%RESET%
if /i "%rootpro%"=="y" call rootpro
del /Q /F .\roottmp.txt >nul 2>nul
exit /b