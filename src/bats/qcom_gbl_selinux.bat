@echo off
cls
doskey #INCLUDE=call :#INCLUDE

:_START
setlocal enabledelayedexpansion
    call .\color.bat

    echo %YELLOW%════════════════════════%RESET%
    echo %GREEN_2%粗粮%ORANGE%宽容SELinux%RESET% ^& %YELLOW%强开BL%RESET%
    echo %YELLOW%════════════════════════%RESET%
    echo %GREEN_2%本工具可以在部分高通设备上（安全补丁在2026.2.1之前）临时宽容SELinux%RESET%
    echo %WARN%请确保已打开USB调试、USB调试（安全设置）、USB安装应用、OEM解锁（可选）%RESET%
    echo %WARN%解锁功能仅适用于骁龙8Egen5机型%RESET%
    ECHO.
    echo %INFO%请按任意键继续%RESET%
    pause >nul

    device_check.exe adb fastboot && ECHO.
    for /f "delims=" %%i in ('type tmp.txt') do set devicestatus=%%i
    if not "%devicestatus%"=="fastboot" (
        echo %INFO%重启设备至Bootloader%RESET%
        adb.exe reboot bootloader
        device_check.exe fastboot && ECHO.
    )
    busybox.exe sleep 2

    echo %INFO%开始注入cmdline%RESET%
    run_cmd "fastboot oem set-gpu-preemption 0 androidboot.selinux=permissive"
        if not !errorlevel! == 0 goto :FATAL
    echo %INFO%注入完成，正在引导%RESET%
    fastboot.exe continue 1>nul 2>nul
    
    echo %INFO%坐和放宽，把时间交给我们%RESET%
    device_check.exe adb 1>nul 2>nul
    busybox sleep 3
    call .\boot_completed.bat
    echo %INFO%解锁手机后按任意键继续%RESET%
    pause >nul

    echo %INFO%开始安装KernelSU%RESET%
    echo %WARN%若弹出安装提示请允许%RESET%
    run_cmd "adb.exe install -r -d apks\kernelsu-ci.apk"
    busybox.exe sleep 2
    adb.exe shell "am start -n me.weishu.kernelsu/.ui.MainActivity" 1>nul
    echo %INFO%即将打开投屏，现在请点击屏幕上的“越狱”按钮，等待大约1分钟左右，等到越狱按钮消失时关闭投屏窗口%RESET%
    call scrcpy.exe
    echo %INFO%等待1秒%RESET%
    busybox.exe sleep 1
    adb.exe shell "am force-stop me.weishu.kernelsu" 1>nul 2>nul
        if not !errorlevel! == 0 (
            echo %INFO%请在多任务界面退出KernelSU后按任意键继续%RESET%
            pause >nul
            busybox.exe sleep 2
        )
    adb.exe shell "am start -n me.weishu.kernelsu/.ui.MainActivity" 1>nul
    echo %SUCCESS%设备获取临时Root成功%RESET%
    set /p _="%INFO%是否要执行解锁操作（警告：该操作可能会让不兼容的机型永久变砖）[yes]: "
    if "!_!" == "yes" (
#INCLUDE qcom_gbl_root
        goto :EOF
    )
endlocal
goto :EOF


:FATAL
setlocal
    echo %ERROR%cmdline注入失败%RESET%
    pause
endlocal
goto :EOF

:#INCLUDE
    set MODULE=%1.module.cmd
    call %MODULE%
goto :EOF
