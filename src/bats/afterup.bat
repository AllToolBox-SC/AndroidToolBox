set /p whoyou=<whoyou.txt
if not "%whoyou%"=="3" exit /b
echo [信息]恭喜你成功完成更新
call uplog
exit /b