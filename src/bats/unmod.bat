CLS
if not exist .\mod ECHO.%ERROR%你没有安装任何扩展 & pause & exit /b
echo %INFO%请选择你要卸载的扩展%RESET%
call sel folder s %cd%\mod
set /p confirm=%YELLOW%确定卸载？(y/n):%RESET% 
if /i "%confirm%"=="y" (
    call .\mod\%sel__file_name%\unmod.bat
    rd /Q /S %sel__folder_path%
    echo %INFO%卸载已完成%RESET%
    echo %INFO%按任意键返回%RESET%
    pause >nul
    exit /b
)
echo.%YELLOW%未确定
pause >nul
exit /b