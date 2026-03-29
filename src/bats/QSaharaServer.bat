:run
busybox timeout 10 cmd /c QSaharaServer.exe %* >QStmp.txt || goto error
exit /b
:error
copy /Y .\QStmp.txt .\logs\QSerror_%RANDOM%%RANDOM%.txt >nul
set QStmp=""
set /p QStmp=%error%9008引导失败！[输入"log"输出日志]按任意键重新尝试...
if "%QStmp%"=="log" type QStmp.txt & echo.按任意键重试... & pause >nul
goto run