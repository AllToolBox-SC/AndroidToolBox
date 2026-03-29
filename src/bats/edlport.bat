:EDL
setlocal
ECHO %INFO%%RESET%%BLUE%正在查找9008端口%RESET%
set try_times=1
set again=1
:EDL-start
busybox sleep 1
if %try_times% GTR 120 ECHO %ERROR%连接edl设备超时!%RESET% && ECHO %YELLOW%按任意键重新检查...%RESET% && pause>nul && set try_times=1
set /a try_times+=1
for /f %%a in ('lsusb ^| find /c "Qualcomm HS-USB QDLoader 9008"') do (if %%a LSS 1 busybox sleep 1 && goto EDL-start & if %%a GTR 1 ECHO %ERROR%有多个edl设备连接! 请断开其他设备.%RESET% && ECHO %YELLOW%按任意键重新检查...%RESET% && pause>nul && goto EDL-start)
::目标设备已经检测到
if not %again% GTR 1 set /a again+=1 && set try_times=1 && goto EDL-start
lsusb | find "Qualcomm HS-USB QDLoader 9008" 1>tmp\output.txt
for /f "tokens=2 delims=()" %%a in (tmp\output.txt) do set "com_port=%%a"
set "port=%com_port:COM=%"
ECHO %INFO%发现9008端口！端口号为...COM%port%%RESET%
ENDLOCAL & set chkdev__edl_port=%port%& set chkdev__edl__port=%port%& set edl_port=%com_port%& set edlport=%port%
del /Q /F .\tmp\output.txt
exit /b