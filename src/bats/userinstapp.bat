@echo off
setlocal enabledelayedexpansion
:MAIN_MENU
CLS
call logo.bat
ECHO %ORANGE%安装应用菜单%YELLOW%
menu.exe .\menu\userinstapp.xml
set /p MENU=<menutmp.txt
if "%MENU%"=="A" (
    if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
    exit /b
)
if "%MENU%"=="a" (
    if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1
    exit /b
)
if "%MENU%"=="1" goto INSTALL_SINGLE_SEL
if "%MENU%"=="2" goto INSTALL_MULTI_SEL
if "%MENU%"=="3" goto INSTALL_FOLDER_SEL
if "%MENU%"=="4" goto CHECK_DEVICE
if "%MENU%"=="5" goto INSTALL_MOD
ECHO %ERROR%输入错误，请重新输入！%RESET%
timeout /t 2 >nul
goto MAIN_MENU

:INSTALL_SINGLE_SEL
setlocal enabledelayedexpansion
echo.
echo %INFO% 正在打开文件选择对话框...%RESET%
call sel file s . [apk]

echo.
echo %INFO% 安装信息：%RESET%
echo %CYAN%文件：%RESET%%PINK%%sel__file_path%%RESET%
echo %CYAN%名称：%RESET%%WHITE%%sel__file_fullname%%RESET%
for %%A in ("%sel__file_path%") do (
    set SIZE_BYTES=%%~zA
    set /a SIZE_MB=%%~zA/1024/1024
)
echo %CYAN%大小：%RESET%%WHITE%!SIZE_BYTES! 字节 (!SIZE_MB! MB)%RESET%
ECHO.请接入ADB设备...
device_check.exe adb&&ECHO.
ECHO.

echo %INFO% 正在安装应用...%RESET%
if exist .\instmod.txt set /p instmod=<instmod.txt
call instapp %sel__file_path% %instmod%

echo.
echo.按任意键返回菜单
pause >nul
endlocal
goto MAIN_MENU

:INSTALL_MULTI_SEL
setlocal enabledelayedexpansion
echo.
echo %INFO% 正在打开文件选择对话框(多选)...%RESET%
call sel file m . [apk]

echo.
echo %INFO% 选择的文件列表：%RESET%
set COUNT=0
for %%f in (%sel__files:/= %) do (
    set /a COUNT+=1
    echo %CYAN%!COUNT!.%RESET% %WHITE%%%f%RESET%
    if defined FILE_LIST (
        set "FILE_LIST=!FILE_LIST! "%%f""
    ) else (
        set "FILE_LIST="%%f""
    )
)
ECHO.请接入ADB设备...
device_check.exe adb&&ECHO.
echo.
echo %INFO% 开始批量安装...%RESET%
set SUCCESS=0
set FAILED=0

REM 创建临时目录
if not exist ".\tmp" mkdir ".\tmp"
if exist .\instmod.txt set /p instmod=<instmod.txt
for %%i in (%sel__files:/= %) do (
    echo.
    echo %CYAN%正在安装: %%~nxi%RESET%
    call instapp "%%i" !instmod!
)

echo.
echo %GREEN%批量安装完成！%RESET%
echo %CYAN%总计：%RESET%%WHITE%!COUNT!%RESET% %CYAN%个应用%RESET%
echo %GREEN%成功：%RESET%%WHITE%!SUCCESS!%RESET% %CYAN%个%RESET%
echo %RED%失败：%RESET%%WHITE%!FAILED!%RESET% %CYAN%个%RESET%
echo.按任意键返回菜单
pause >nul
endlocal
goto MAIN_MENU

:INSTALL_FOLDER_SEL
setlocal enabledelayedexpansion
echo.
echo %INFO% 正在打开文件夹选择对话框...%RESET%
call sel folder s .

echo %INFO% 选择的文件夹：%RESET%%PINK%%sel__folder_path%%RESET%

