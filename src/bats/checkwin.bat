@echo off
setlocal enabledelayedexpansion
:: 使用PowerShell获取系统信息
for /f "delims=" %%i in ('powershell -Command "(Get-CimInstance Win32_OperatingSystem).Caption"') do set "os_name=%%i"
for /f "delims=" %%i in ('powershell -Command "(Get-CimInstance Win32_OperatingSystem).Version"') do set "os_version=%%i"
for /f "delims=" %%i in ('powershell -Command "(Get-CimInstance Win32_OperatingSystem).OSArchitecture"') do set "arch=%%i"

:: 输出格式化的信息
echo %INFO%当前运行环境:%os_name%_%arch%_%os_version%%RESET%

:: 清理系统名称中的空格
set "os_name=%os_name: =%"

:: 检查系统版本
echo !os_name! | find "Windows11" >nul && echo %INFO%当前系统: Windows 11%RESET% && exit /b
echo !os_name! | find "Windows10" >nul && echo %INFO%当前系统: Windows 10%RESET% && exit /b
echo !os_name! | find "Windows8.1" >nul && echo %INFO%当前系统: !os_name!%RESET% && exit /b
echo !os_name! | find "Windows8" >nul && echo %INFO%当前系统: !os_name!%RESET% && exit /b

echo %ERROR%此脚本需要 Windows 8 或更高版本%RESET%

pause
exit