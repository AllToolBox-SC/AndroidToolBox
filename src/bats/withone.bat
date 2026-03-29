@echo off
REM ========== 读取身份 ==========
set /p whoyou=<whoyou.txt
if "%whoyou%"=="1" (
    call uplog
)

REM ========== 检查高通设备 ==========
set "QUALCOMM=0"
if exist "%SystemRoot%\System32\DriverStore\FileRepository\qdbusb*" (
    set "QUALCOMM=1"
)
if exist "%SystemRoot%\System32\DriverStore\FileRepository\qcusb*" (
    set "QUALCOMM=1"
)
if exist "%SystemRoot%\System32\DriverStore\FileRepository\qcmbn*" (
    set "QUALCOMM=1"
)
REM ========== 检查ADB ==========
set "ADB_INSTALLED=0"
if exist "%SystemRoot%\System32\drivers\winusb.sys" (
    set "ADB_INSTALLED=1"
)
if exist "%SystemRoot%\System32\drivers\usbwin.sys" (
    set "ADB_INSTALLED=1"
)
REM ========== 检查VC运行库 ==========
set "VC_RUNTIMES=0"
if exist "%SystemRoot%\System32\vc*" (
    set "VC_RUNTIMES=1"
)

REM ========== 检查结果并引导安装 ==========
if %QUALCOMM% equ 1 if %ADB_INSTALLED% equ 1 if %VC_RUNTIMES% equ 1 (
    exit /b
)
echo %ERROR%检查到驱动未安装或环境不完整
if %ADB_INSTALLED% neq 1 (
    echo %INFO%安装ADB驱动...
    .\drivers\ADB.exe
    echo %INFO%安装ADB驱动完毕
)
if %QUALCOMM% neq 1 (
    echo %INFO%安装Qualcomm驱动...
    .\drivers\9008.exe
    echo %INFO%安装Qualcomm驱动完毕
    echo %WARN%安装Qualcomm驱动完毕
)
if %VC_RUNTIMES% neq 1 (
    echo %INFO%安装VC运行库...
    .\drivers\vc.exe
    echo %INFO%安装VC运行库完毕
)
echo %INFO%驱动和环境配置完毕，部分更改可能需要重启电脑以完成安装
exit /b
