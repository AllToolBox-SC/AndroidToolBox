@echo off
:MAIN_MENU
CLS
call logo.bat
ECHO %ORANGE%卸载应用菜单%YELLOW%
menu.exe .\menu\unapp.xml
set /p MENU=<menutmp.txt
if "%MENU%"=="A" exit /b
if "%MENU%"=="a" exit /b
if "%MENU%"=="1" echo.%INFO% 所有应用列表:%RESET% & adb shell pm list packages & pause & goto MAIN_MENU
if "%MENU%"=="2" echo.%INFO% 第三方应用列表:%RESET% & adb shell pm list packages -3 & pause & goto MAIN_MENU
if "%MENU%"=="3" goto UNINSTALL
if "%MENU%"=="4" call check_adb & pause & goto MAIN_MENU
ECHO %ERROR%输入错误，请重新输入！%RESET%
timeout /t 2 >nul
goto MAIN_MENU

:UNINSTALL
echo.
echo %YELLOW%卸载应用%RESET%
set package_name=""
set /p package_name=%CYAN%请输入包名:%RESET% 
if "%package_name%"=="" (
    echo %ERROR%未输入包名！%RESET%
    goto UNINSTALL
)

echo %WARN% 确认卸载应用: %PINK%%package_name%%RESET%
set confirm=""
set /p confirm=%YELLOW%确定卸载？(y/n):%RESET% 
if /i "%confirm%"=="y" (
    adb uninstall %package_name% > ".\tmp\unapptmp.txt"
    find /i "Success" "%cd%\tmp\unapptmp.txt" >nul
    if !errorlevel! equ 0 (
        echo %GREEN% 卸载成功！%RESET%
    ) else (
        echo %ERROR% 卸载失败！%RESET%
    )
)

echo %info%按任意键返回...%RESET%
pause >nul
goto MAIN_MENU