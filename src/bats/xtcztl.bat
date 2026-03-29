cls
echo %YELLOW%你想使用哪种方式安装并打开XTC状态栏
set /p yesno=1普通安装，2root静默安装，按任意键返回：
if "%yesno%"=="1" goto a1
if "%yesno%"=="2" goto a2
exit /b

:a1
echo %INFO%请接入ADB设备%RESET%
device_check.exe adb&&ECHO.

echo %INFO%正在安装XTC状态栏...%RESET%
adb install "%cd%\apks\xtcztl.apk"
if errorlevel 1 (
    timeout /t 2 /nobreak >nul
    echo %RED%安装失败！按任意键返回%RESET%
    pause
    exit /b
) else (
    echo %GREEN%小天 才状态栏安装成功！%RESET%
    goto openxtcztl
)

:a2
echo %INFO%请接入ADB设备%RESET%
device_check.exe adb&&ECHO.

echo %INFO%正在安装XTC状态栏...%RESET%
adb install -r "%cd%\apks\xtcztl.apk"
if errorlevel 1 (
    timeout /t 2 /nobreak >nul
    echo %RED%安装失败！按任意键返回%RESET%
    pause
    exit /b
) else (
    echo %GREEN%小天 才状态栏安装成功！%RESET%
    goto openxtcztl
)

:openxtcztl
echo %INFO%正在打开XTC状态栏...%RESET%
adb shell monkey -p com.xtcztl.xtcstatusbar 1
if %errorlevel% equ 0 (
    echo %GREEN%打开成功%RESET%
    timeout /t 1 /nobreak >nul
    echo %INFO%按任意键返回%RESET%
    pause >nul
    exit /b
) else (
    echo %ERROR%打开失败！
    echo %ERROR%错误值：%errorlevel%
    echo %INFO%请前往XTC状态栏桌面快捷方式打开！%RESET%
    echo %INFO%按任意键返回%RESET%
    pause >nul
    exit /b
)

