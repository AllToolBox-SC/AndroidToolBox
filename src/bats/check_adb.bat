@echo off
setlocal enabledelayedexpansion
call color.bat
:MAIN_MENU
CLS
echo.
echo %INFO% 检查设备连接...%RESET%


if not exist ".\tmp" mkdir ".\tmp"


adb devices > ".\tmp\instapptmp.txt" 2>&1


set /a DEVICE_COUNT=0
for /f "usebackq delims=" %%i in (".\tmp\instapptmp.txt") do (
    set /a DEVICE_COUNT+=1
)


set /a DEVICE_COUNT=!DEVICE_COUNT!-1


if !DEVICE_COUNT! equ 0 (
    echo %ERROR% 没有找到连接的设备%RESET%
    if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
) else (
    echo %CYAN%ADB设备列表：%RESET%
    type ".\tmp\instapptmp.txt"
    echo.
    echo %GREEN%找到 !DEVICE_COUNT! 个设备%RESET%
    if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
)

if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