echo.
echo %INFO% 扫描APK文件...%RESET%
set COUNT=0
for %%i in ("%sel__folder_path%\*.apk") do (
    set /a COUNT+=1
    set "FILE_!COUNT!=%%i"
    echo %CYAN%!COUNT!.%RESET% %WHITE%%%i%RESET%
)

if !COUNT! equ 0 (
    echo %ERROR% 在指定文件夹中未找到APK文件%RESET%
    pause
    goto MAIN_MENU
)
ECHO.请接入ADB设备...
device_check.exe adb&&ECHO.
echo.
echo %INFO% 开始批量安装...%RESET%
set SUCCESS=0
set FAILED=0

REM 创建临时目录
if not exist ".\tmp" mkdir ".\tmp"
if exist .\instmod.txt set /p instmod=<instmod.txt
for /l %%n in (1,1,!COUNT!) do (
    set "file=!FILE_%%n!"
    for %%A in ("!file!") do (
        set "filename=%%~nxA"
    )
    echo.
    echo %CYAN%正在安装: !filename!%RESET%
    call instapp "!file!" !instmod!
)

echo.
echo %GREEN%批量安装完成！%RESET%
echo %CYAN%总计：%RESET%%WHITE%!COUNT!%RESET% %CYAN%个应用%RESET%
echo %GREEN%成功：%RESET%%WHITE%!SUCCESS!%RESET% %CYAN%个%RESET%
echo %RED%失败：%RESET%%WHITE%!FAILED!%RESET% %CYAN%个%RESET%
echo.按任意键返回菜单
pause >nul
endlocal
goto MAIN_MENU

:CHECK_DEVICE
setlocal enabledelayedexpansion
echo.
echo %INFO% 检查设备连接...%RESET%

REM 创建临时目录
if not exist ".\tmp" mkdir ".\tmp"

REM 执行adb devices命令并将输出重定向到临时文件
adb devices > ".\tmp\instapptmp.txt" 2>&1

REM 计算文件行数（不包括空行）
set /a DEVICE_COUNT=0
for /f "usebackq delims=" %%i in (".\tmp\instapptmp.txt") do (
    set /a DEVICE_COUNT+=1
)

REM 行数减1（去掉标题行）
set /a DEVICE_COUNT=!DEVICE_COUNT!-1

REM 判断设备数量并显示相应信息
if !DEVICE_COUNT! equ 0 (
    echo %ERROR% 没有找到连接的设备%RESET%
) else (
    echo %CYAN%ADB设备列表：%RESET%
    type ".\tmp\instapptmp.txt"
    echo.
    echo %GREEN%找到 !DEVICE_COUNT! 个设备%RESET%
)

REM 删除临时文件
if exist ".\tmp\instapptmp.txt" del ".\tmp\instapptmp.txt" >nul 2>&1

echo.
echo.按任意键返回菜单
pause >nul
endlocal
goto MAIN_MENU

:INSTALL_MOD
CLS
call logo.bat
ECHO %ORANGE%安装应用菜单%YELLOW%
menu.exe .\menu\userinstapp_mod.xml
set /p MENU=<menutmp.txt
if "%MENU%"=="A" goto MAIN_MENU
if "%MENU%"=="a" goto MAIN_MENU
if "%MENU%"=="1" set /p="install" <nul > instmod.txt & goto INSTALL_MOD_d
if "%MENU%"=="2" set /p="data" <nul > instmod.txt & goto INSTALL_MOD_d
if "%MENU%"=="3" set /p="3install" <nul > instmod.txt & goto INSTALL_MOD_d
if "%MENU%"=="4" set /p="create" <nul > instmod.txt & goto INSTALL_MOD_d
ECHO %ERROR%输入错误，请重新输入！%RESET%
timeout /t 2 >nul
goto INSTALL_MOD

:INSTALL_MOD_d
echo %GREEN%修改安装方式成功%RESET%
timeout /t 2 >nul
goto INSTALL_MOD