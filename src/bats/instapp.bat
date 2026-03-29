@echo off
setlocal enabledelayedexpansion
set args1=%~1
set args2=%~2
set args3=%~3
:callinst
echo %CYAN%正在安装：%RESET%%PINK%%args1%%RESET%
REM 创建临时目录
if not exist ".\tmp" mkdir ".\tmp"
REM 执行安装并将输出重定向到临时文件
if "%args2%"=="nostreaming" call adbdevice adb && adb install -r -t -d --no-streaming "%args1%" > ".\tmp\instapptmp.txt"
if "%args2%"=="install" call adbdevice adb && adb install -r -t -d "%args1%" > ".\tmp\instapptmp.txt"
if "%args2%"=="data" goto data
if "%args2%"=="create" goto create
if "%args2%"=="3install" goto 3install
call adbdevice adb
adb install -r -t -d "%args1%" > ".\tmp\instapptmp.txt"
:instfind
REM 检查输出中是否包含Success
if not exist ".\tmp\instapptmp.txt" %ERROR%发生错误，没有任何安装命令被调用，请检查语法是否正确 & goto error
find /i "Success" "%cd%\tmp\instapptmp.txt" >nul
if !errorlevel! equ 0 (
    echo %GREEN% 安装成功！%RESET%
    if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
    endlocal
    set /a SUCCESS+=1
    exit /b
) else goto error

:data
echo %INFO% 使用 data/app 安装方式...%RESET%

for %%A in ("%args1%") do set APK_NAME=%%~nxA

REM 创建临时目录
if not exist ".\tmp" mkdir ".\tmp"
adb root | find "restarting" 1>nul 2>nul && goto data-root
adb shell "su -c magisk -v" && goto data-su
echo %error% 设备未获得root权限%RESET%
goto error

:data-su
echo %GREEN% 设备已获得su权限%RESET%

REM 生成随机目录名（避免冲突）
set "RANDOM_DIR=copydata-!RANDOM!!RANDOM!"
echo %INFO% 创建应用目录：/data/app/!RANDOM_DIR!%RESET%

REM 创建应用目录
adb shell su -c "mkdir -p /data/app/!RANDOM_DIR!" > ".\tmp\instapptmp.txt" 2>&1
if !errorlevel! neq 0 (
    echo %ERROR% 创建应用目录失败%RESET%
    type ".\tmp\instapptmp.txt" 2>nul
    if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
    goto error
)

REM 推送APK文件到应用目录
echo %INFO% 推送APK文件到应用目录...%RESET%
call adbdevice adb
adb push "!args1!" /data/local/tmp/!APK_NAME! > ".\tmp\instapptmp.txt" 2>&1
if !errorlevel! neq 0 (
    echo %ERROR% 推送APK到临时目录失败%RESET%
    type ".\tmp\instapptmp.txt" 2>nul
    if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
    goto error
)

REM 将APK从临时目录移动到应用目录
echo %INFO% 移动APK到应用目录...%RESET%
adb shell su -c "mv /data/local/tmp/!APK_NAME! /data/app/!RANDOM_DIR!/base.apk" > ".\tmp\instapptmp.txt" 2>&1
if !errorlevel! neq 0 (
    echo %ERROR% 移动APK文件失败%RESET%
    type ".\tmp\instapptmp.txt" 2>nul
    adb shell su -c "rm -rf /data/app/!RANDOM_DIR!" >nul 2>&1
    if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
    goto error
)

REM 设置权限
echo %INFO% 设置文件权限...%RESET%
adb shell su -c "chmod 755 /data/app/!RANDOM_DIR!" >nul 2>&1
adb shell su -c "chmod 644 /data/app/!RANDOM_DIR!/base.apk" >nul 2>&1

REM 设置所有者（通常为system:system）
echo %INFO% 设置文件所有者...%RESET%
adb shell su -c "chown system:system /data/app/!RANDOM_DIR!/" >nul 2>&1
adb shell su -c "chown system:system /data/app/!RANDOM_DIR!/base.apk" >nul 2>&1

echo %GREEN% APK已复制到/data/app/!RANDOM_DIR!/base.apk%RESET%

REM 检查是否需要重启包管理器或系统
echo.
echo %YELLOW% data/app安装方式可能需要重启应用包管理器或系统才能生效%RESET%
echo %YELLOW%请选择操作：
echo 1. 重启设备[推荐]
echo 2. 不执行任何操作
set /p RESTART_CHOICE=%YELLOW%请输入序号并按下回车键(默认1): %RESET%

if "!RESTART_CHOICE!"=="" set "RESTART_CHOICE=1"

if "!RESTART_CHOICE!"=="1" (
    echo %INFO% 正在重启设备...%RESET%
    adb reboot
    device_check.exe adb&&ECHO.
)
if "!RESTART_CHOICE!"=="2" (
    echo %INFO% 未执行任何操作，应用可能需要重启设备才能生效%RESET%
)

REM 清理临时文件
adb shell su -c "rm -f /data/local/tmp/!APK_NAME!" >nul 2>&1
echo %GREEN% data/app安装完成！%RESET%
echo %CYAN%应用路径：/data/app/!RANDOM_DIR!/base.apk%RESET%
if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
endlocal
set /a SUCCESS+=1
exit /b

:data-root
echo %GREEN% 设备已获得root权限%RESET%
REM 生成随机目录名（避免冲突）
set "RANDOM_DIR=copydata-!RANDOM!!RANDOM!"
echo %INFO% 创建应用目录：/data/app/!RANDOM_DIR!%RESET%

REM 创建应用目录
adb shell mkdir -p /data/app/!RANDOM_DIR! > ".\tmp\instapptmp.txt" 2>&1
if !errorlevel! neq 0 (
    echo %ERROR% 创建应用目录失败%RESET%
    type ".\tmp\instapptmp.txt" 2>nul
    if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
    goto error
)

