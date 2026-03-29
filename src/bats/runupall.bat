@echo off
ECHO.[信息]正在更新...
set "random1=%random%"
curl.exe -s -k https://ATB.xgj.qzz.io/dllink.txt -o dllink.txt 2>nul
set /p dllink=<dllink.txt
curl.exe -k# -A "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36" -L %dllink% -o .\
if %errorlevel% neq 0 (
   echo 下载文件时出现错误，错误值:%errorlevel%，请自行去官网https://atb.xgj.qzz.io下载最新版本，按任意键退出更新
   pause >nul
   exit /b
)
:: set "file=%random1%.7z"
:: for %%f in ("%file%") do (
    :: if %%~zf LEQ 1024 (
        :: echo 更新出错，下载的文件不正常，请自行去官网https://atb.xgj.qzz.io下载最新版本
        :: pause
        :: exit /b
    :: )
:: )

taskkill /IM "adb.exe" /F /T
rd /S /Q ".\bin"
del /Q /F ".\双击运行.bat"
del /Q /F ".\双击运行.exe"
7z x %random1%.7z -o.\ -aoa -bsp1
del /Q /F ".\%random1%.7z"
del /Q /F .\7z.dll
del /Q /F .\7z.exe
del /Q /F .\curltool.bat
del /Q /F .\curl.exe
del /Q /F .\dllink.txt
if exist .\mod xcopy /s /e /y  .\mod .\bin\mod
set /p="3" <nul > .\bin\whoyou.txt
start "installer.exe"
powershell -Command "Start-Sleep -Milliseconds 1000; Remove-Item -Path '%~f0' -Force"
exit