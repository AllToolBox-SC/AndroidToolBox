::call magiskpatch 25/21
@echo off
:MAGISKPATCH
SETLOCAL
set magiskver=%1
set bootpath=%cd%\tmp\boot.img& set outputpath=%cd%\boot.img
::检查是否存在
if not exist %bootpath% goto FATAL
::清理临时文件目录
del /Q /F .\magiskinit >nul 2>nul
del /Q /F .\header >nul 2>nul
del /Q /F .\kernel_dtb >nul 2>nul
del /Q /F .\kernel >nul 2>nul
del /Q /F .\ramdisk.cpio >nul 2>nul
del /Q /F .\config >nul 2>nul
del /Q /F .\ramdisk.cpio.orig >nul 2>nul
copy /y .\magiskinit%magiskver% .\magiskinit 1>nul 2>nul
::设置修补选项
::- 保留AVB2.0, dm-verity (27006-17000) 建议默认true
set KEEPVERITY=true
::- 保留强制加密 (27006-17000) 建议默认true
set KEEPFORCEENCRYPT=true
::- 修补vbmeta标志 (27006-24000) 默认false
set PATCHVBMETAFLAG=false
::- 安装到Recovery (27006-19100) 默认false (注: 在23000及更低的版本, 当boot解压出现recovery_dtbo文件时, 此项将被强制设为true)
set RECOVERYMODE=false
::- 强开rootfs (27006-26000) 建议默认true
set LEGACYSAR=true
::- 处理器架构 (arm和x86系列中, 27006-19000支持64位, 18100-17000不区分或不支持64位. 27006-27005支持riscv_64) 建议默认arm64
::  arm_64   arm_32   x86_64   x86_32   riscv_64
set arch=arm_32
goto MAGISKPATCH-%magiskver%200
:MAGISKPATCH-25200
::检查Magisk组件
if not exist .\magiskinit goto FATAL
if not exist .\magisk32.xz goto FATAL
::解包boot
magiskboot.exe unpack -h %bootpath% 1>nul 2>nul
::测试ramdisk
if not exist .\ramdisk.cpio (
    set STATUS=0& goto MAGISKPATCH-25200-MODE0)
magiskboot.exe cpio .\ramdisk.cpio test 1>nul 2>nul
set STATUS=%errorlevel%
if "%STATUS%"=="0" goto MAGISKPATCH-25200-MODE0
if "%STATUS%"=="1" goto MAGISKPATCH-25200-MODE1
goto FATAL
::模式0-Stock boot image detected
:MAGISKPATCH-25200-MODE0
set SHA1=
for /f %%a in ('magiskboot sha1 %bootpath% 1^>nul 2^>nul') do set SHA1=%%a
if exist .\ramdisk.cpio (
    copy /Y .\ramdisk.cpio .\ramdisk.cpio.orig 1>nul 2>nul)
goto MAGISKPATCH-25200-2
::模式1-Magisk patched boot image detected
:MAGISKPATCH-25200-MODE1
set SHA1=
for /f %%a in ('magiskboot cpio ramdisk.cpio sha1 1^>nul 2^>nul') do set SHA1=%%a
magiskboot.exe cpio ramdisk.cpio restore 1>nul 2>nul
copy /Y .\ramdisk.cpio .\ramdisk.cpio.orig 1>nul 2>nul
goto MAGISKPATCH-25200-2
:MAGISKPATCH-25200-2
::修补ramdisk.cpio
echo.KEEPVERITY=%KEEPVERITY%>config& echo.KEEPFORCEENCRYPT=%KEEPFORCEENCRYPT%>>config& echo.PATCHVBMETAFLAG=%PATCHVBMETAFLAG%>>config& echo.RECOVERYMODE=%RECOVERYMODE%>>config
if not "%SHA1%"=="" echo.SHA1=%SHA1%|find "SHA1" 1>>config
busybox.exe sed -i "s/\r//g;s/^M//g" config
set var=#
magiskboot.exe cpio ramdisk.cpio "add 0750 init magiskinit" "mkdir 0750 overlay.d" "mkdir 0750 overlay.d/sbin" "add 0644 overlay.d/sbin/magisk32.xz magisk32.xz" "%var% add 0644 overlay.d/sbin/magisk64.xz magisk64.xz" "patch" "backup ramdisk.cpio.orig" "mkdir 000 .backup" "add 000 .backup/.magisk config" 1>nul 2>nul
:MAGISKPATCH-25200-3
::测试和修补dtb
set dtbname=
if exist dtb set dtbname=dtb& call :magiskpatch-25200-dtb
if exist kernel_dtb set dtbname=kernel_dtb& call :magiskpatch-25200-dtb
if exist extra set dtbname=extra& call :magiskpatch-25200-dtb
goto MAGISKPATCH-25200-4
:magiskpatch-25200-dtb
magiskboot.exe dtb %dtbname% test 1>nul 2>nul
magiskboot.exe dtb %dtbname% patch 1>nul 2>nul
goto :eof
:MAGISKPATCH-25200-4
::尝试修补kernel
magiskboot.exe hexpatch kernel 49010054011440B93FA00F71E9000054010840B93FA00F7189000054001840B91FA00F7188010054 A1020054011440B93FA00F7140020054010840B93FA00F71E0010054001840B91FA00F7181010054 1>nul 2>nul
magiskboot.exe hexpatch kernel 821B8012 E2FF8F12 1>nul 2>nul
magiskboot.exe hexpatch kernel 77616E745F696E697472616D667300 736B69705F696E697472616D667300 1>nul 2>nul

