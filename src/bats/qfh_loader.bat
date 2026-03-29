:run
qfh_loader.exe %* >qfhtmp.txt || goto error
exit /b
:error
copy /Y .\qfhtmp.txt .\logs\qfherror_%RANDOM%%RANDOM%.txt >nul
set qfhtmp=""
set /p qfhtmp=%error%9008重启失败！[输入"log"输出日志]按任意键重新尝试...
if "%qfhtmp%"=="log" type qfhtmp.txt & echo.按任意键重试... & pause >nul
goto run