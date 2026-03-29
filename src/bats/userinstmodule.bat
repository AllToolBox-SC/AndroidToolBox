:MAIN_MENU
CLS
call logo.bat
ECHO %ORANGE%刷入模块菜单%YELLOW%
menu.exe .\menu\userinstmodule.xml
set /p MENU=<menutmp.txt
if "%MENU%"=="A" exit /b
if "%MENU%"=="a" exit /b
if "%MENU%"=="1" goto INSTALL_SINGLE_SEL
if "%MENU%"=="2" goto INSTALL_MULTI_SEL
if "%MENU%"=="3" goto INSTALL_FOLDER_SEL
if "%MENU%"=="4" goto CHECK_DEVICE
ECHO %ERROR%输入错误，请重新输入！%RESET%
timeout /t 2 >nul
goto MAIN_MENU

:INSTALL_SINGLE_SEL
setlocal enabledelayedexpansion
echo.
echo %INFO% 正在打开文件选择对话框...%RESET%
call sel file s . [zip]

echo.
echo %INFO%即将刷入：%sel__file_fullname%%RESET%
ECHO.请接入ADB设备...
device_check.exe adb&&ECHO.
ECHO.

for /f "delims=" %%i in ('adb shell getprop ro.product.innermodel') do set innermodel=%%i
ECHO.%INFO%您的设备innermodel为:%innermodel%
for /f "delims=" %%i in ('adb shell getprop ro.build.version.release') do set androidversion=%%i
ECHO.%INFO%您的设备安卓版本为:%androidversion%
echo %INFO% 正在刷入模块...%RESET%
if "%androidversion%"=="7.1.1" (call instmodule2.bat %sel__file_path%) else if "%androidversion%"=="4.4.4" (call instmodule2.bat %sel__file_path%) else (call instmodule.bat %sel__file_path%)
echo %GREEN%刷入完成！%RESET%
call userinstmodulereboot.bat
echo.按任意键返回菜单
pause >nul
endlocal
goto MAIN_MENU

:INSTALL_MULTI_SEL
setlocal enabledelayedexpansion
echo.
echo %INFO% 正在打开文件选择对话框(多选)...%RESET%
call sel file m . [zip]

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
for /f "delims=" %%i in ('adb shell getprop ro.product.innermodel') do set innermodel=%%i
ECHO.%INFO%您的设备innermodel为:%innermodel%
for /f "delims=" %%i in ('adb shell getprop ro.build.version.release') do set androidversion=%%i
ECHO.%INFO%您的设备安卓版本为:%androidversion%
echo %INFO% 开始批量刷入...%RESET%

for %%i in (%sel__files:/= %) do (
    echo.
    echo %CYAN%正在刷入: %%~nxi%RESET%
    for %%A in ("%%i") do set SIZE_BYTES=%%~zA
    if "%androidversion%"=="7.1.1" (call instmodule2.bat %%i) else if "%androidversion%"=="4.4.4" (call instmodule2.bat %%i) else (call instmodule.bat %%i)
)

echo.
echo %GREEN%批量刷入完成！%RESET%
echo %CYAN%总计：%RESET%%WHITE%!COUNT!%RESET% %CYAN%个模块%RESET%
call userinstmodulereboot.bat
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
echo %INFO% 扫描zip文件...%RESET%
set COUNT=0
for %%i in ("%sel__folder_path%\*.zip") do (
    set /a COUNT+=1
    set "FILE_!COUNT!=%%i"
    echo %CYAN%!COUNT!.%RESET% %WHITE%%%i%RESET%
)

if !COUNT! equ 0 (
    echo %ERROR% 在指定文件夹中未找到zip文件%RESET%
    pause
    goto MAIN_MENU
)
ECHO.请接入ADB设备...
device_check.exe adb&&ECHO.
echo.
for /f "delims=" %%i in ('adb shell getprop ro.product.innermodel') do set innermodel=%%i
ECHO.%INFO%您的设备innermodel为:%innermodel%
for /f "delims=" %%i in ('adb shell getprop ro.build.version.release') do set androidversion=%%i
ECHO.%INFO%您的设备安卓版本为:%androidversion%
echo %INFO% 开始批量刷入...%RESET%

for /l %%n in (1,1,!COUNT!) do (
    set "file=!FILE_%%n!"
    for %%A in ("!file!") do (
        set "filename=%%~nxA"
        set SIZE_BYTES=%%~zA
    )
    echo.
    echo %CYAN%正在刷入: !filename!%RESET%
    if "%androidversion%"=="7.1.1" (call instmodule2.bat !file!) else if "%androidversion%"=="4.4.4" (call instmodule2.bat !file!) else (call instmodule.bat !file!)
)

echo.
echo %GREEN%批量刷入完成！%RESET%
echo %CYAN%总计：%RESET%%WHITE%!COUNT!%RESET% %CYAN%个模块%RESET%
echo.按任意键返回菜单
pause >nul
endlocal
goto MAIN_MENU

:CHECK_DEVICE
setlocal enabledelayedexpansion
echo.
echo %INFO% 检查设备连接...%RESET%

if not exist ".\tmp" mkdir ".\tmp"

adb devices > ".\tmp\instmoduletmp.txt" 2>&1

set /a DEVICE_COUNT=0
for /f "usebackq delims=" %%i in (".\tmp\instmoduletmp.txt") do (
    set /a DEVICE_COUNT+=1
)

set /a DEVICE_COUNT=!DEVICE_COUNT!-1

if !DEVICE_COUNT! equ 0 (
    echo %ERROR% 没有找到连接的设备%RESET%
) else (
    echo %CYAN%ADB设备列表：%RESET%
    type ".\tmp\instmoduletmp.txt"
    echo.
    echo %GREEN%找到 !DEVICE_COUNT! 个设备%RESET%
)

if exist ".\tmp\instmoduletmp.txt" del ".\tmp\instmoduletmp.txt" >nul 2>&1

echo.
echo.按任意键返回菜单
pause >nul
endlocal
goto MAIN_MENU