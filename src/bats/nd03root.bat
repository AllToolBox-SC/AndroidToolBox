echo.%info%免责声明：
echo.%info%由于nd03无法简单打开adb，无法判断你的机型和版本号，所以请你自行确认，如果机型和版本号不匹配，那么可能会变砖
echo.%info%Root后版本均为2.8.1，没有录屏等功能
echo.%info%该Root文件网络搜集而来，无任何限制
echo.%info%正在为你准备开始
del /Q /F tmp.txt >nul 2>nul
del /Q /F .\*.img >nul 2>nul
del /Q /F .\tmp\boot.img >nul 2>nul
del /Q /F .\header >nul 2>nul
del /Q /F .\kernel_dtb >nul 2>nul
del /Q /F .\kernel >nul 2>nul
del /Q /F .\ramdisk.cpio >nul 2>nul
del /Q /F .\port_trace.txt >nul 2>nul
del /Q /F .\EDL\rooting\*.* >nul 2>nul
rd /Q /S .\EDL\rooting\xtcpatch >nul 2>nul
rd /Q /S .\EDL\rooting\magiskfile >nul 2>nul
md .\EDL\rooting 1>nul 2>>nul
:existinfo
ECHO %INFO%正在为您从本地拷贝文件
copy /Y "%cd%\EDL\%innermodel%.zip" "%cd%\EDL\rooting\root.zip"
if %errorlevel% neq 0 (
   ECHO %ERROR%找不到文件
   ECHO %INFO%按任意键退出
   pause >nul
)

