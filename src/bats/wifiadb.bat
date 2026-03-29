setlocal
@echo off
ECHO %INFO%%RESET%请确保在同一局域网下并已用数据线连接%RESET%
device_check.exe adb&&ECHO.
ECHO %INFO%%RESET%%BLUE%正在开启调试端口5555%RESET%
adb usb 1>nul 2>nul
busybox sleep 5
adb tcpip 5555 1>nul || echo %error%开启调试端口失败 && pause && exit /b
ECHO %INFO%%RESET%%BLUE%正在获取IP地址%RESET%
for /f "tokens=*" %%a in ('adb shell ip route 1^>nul 2^>nul') do (set "iproute=%%a")
set num=1
:run
if "%num%"=="20" echo %error%查找IP地址失败%RESET% && pause && exit /b
set /a num+=1
for /f "tokens=%num%" %%a in ("%iproute%") do set "ip=%%a"
echo %ip% | find "." 1>nul 2>nul || goto run
ECHO %INFO%%RESET%%BLUE%查找到IP地址：%ip%%RESET%
ECHO %INFO%%RESET%%BLUE%与设备建立连接%RESET%
adb connect %ip%:5555 1>nul  || echo %error%连接失败 && pause && exit /b
ECHO %INFO%%RESET%%BLUE%已启动无线调试%RESET%
ECHO %INFO%%RESET%%BLUE%请拔掉数据线即可正常使用%RESET%
ENDLOCAL