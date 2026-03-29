@echo off
:run
setlocal

:: 直接查询注册表值
for /f "tokens=4" %%i in ('reg query "HKEY_CLASSES_ROOT\ToDesk" /v "URL Protocol" 2^>nul') do set "todesk=%%i"

:: 显示结果
if defined todesk (
    echo %INFO%检查到你电脑中有todesk，路径在: %todesk%%RESET%
    start "" ""%todesk%""
    timeout /t 2 /NOBREAK >nul
    exit /b
) else (
    echo %ERROR%未找到todesk远程软件，按任意键开始下载%RESET%
    pause >nul
    goto download
)

endlocal

:download
echo %INFO%开始下载%RESET%
call curltool https://dl.todesk.com/irrigation/ToDesk_4.8.3.3.exe -o tmp\ToDesk_4.8.3.3.exe
if %errorlevel% neq 0 (
   echo %ERROR%下载文件时出现错误，错误值:%errorlevel%，按任意键跳过
   pause >nul
)
echo %GREEN%下载完成%RESET%
echo %INFO%请根据指引进行安装%RESET%
.\tmp\ToDesk_4.8.3.3.exe
echo %INFO%清理缓存文件%RESET%
del /Q /F .\tmp\ToDesk_4.8.3.3.exe
goto run
pause