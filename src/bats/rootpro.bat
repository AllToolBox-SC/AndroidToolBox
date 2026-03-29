device_check.exe adb&&ECHO.
call boot_completed.bat
ECHO.%INFO%开始执行优化...
for /f "delims=" %%i in ('adb shell getprop ro.product.innermodel') do set innermodel=%%i
echo %INFO%您的设备innermodel为:%innermodel%
for /f "delims=" %%i in ('adb shell getprop ro.product.model') do set model=%%i
echo %INFO%手表型号:%model%
for /f "delims=" %%i in ('adb shell getprop ro.build.version.release') do set androidversion=%%i
echo %INFO%安卓版本:%androidversion%
for /f "delims=" %%i in ('adb shell getprop ro.build.version.sdk') do set sdkversion=%%i
echo %INFO%SDK版本号:%sdkversion%
for /f "delims=" %%i in ('adb shell getprop ro.product.current.softversion') do set version=%%i
echo %INFO%版本号:%version%
if not "%sdkversion%"=="27" (
echo %error%你的安卓版本不是8.1，该功能不支持你的手表
pause
exit /b
)
call isv3
adb shell pm path com.android.systemui > nul 2> nul
if %errorlevel%==0 (
    set havesystemui=1
    ECHO %GREEN%系统存在SystemUI
) else (
    set havesystemui=0
    ECHO %GREEN%系统不存在SystemUI
)
device_check.exe adb&&ECHO.
call boot_completed.bat
ECHO.%INFO%开始安装应用,共计6个
call instapp.bat .\rootproapks\LocalSend.apk
call instapp.bat .\rootproapks\Via.apk
call instapp.bat .\rootproapks\Xposed_Edge_Pro.apk
call instapp.bat .\rootproapks\MTfile.apk
call instapp.bat .\rootproapks\xtcinputpro.apk
call instapp.bat .\rootproapks\sogouwearpro.apk
ECHO.%INFO%安装完成
ECHO.%INFO%你是否需要安装禁用模式切换的桌面？
set /p i13yesorno=%YELLOW%输入y进行安装，按任意键跳过%RESET% 
if not "%i13yesorno%"=="y" goto rootpro-noi13
if "%isv3%"=="1" (
    if "%havesystemui%"=="1" (
        call instapp.bat .\rootproapks\130510_D.apk
    ) else (
        call instapp.bat .\rootproapks\121750_D.apk
    )
) else (
    call instapp.bat .\rootproapks\116100_D.apk
)
:rootpro-noi13
:: ECHO.%INFO%你是否要刷入原生修复？
:: set /p setyesorno=%YELLOW%输入y进行刷入，按任意键跳过%RESET%
:: if not "%setyesorno%"=="y" goto noset
:: ECHO.%INFO%原生修复依赖systemplus付费功能-SystemUl,DocumentsUl和原生设置适配
:: set /p sethookyesorno=%YELLOW%输入y刷入破解模块，输入n打开解锁界面%RESET%
:: if "%sethookyesorno%"=="y" (
:: call instapp.bat .\rootproapks\WeichatPro2.apk
:: call instapp.bat .\rootproapks\SystemPlus.apk
:: call autosystemplus.bat
:: busybox sleep 5
:: ECHO.%INFO%重启手表
:: run_cmd "adb reboot"
:: )
:: run_cmd "adb shell ""su -c am start -n com.huanli233.systemplus/.UnlockActivity"""
:: ECHO.%INFO%请确保已经打开'SystemUl,DocumentsUl和原生设置适配'功能
:: pause
:: ECHO.%INFO%再次确认已经打开该功能，如果不打开该功能会导致变砖
:: pause
:: call instmodule.bat .\magiskmod\xtcrootultra.zip
:: ECHO.%INFO%重启手表
:: run_cmd "adb reboot"
:: ECHO.%INFO%你是否正常启动进入了系统？
:: set /p rmyesorno=%YELLOW%如果变砖了输入y快速救砖，按任意键跳过%RESET%
:: if "%rmyesorno%"=="y" run_cmd "adb shell ""su -c rm -rf /data/adb/modules/xtc_root_ultra""" && run_cmd "adb reboot"
:: if "%havesystemui%"=="0" (
:: ECHO.%INFO%你是否要刷入XTC systemui并安装130510桌面？
:: ECHO.%INFO%！与多任务和导航栏冲突！
:: set /p sysuiyesorno=%YELLOW%输入y进行刷入，按任意键跳过%RESET%
:: if "%sysuiyesorno%"=="y" call instmodule.bat .\magiskmod\xtcsystemui.zip & call instapp.bat .\apks\130510.apk else goto noset
:: if "%i13yesorno%"=="y" call instapp.bat .\rootproapks\130510_D.apk
:: )
:: :noset

ECHO.%INFO%你是否要刷入录制器？
set /p Rcyesorno=%YELLOW%输入y进行刷入，按任意键跳过%RESET%
if "%Rcyesorno%"=="y" call instmodule.bat .\magiskmod\Recorder.zip

ECHO.%INFO%你是否要刷入破解SystemPlus和WeichatPro2？
ECHO.%INFO%～建议支持正版哦～
set /p syswcpyesorno=%YELLOW%输入y进行刷入，按任意键跳过%RESET%
if "%syswcpyesorno%"=="y" (
call instapp.bat .\rootproapks\WeichatPro2.apk
call instapp.bat .\rootproapks\SystemPlus.apk
call autosystemplus.bat
busybox sleep 5
ECHO.%INFO%重启手表
run_cmd "adb reboot"
)
ECHO.%INFO%优化全部完成
ECHO.%INFO%按任意键返回上一页...
pause >nul
exit /b