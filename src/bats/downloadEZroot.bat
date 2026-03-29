@echo off
setlocal enabledelayedexpansion
echo %INFO%开始下载EZ-root文件...%RESET%
echo.

:: 直接列出所有模型名称
set "models=I17D I11 IB I12 I13 I13C I18 I17 I16 GLI17 DI02 HKI17 I19 IDI13 THI17 PHI17 I20 I28 I26A I25 I25C I25D I32 ND01 ND07 ND03 DI01 I23-D2 I23-Q1A I23-Y8 I27 I31-Z6A I31-Z6S ND02"

for %%m in (%models%) do (
    echo %INFO%开始下载%RESET%
    
    :: 下载文件
    call curltool.bat https://vip.123pan.cn/1814215835/xtc_root/ezr_file/root_file/%%m.zip -o EDL\%%m.zip
    if %errorlevel% neq 0 (
        echo %ERROR%下载文件时出现错误，错误值:%errorlevel%，按任意键跳过
        pause >nul
    )
    echo %GREEN%下载完成%RESET%
    echo.
)

echo  %INFO%处理不受支持的root文件%RESET%

:: 进入root目录
cd .\EDL

:: 查找并删除所有小于1KB的zip文件
for %%f in (*.zip) do (
    set "filesize=%%~zf"
    if !filesize! LSS 1024 (
        del "%%f"
    )
)

:: 返回上级目录
cd ..

echo %GREEN%EZ-root文件全部处理完成%RESET%