REM 推送APK文件到应用目录
echo %INFO% 推送APK文件到应用目录...%RESET%
call adbdevice adb
adb push "!args1!" /data/local/tmp/!APK_NAME! > ".\tmp\instapptmp.txt" 2>&1
if !errorlevel! neq 0 (
    echo %ERROR% 推送APK到临时目录失败%RESET%
    type ".\tmp\instapptmp.txt" 2>nul
    if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
    goto error
)

REM 将APK从临时目录移动到应用目录
echo %INFO% 移动APK到应用目录...%RESET%
adb shell mv /data/local/tmp/!APK_NAME! /data/app/!RANDOM_DIR!/base.apk > ".\tmp\instapptmp.txt" 2>&1
if !errorlevel! neq 0 (
    echo %ERROR% 移动APK文件失败%RESET%
    type ".\tmp\instapptmp.txt" 2>nul
    adb shell rm -rf /data/app/!RANDOM_DIR! >nul 2>&1
    if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
    goto error
)

REM 设置权限
echo %INFO% 设置文件权限...%RESET%
adb shell chmod 755 /data/app/!RANDOM_DIR! >nul 2>&1
adb shell chmod 644 /data/app/!RANDOM_DIR!/base.apk >nul 2>&1

REM 设置所有者（通常为system:system）
echo %INFO% 设置文件所有者...%RESET%
adb shell chown system:system /data/app/!RANDOM_DIR!/ >nul 2>&1
adb shell chown system:system /data/app/!RANDOM_DIR!/base.apk >nul 2>&1

echo %GREEN% APK已复制到/data/app/!RANDOM_DIR!/base.apk%RESET%

REM 检查是否需要重启包管理器或系统
echo.
echo %YELLOW% data/app安装方式可能需要重启应用包管理器或系统才能生效%RESET%
echo %YELLOW%请选择操作：
echo 1. 重启应用包管理器[推荐]
echo 2. 重启设备
echo 3. 不执行任何操作
set /p RESTART_CHOICE=%YELLOW%请输入序号并按下回车键(默认1): %RESET%

if "!RESTART_CHOICE!"=="" set "RESTART_CHOICE=1"

if "!RESTART_CHOICE!"=="1" (
    echo %INFO% 重启应用包管理器...%RESET%
    adb shell am force-stop com.android.packageinstaller >nul 2>&1
    adb shell pm disable com.android.packageinstaller && pm enable com.android.packageinstaller >nul 2>&1
    echo %GREEN% 应用包管理器已重启%RESET%
    echo %INFO% 等待5秒钟让系统识别新应用%RESET%
    busybox sleep 5
)
if "!RESTART_CHOICE!"=="2" (
    echo %INFO% 正在重启设备...%RESET%
    adb reboot
    device_check.exe adb&&ECHO.
)
echo %INFO% 未执行任何操作，应用可能需要重启设备才能生效%RESET%

REM 清理临时文件
adb shell rm -f /data/local/tmp/!APK_NAME! >nul 2>&1
echo %GREEN% data/app安装完成！%RESET%
echo %CYAN%应用路径：/data/app/!RANDOM_DIR!/base.apk%RESET%
if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
endlocal
set /a SUCCESS+=1
exit /b

:create
for %%A in ("%args1%") do set APK_SIZE=%%~zA
for %%A in ("%args1%") do set APK_NAME=%%~nxA

echo %INFO% 使用 pm install-create 安装...%RESET%

REM 创建临时目录
if not exist ".\tmp" mkdir ".\tmp"

REM 创建安装会话
set "SESSION_ID="
for /f "tokens=2 delims=[]" %%i in ('adb shell pm install-create -r -t -S !APK_SIZE!') do (
    set "SESSION_ID=%%i"
)

if "!SESSION_ID!"=="" (
    echo %ERROR% 创建安装会话失败%RESET%
    REM 删除临时文件
    if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
    goto error
)

echo %INFO% 会话创建成功: [!SESSION_ID!]%RESET%

REM 推送APK文件到设备临时目录
echo %INFO% 推送APK文件到设备...%RESET%
call adbdevice adb
adb push "!args1!" /data/local/tmp/!APK_NAME!

REM 写入会话
echo %INFO% 写入安装会话...%RESET%
adb shell pm install-write !SESSION_ID! base.apk /data/local/tmp/!APK_NAME!

REM 提交安装并将输出重定向到临时文件
echo %INFO% 提交安装...%RESET%
adb shell pm install-commit !SESSION_ID! > ".\tmp\instapptmp.txt" 2>&1
adb shell rm -f /data/local/tmp/!APK_NAME!
goto instfind

:3install

echo %INFO% 使用 第三方安装器 安装...%RESET%

REM 创建临时目录
if not exist ".\tmp" mkdir ".\tmp"

REM 推送APK文件到设备临时目录
echo %INFO% 推送APK文件到设备...%RESET%
call adbdevice adb
adb push "!args1!" /sdcard/tmp.apk

echo %INFO% 开始调用安装器安装...%RESET%
adb shell am start -a android.intent.action.VIEW -d file:///sdcard/tmp.apk -t application/vnd.android.package-archive > ".\tmp\instapptmp.txt" 2>&1
echo %INFO% 请在设备上进行安装后按任意键继续
pause >nul
echo %GREEN% 安装完成！%RESET%
call adbdevice adb
adb shell rm /sdcard/tmp.apk
if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
endlocal
set /a SUCCESS+=1
exit /b

:error
if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
set /p yesno=%ERROR% 安装失败！按任意键重试...[输入no跳过]%RESET%
if "%yesno%"=="no" endlocal&&set /a FAILED+=1&&exit /b
goto callinst