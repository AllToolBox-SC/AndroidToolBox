::call number [文件] [输出变量]

@echo off
setlocal enabledelayedexpansion

set "file_path=%1"
set "chown="
if not exist .\%file_path% echo.%ERROR%找不到指定文件 & pause & exit /b

if exist "%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" (
    for /f "delims=" %%a in ('powershell -Command "Get-Content '%file_path%' | Select-String -Pattern '\d+' -AllMatches | ForEach-Object { $_.Matches } | ForEach-Object { $_.Value }"') do (
        set "chown=!chown! %%a"
    )
)
if defined chown set "chown=%chown:~1%"
ENDLOCAL & set "%2=%chown%"
goto :eof

:extract_numbers
for %%i in (%*) do (
    echo %%i | findstr /r "^[0-9][0-9]*$" >nul
    if not errorlevel 1 set "chown=%chown% %%i"
)
exit /b