goto MAGISKPATCH-DONE

:MAGISKPATCH-21200
::检查Magisk组件
if not exist .\magiskinit goto FATAL
::解包boot

magiskboot.exe unpack -h %bootpath% 1>nul 2>nul

::检查recovery_dtbo
if exist .\recovery_dtbo set RECOVERYMODE=true
::测试ramdisk
if not exist .\ramdisk.cpio (
    set STATUS=0& goto MAGISKPATCH-21200-MODE0)
magiskboot.exe cpio .\ramdisk.cpio test 1>nul 2>nul
set STATUS=%errorlevel%
if "%STATUS%"=="0" goto MAGISKPATCH-21200-MODE0
if "%STATUS%"=="1" goto MAGISKPATCH-21200-MODE1
pause>nul & goto MAGISKPATCH-21200
::模式0-Stock boot image detected
:MAGISKPATCH-21200-MODE0
set SHA1=
for /f %%a in ('magiskboot sha1 %bootpath% 1^>nul 2^>nul') do set SHA1=%%a
if exist .\ramdisk.cpio (
    copy /Y .\ramdisk.cpio .\ramdisk.cpio.orig 1>nul 2>nul)
goto MAGISKPATCH-21200-2
::模式1-Magisk patched boot image detected
:MAGISKPATCH-21200-MODE1
set SHA1=
for /f %%a in ('magiskboot cpio ramdisk.cpio sha1 1^>nul 2^>nul') do set SHA1=%%a

magiskboot.exe cpio ramdisk.cpio restore 1>nul 2>nul

copy /Y .\ramdisk.cpio .\ramdisk.cpio.orig 1>nul 2>nul
goto MAGISKPATCH-21200-2
:MAGISKPATCH-21200-2
::修补ramdisk.cpio

echo.KEEPVERITY=%KEEPVERITY%>config& echo.KEEPFORCEENCRYPT=%KEEPFORCEENCRYPT%>>config& echo.RECOVERYMODE=%RECOVERYMODE%>>config
if not "%SHA1%"=="" echo.SHA1=%SHA1%|find "SHA1" 1>>config
busybox.exe sed -i "s/\r//g;s/^M//g" config
type config>>nul
magiskboot.exe cpio ramdisk.cpio "add 750 init magiskinit" "patch" "backup ramdisk.cpio.orig" "mkdir 000 .backup" "add 000 .backup/.magisk config" 1>nul 2>nul

:MAGISKPATCH-21200-3
::尝试修补dtb
set dtbname=
if exist .\dtb set dtbname=dtb
if exist .\kernel_dtb set dtbname=kernel_dtb
if exist .\extra set dtbname=extra
if exist .\recovery_dtbo set dtbname=recovery_dtbo
if "%dtbname%"=="" (
    goto MAGISKPATCH-21200-4)
magiskboot.exe dtb %dtbname% patch 1>nul 2>nul
:MAGISKPATCH-21200-4
::尝试修补kernel
magiskboot.exe hexpatch kernel 49010054011440B93FA00F71E9000054010840B93FA00F7189000054001840B91FA00F7188010054 A1020054011440B93FA00F7140020054010840B93FA00F71E0010054001840B91FA00F7181010054 1>nul 2>nul
magiskboot.exe hexpatch kernel 821B8012 E2FF8F12 1>nul 2>nul
magiskboot.exe hexpatch kernel 77616E745F696E697472616D667300 736B69705F696E697472616D667300 1>nul 2>nul
goto MAGISKPATCH-DONE

:MAGISKPATCH-DONE
::打包boot
magiskboot.exe repack %bootpath% boot_new.img 1>nul 2>nul
::和原boot比较大小
set origbootsize=
for /f "tokens=2 delims= " %%a in ('busybox.exe stat -t %bootpath%') do set origbootsize=%%a
if "%origbootsize%"=="" goto FATAL
set patchedbootsize=
for /f "tokens=2 delims= " %%a in ('busybox.exe stat -t .\boot_new.img') do set patchedbootsize=%%a
if "%patchedbootsize%"=="" goto FATAL
::移动成品到指定目录
move /Y .\boot_new.img %outputpath% 1>nul 2>nul
::清理临时文件目录
del /Q /F .\magiskinit >nul 2>nul
del /Q /F .\header >nul 2>nul
del /Q /F .\kernel_dtb >nul 2>nul
del /Q /F .\kernel >nul 2>nul
del /Q /F .\ramdisk.cpio >nul 2>nul
del /Q /F .\config >nul 2>nul
del /Q /F .\ramdisk.cpio.orig >nul 2>nul
ENDLOCAL
goto :eof

:FATAL
exit /b 255
