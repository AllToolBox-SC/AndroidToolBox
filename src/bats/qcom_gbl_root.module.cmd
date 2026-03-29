:_MODULE_MAIN
setlocal enabledelayedexpansion
    cls
    call .\logo.bat
    echo %INFO%点击超级用户，找到Shell并授予权限%RESET%
    call scrcpy.exe
    busybox.exe sleep 1
    adb.exe disconnect
    busybox.exe sleep 2
    echo %INFO%若一直显示等待连接中请重新插拔设备%RESET%
    device_check.exe adb
    echo %INFO%复制efisp%RESET%
    adb push gbl_efi_unlock.efi /data/local/tmp/gbl_efi_unlock.efi 1>nul 2>nul
    echo %INFO%刷入efisp%RESET%
    adb shell su -c "dd if=/data/local/tmp/gbl_efi_unlock.efi of=/dev/block/by-name/efisp" 1>nul
        if not !errorlevel! == 0 (
            echo %ERROR%刷入失败%RESET%
            pause
            goto :EOF
        )
    echo %INFO%重启设备到Bootloader%RESET%
    adb reboot bootloader
    device_check.exe fastboot
    echo %INFO%清除数据%RESET%
    fastboot.exe format userdata
    fastboot.exe format cache
    fastboot.exe reboot
    echo %SUCCESS%解锁成功%RESET%
    goto :EOF
endlocal
goto :EOF
