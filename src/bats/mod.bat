CLS
echo %INFO%请选择一个后缀名为.atbmod的英文名文件进行安装%RESET%
call sel file s . [atbmod]
if not exist .\mod md mod
copy /Y %sel__file_path% .\mod\%sel__file_name%.zip
md .\mod\%sel__file_name%
if %errorlevel% neq 0 (
   echo %ERROR%文件夹创建失败，按任意键退出
   pause >nul
   exit /b
)
7z x .\mod\%sel__file_name%.zip -o.\mod\%sel__file_name%\ -aoa -bsp1
if %errorlevel% neq 0 (
   echo %ERROR%解压文件时出现错误，错误值:%errorlevel%，按任意键退出
   pause >nul
   exit /b
)
del /Q /F .\mod\%sel__file_name%.zip
call .\mod\%sel__file_name%\inst.bat
echo %INFO%扩展安装成功%RESET%
echo %INFO%使用方法:在控制台内输入扩展名称或者序号%RESET%
echo %INFO%按任意键返回%RESET%
pause >nul
exit /b