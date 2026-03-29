@echo off
setlocal EnableDelayedExpansion
chcp 936 1>nul 2>nul
CLS
call .\\color.bat
call .\\logo.bat
if "%1"=="1" (
    goto :FASTBOOTROOT
) 
if "%1"=="2" (
    goto :EDLROOT
)
if "%1"=="3" (
    set UNLOCKED=1
    goto :FASTBOOTROOT
)

echo.
echo 安卓一键Root++
echo.

adb.exe start-server 1>nul

:FASTBOOTROOT
device_check.exe adb fastboot & echo.
busybox.exe sleep 2

for /f "delims=" %%i in ('type tmp.txt') do set devicestatus=%%i
if "%devicestatus%"=="adb" (
    echo %INFO%5秒钟后再次检查
    device_check.exe adb 1>nul 2>nul
    busybox.exe sleep 2
    for /f "delims=" %%i in ('type tmp.txt') do set devicestatus=%%i
    if "%devicestatus%" NEQ "adb" (
        echo %ERROR%设备已断开连接，请重新连接设备并重试！
        busybox.exe sleep 2
        call %0 3
    )
    echo %INFO%设备已连接，检测是否Root...
    adb.exe root 1>nul 2>nul
    if errorlevel 1 (
        adb.exe shell su -v 1>nul 2>nul
        if errorlevel 1 (
            set ROOTED=0
        ) else (
            set ROOTED=1
        )
    ) else (
        set ROOTED=2
    )

    if !ROOTED! EQU 1 (
        echo %INFO%寻找boot,init_boot分区...
        adb shell su -c "dd if=/dev/block/bootdevice/by-name/init_boot of=/sdcard/init_boot.img bs=2048" 1>nul 2>nul
        if errorlevel 1 adb shell su -c "dd if=/dev/block/by-name/init_boot of=/sdcard/init_boot.img bs=2048" 1>nul 2>nul
        adb pull /sdcard/init_boot.img .\tmp\init_boot.img 1>nul 2>nul
        if errorlevel 1 (
            set NOINITBOOT=1
            adb shell su -c "dd if=/dev/block/bootdevice/by-name/boot of=/sdcard/boot.img bs=2048" 1>nul 2>nul
            if errorlevel 1 adb shell su -c "dd if=/dev/block/by-name/boot of=/sdcard/boot.img bs=2048" 1>nul 2>nul
            adb pull /sdcard/boot.img .\tmp\boot.img 1>nul 2>nul
            if errorlevel 1 (
                echo %WARN%提取失败，跳过此步骤...
                set NOPULL=1
            )
        )
    ) else if !ROOTED! EQU 2 (
        echo %INFO%寻找boot,init_boot分区...
        adb shell dd if=/dev/block/bootdevice/by-name/init_boot of=/sdcard/init_boot.img bs=2048 1>nul 2>nul
        if errorlevel 1 adb shell dd if=/dev/block/by-name/init_boot of=/sdcard/init_boot.img bs=2048 1>nul 2>nul
        adb pull /sdcard/init_boot.img .\tmp\init_boot.img 1>nul 2>nul
        if errorlevel 1 (
            set NOINITBOOT=1
            adb shell dd if=/dev/block/bootdevice/by-name/boot of=/sdcard/boot.img bs=2048 1>nul 2>nul
            if errorlevel 1 adb shell dd if=/dev/block/by-name/boot of=/sdcard/boot.img bs=2048 1>nul 2>nul
            adb pull /sdcard/boot.img .\tmp\boot.img 1>nul 2>nul
            if errorlevel 1 (
                echo %WARN%提取失败，跳过此步骤...
                set NOPULL=1
            )
        )
    ) else (
        echo %INFO%您的设备没有Root权限
        set NOPULL=1
    )
    if DEFINED NOPULL (
        set /p INITBOOT="%INFO%是否修补init_boot(y):"
        if /I "!INITBOOT!"=="y" ( set INITBOOTV=1 ) else ( set INITBOOTV=0 )
        call .\\sel.bat file s . 1>nul
        for /f "delims=" %%i in ('type tmp\\output.txt') do set selfile=%%i
        set bootpath=!selfile!

        if !INITBOOTV! EQU 1 (
            copy /y !bootpath! .\tmp\init_boot.img 1>nul 2>nul
            set bootpath=.\tmp\init_boot.img
        ) else (
            copy /y !bootpath! .\tmp\boot.img 1>nul 2>nul
            set bootpath=.\tmp\boot.img
        )
        
    ) else (
        if EXIST .\tmp\init_boot.img (
            set bootpath=.\tmp\init_boot.img
        ) else (
            set bootpath=.\tmp\boot.img
        )
    )

    if !INITBOOTV! EQU 1 (
        echo %INFO%正在修补init_boot分区镜像...
        call run_cmd "magiskpatcher.exe magisk64.apk .\\tmp\\init_boot.img -cpu=arm_64 -out=init_boot.img"
    ) else (
        echo %INFO%正在修补boot分区镜像...
        call run_cmd "magiskpatcher.exe magisk64.apk .\\tmp\\boot.img -cpu=arm_64 -out=boot.img"
    )
    echo %INFO%重启至Bootloader模式...
    adb.exe reboot bootloader 1>nul 2>nul
    device_check.exe fastboot 1>nul 2>nul
    busybox.exe sleep 2
    
    echo %INFO%正在刷入修补后的镜像...
    if !INITBOOTV! EQU 1 (
        fastboot.exe flash init_boot init_boot.img
    ) else (
        fastboot.exe flash boot boot.img
    )
    fastboot.exe reboot 1>nul 2>nul
    echo %SUCCESS%- 跨越山海 终见曙光 -
    echo %SUCCESS%您的设备已成功Root！
    pause>nul
    exit /b

)

if "%devicestatus%"=="fastboot" (
    
    set /p INITBOOT="%INFO%是否修补init_boot(y):"
    if /I "!INITBOOT!"=="y" ( set INITBOOTV=1 ) else ( set INITBOOTV=0 )
    call .\\sel.bat file s . 1>nul
    for /f "delims=" %%i in ('type tmp\\output.txt') do set selfile=%%i
    set bootpath=!selfile!

    if !INITBOOTV! EQU 1 (
        copy /y !bootpath! .\tmp\init_boot.img 1>nul
        set bootpath=.\tmp\init_boot.img
    ) else (
        copy /y !bootpath! .\tmp\boot.img 1>nul
        set bootpath=.\tmp\boot.img
    )
    if !INITBOOTV! EQU 1 (
        echo %INFO%正在修补init_boot分区镜像...
        call run_cmd "magiskpatcher.exe magisk64.apk .\\tmp\\init_boot.img -cpu=arm_64 -out=init_boot.img"
    ) else (
        echo %INFO%正在修补boot分区镜像...
        call run_cmd "magiskpatcher.exe magisk64.apk .\\tmp\\boot.img -cpu=arm_64 -out=boot.img"
    )
    echo %INFO%正在刷入修补后的镜像...
    if !INITBOOTV! EQU 1 (
        fastboot.exe flash init_boot init_boot.img
    ) else (
        fastboot.exe flash boot boot.img
    )
    fastboot.exe reboot 1>nul 2>nul
    echo %SUCCESS%- 跨越山海 终见曙光 -
    echo %SUCCESS%您的设备已成功Root！
    pause>nul
    exit /b
)