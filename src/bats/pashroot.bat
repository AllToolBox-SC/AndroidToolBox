if "%1"=="" goto run
goto %1
:run
call innermodel
ECHO %INFO%请选择一个由innermodel命名的.zip文件
ECHO %INFO%比如I20.zip
call sel file s .. [zip]
copy /Y "%sel__file_path%" "%cd%\EDL\%sel__file_fullname%"
ECHO 导入完成，请确保文件格式正确并可用，按任意键返回上级菜单
pause >nul
exit /b

:noinfo
call sel file s .. [zip]
copy /Y "%sel__file_path%" "%cd%\EDL\%sel__file_fullname%"