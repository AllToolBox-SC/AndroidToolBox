@echo off
setlocal enabledelayedexpansion
ECHO %WARN%此功能将从EZroot云端下载文件，有一定侵权风险，需要用户自己承担
ECHO %WARN%下载本工具本地会带一份文件，正常不需要使用此功能
ECHO %INFO%读完以上提示后，按任意键继续下载
pause >nul
echo %INFO%测试与EZ服务器的连通性%RESET%
curl.exe -sSkL "https://vip.123pan.cn/1814215835/xtc_root/ezr_file/ezr_notice" -o "tmp\ezr_notice.tmp" 2>nul
if %errorlevel% neq 0 (
   echo %ERROR%下载文件时出现错误，错误值:%errorlevel%
   ECHO %ERROR%无法与EZ服务器取得连接，按任意键返回%RESET%
   pause >nul
   exit /b
)
set /p notice=<.\tmp\ezr_notice.tmp
set "notice=!notice:"=!"
if "%notice%"=="强烈推荐凌CIM：lcim.guozhufeng.com 凌红科技旗下的一款聊天软件 如果您在使用过程中产生问题，请加QQ群：1055102921（进群问题答案：lcim.guozhufeng.com)" goto run
ECHO %ERROR%无法与EZ服务器取得连接，按任意键返回%RESET%
pause >nul
exit /b

:run
echo %INFO%与EZ服务器的连通正常%RESET%
ECHO %date% %time% >"%cd%\EDL\time.txt"
echo %INFO%开始下载%RESET%
call curltool.bat https://vip.123pan.cn/1814215835/xtc_root/ezr_file/root_file/userdata.img -o tmp\userdata.img
if %errorlevel% neq 0 (
   echo %ERROR%下载文件时出现错误，错误值:%errorlevel%，按任意键跳过
   pause >nul
)
echo %GREEN%下载完成%RESET%
::echo.
::echo %INFO%开始下载%RESET%
::call curltool.bat https://vip.123pan.cn/1814215835/xtc_root/xtcpatch/xtcpatch.zip -o tmp\xtcpatch.zip
::if %errorlevel% neq 0 (
::   echo %ERROR%下载文件时出现错误，错误值:%errorlevel%，按任意键跳过
::   pause >nul
::)
::echo %GREEN%下载完成%RESET%
::echo.
echo %INFO%开始下载%RESET%
call curltool.bat https://vip.123pan.cn/1814215835/xtc_root/444xposed.zip -o tmp\xposed.zip
if %errorlevel% neq 0 (
   echo %ERROR%下载文件时出现错误，错误值:%errorlevel%，按任意键跳过
   pause >nul
)
echo %GREEN%下载完成%RESET%
echo.
echo %INFO%开始下载%RESET%
call curltool.bat https://vip.123pan.cn/1814215835/xtc_root/toolkit.apk -o tmp\toolkit.apk
if %errorlevel% neq 0 (
   echo %ERROR%下载文件时出现错误，错误值:%errorlevel%，按任意键跳过
   pause >nul
)
echo %GREEN%下载完成%RESET%
echo.
echo %INFO%开始下载%RESET%
call curltool.bat https://vip.123pan.cn/1814215835/xtc_root/25-xposed-magisk.zip -o tmp\25-xposed-magisk.zip
if %errorlevel% neq 0 (
   echo %ERROR%下载文件时出现错误，错误值:%errorlevel%，按任意键跳过
   pause >nul
)
echo %GREEN%下载完成%RESET%
echo.
call downloadEZroot
ECHO.
ECHO %GREEN%下载全部完成，按任意键返回上一级目录%RESET%
pause >nul
exit /b