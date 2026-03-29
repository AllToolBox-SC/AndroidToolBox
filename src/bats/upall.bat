@echo off
goto %1 1>nul 2>nul
:run
del /Q /F .\versiontmp.txt 1>nul 2>nul

curl.exe -s -k https://ATB.xgj.qzz.io/versiontmp.txt -o versiontmp.txt 2>nul

if %errorlevel% neq 0 (
   echo %ERROR%检查更新时出错，错误值:%errorlevel%
   exit /b
)
find /i "NO" "%cd%\versiontmp.txt" 2>nul 1>nul
if %errorlevel% equ 0 (
    CLS
    ECHO.%ERROR%运行失败%RESET%
    ECHO.%ERROR%运行失败%RESET%
    ECHO.%ERROR%运行失败%RESET%
    ECHO.%ERROR%运行失败%RESET%
    ECHO.%ERROR%运行失败%RESET%
    timeout /t 5 /nobreak >nul
    taskkill /im main.exe /f
)

set /p num1=<versiontmp.txt
set /p num2=<version.txt
if %num1% GTR %num2% (
    ECHO.%GREEN%检查到有新版本%RESET%
    type versiontmp.txt & ECHO.
    goto up
)

ECHO.%INFO%没有检查到新版本%RESET%
del /Q /F .\versiontmp.txt 1>nul 2>nul
exit /b
:up
del /Q /F .\versiontmp.txt 1>nul 2>nul
set /p yes="输入yes更新，按任意键不更新："
if not "%yes%"=="yes" exit /b
copy /Y .\runupall.bat ..\runupall.bat
copy /Y .\7z.dll ..\7z.dll
copy /Y .\7z.exe ..\7z.exe
copy /Y .\curltool.bat ..\curltool.bat
copy /Y .\curl.exe ..\curl.exe
if exist .\mod xcopy /s /e /y  .\mod ..\mod
start ..\runupall.bat
taskkill /im main.exe /f