ECHO %INFO%开始解压文件
7z x EDL\rooting\root.zip -o.\EDL\rooting -aoa >nul 2>&1
if %errorlevel% neq 0 (
   ECHO %ERROR%解压文件时出现错误，错误值:%errorlevel%
   ECHO %INFO%按任意键退出
   pause >nul
   exit /b
)
echo.%GREEN_2%════════════════════════
ECHO %INFO%准备完成，即将开始root
echo.%info%等待9008设备连接...
device_check.exe qcom_edl&&ECHO.
ECHO.%INFO%获取9008端口
call edlport
ECHO.%INFO%发送引导
call QSaharaServer.bat -p \\.\COM%chkdev__edl_port% -s 13:%cd%\EDL\prog_firehose_ddr.elf
busybox sleep 2
ECHO.%INFO%批量刷入2.8.1完整固件
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --search_path=EDL\rooting --sendxml=EDL\rooting\281\rawprogram0.xml --noprompt
ECHO.%INFO%批量刷入分区super，abl，vbmeta，vbmeta_system
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --search_path=EDL\rooting --sendxml=EDL\rooting\rawprogram0.xml --noprompt
copy /Y "%cd%\tmp\eboot.img" "%cd%\EDL\rooting\eboot.img"
ECHO.%INFO%清除boot
call fh_loader.bat --port=\\.\COM%chkdev__edl_port% --memoryname=EMMC --search_path=EDL\rooting --sendxml=EDL\rooting\eboot.xml --noprompt
ECHO.%INFO%重启手表
call qfh_loader.bat --port=\\.\COM%chkdev__edl__port% --memoryname=EMMC --search_path=EDL\ --sendxml=reboot.xml --noprompt
ECHO.%WARN%你的手表没有变砖!
ECHO.%WARN%你的手表没有变砖!
ECHO.%WARN%你的手表没有变砖!
ECHO.%WARN%不是进入fastboot就是变砖！
ECHO.%INFO%等待fastboot连接...
device_check.exe fastboot&&ECHO.
::ECHO.%INFO%格式化userdata
::run_cmd "fastboot erase userdata"
ECHO.%INFO%刷入boot
run_cmd "fastboot flash boot EDL\rooting\boot.img"
ECHO.%INFO%重启并进入recovery
run_cmd "fastboot boot EDL\rooting\recovery.img"
ECHO.%INFO%坐和放宽，让我们等待您的手表一段时间
ECHO.%INFO%进入sideload
call adbdevice.bat sideload
ECHO.%INFO%正在刷dm，请等待20秒...
busybox timeout 20 cmd /c adb sideload .\EDL\rooting\Dm.zip
ECHO.
call adbdevice.bat noadb
ECHO.═══════════════════════
ECHO.%INFO%坐和放宽，让我们等待一会
busybox sleep 200
ECHO.%INFO%请确认已经重启三次后，按任意键继续
pause >nul
ECHO.%INFO%检查是否有adb
busybox timeout 20 cmd /c call adbdevice adb
if "%errorlevel%"=="0" goto findadb
ECHO.%INFO%未发现adb连接，请短接进入9008
ECHO.%INFO%等待9008设备连接...
device_check.exe qcom_edl >nul
ECHO.
call qmmi z10
device_check.exe adb&&ECHO.
call boot_completed.bat
busybox sleep 5
ECHO.%INFO%擦除misc并重启
run_cmd "adb reboot bootloader"
device_check.exe adb fastboot&&ECHO.
for /f "delims=" %%i in ('type tmp.txt') do set devicestatus=%%i
if "!devicestatus!"=="adb" (
    run_cmd "adb reboot bootloader"
)
run_cmd "fastboot erase misc"
run_cmd "fastboot reboot"
device_check.exe adb&&ECHO.
call boot_completed.bat
busybox sleep 5
ECHO.%INFO%稍等片刻，即将开始
:findadb
device_check.exe adb&&ECHO.
call boot_completed.bat
run_cmd "adb shell wm density 280"
ECHO.%INFO%正在启动投屏！如手表端不方便操作，可在电脑端进行操作
ECHO.%INFO%提示：如果手表息屏，在投屏窗口单击右键即可
start scrcpy-noconsole.vbs
ECHO.%INFO%根据小天才引导界面提示，依次点击
ECHO.%INFO%进入表盘界面后按任意键继续
:magisk
pause
ECHO.%INFO%3秒后申请权限，请在手表上选择允许
busybox sleep 3
adb shell "su -c magisk -v" || echo.%ERROR%授予出错，请重新授予&&goto magisk
ECHO.%ORANGE%--------------------------------------------------------------------
ECHO.%PINK%-把时间交给我们-
device_check.exe adb&&ECHO.
ECHO.%INFO%开始安装XTC Patch模块
adb push tmp\xtcpatch.zip /sdcard/xtcpatch.zip
adb shell "su -c magisk --install-module /sdcard/xtcpatch.zip"
run_cmd "adb shell ""rm -rf /sdcard/xtcpatch.zip"""
ECHO.%INFO%安装XTC Patch模块成功
run_cmd "adb shell pm clear com.android.packageinstaller"
ECHO.%INFO%开始安装LSP和核心破解
call instapp.bat .\apks\LSP.apk
call instapp.bat .\apks\toolkit_4.8.apk
run_cmd "adb shell ""su -c am start -n org.lsposed.manager/.ui.activity.MainActivity"""
ECHO.%INFO%请点击xtcpatch和核心破解，并勾选作用域系统框架，然后按任意键继续
pause
run_cmd "adb shell wm density reset"
run_cmd "adb reboot"
device_check.exe adb&&ECHO.
call boot_completed.bat
ECHO.%INFO%开始安装重要预装应用,共计5个[请勿跳过]
call instapp.bat .\apks\123700.apk
busybox sleep 20
device_check.exe adb&&ECHO.
call boot_completed.bat
call instapp.bat .\apks\wxzf.apk
call instapp.bat .\apks\personalcenter-ND03.apk
call instapp.bat .\rootproapks\MTfile.apk
call instapp.bat .\apks\appsettings-ND03.apk
ECHO.%INFO%开始安装预装应用,共计3个
set /p noapp=%YELLOW%如需跳过在10秒内输入no[不推荐]:%RESET%
if "%noapp%"=="no" goto ROOT-noapp
call instapp.bat .\apks\appstore.apk
call instapp.bat .\apks\appstore2.apk
call instapp.bat .\apks\appstore3.apk
ECHO.%INFO%预装应用安装完成
:ROOT-noapp
ECHO.%INFO%正在执行提前编译，可能需要一些时间
run_cmd "adb shell cmd package compile -m everything-profile -f com.xtc.i3launcher"
run_cmd "adb shell cmd package compile -m everything-profile -f com.xtc.setting"
ECHO.%GRAY%-跨越山海 终见曙光-
ECHO.%INFO%您的手表已ROOT完毕
pause
exit /b