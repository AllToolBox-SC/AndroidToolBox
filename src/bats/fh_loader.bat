:run
fh_loader.exe %* >FHtmp.txt || goto error
exit /b
:error
copy /Y .\FHtmp.txt .\logs\FHerror_%RANDOM%%RANDOM%.txt >nul
set FHtmp=""
set /p FHtmp=%error%9008读取或刷入失败！[输入"log"输出日志]按任意键重新尝试...
if "%FHtmp%"=="log" type FHtmp.txt & echo.按任意键重试... & pause >nul
goto run