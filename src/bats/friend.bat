@echo off
echo.%WARN%免责声明及风险须知：
echo.%ERROR%该功能已经被XTC制裁掉了，现在只有你和被加的好友在同一个群里才能加
echo.%WARN%本工具仅为个人合法使用场景设计，仅限添加已知并且对方已回意的好友严禁用于恶意骚扰、侵犯他人隐私、违反平台用户协议及相关法律法规的非法用途，任何因滥用本工具导致的账号封禁、法律责任、民事纠纷等一切后果，均由使用者自行承担，我们并未强求您使用本工具，请不要为他人进行操作，即使他人已同意，如果您仍然在帮助他人此工具，若遇到以上行为都与工具开发者无关我方不会将您的账号上传到云端并且泄露，如果遇到账号泄露，与我方无任何关系，需由手表机主及其监护人自行承担%RESET%
echo.%INFO%请看完免责声明及风险须知后按任意键继续
pause >nul

cls
echo %INFO%准备提取设备基础信息%RESET%
device_check.exe adb&&ECHO.
for /f "tokens=*" %%a in ('adb shell getprop persist.sys.watch.id') do set "PROP7=%%a"
adb shell getprop persist.sys.watch.id > friend\watchid.txt
for /f "tokens=*" %%a in ('adb shell "content query --uri content://com.xtc.provider/BaseDataProvider/watchId/1 --projection openID | cut -d= -f2"') do set "PROP8=%%a"
adb shell "content query --uri content://com.xtc.provider/BaseDataProvider/watchId/1 --projection openID | cut -d= -f2" > friend\openid.txt
for /f "tokens=*" %%a in ('adb shell getprop ro.boot.bindnumber') do set "PROP4=%%a"
adb shell getprop ro.boot.bindnumber > friend\bindnumber.txt
for /f "tokens=*" %%a in ('adb shell getprop persist.sys.serverinner') do set "PROP2=%%a"
adb shell getprop persist.sys.serverinner > friend\serverinner.txt

echo %INFO%已获取设备信息，存在bin文件夹中的friend文件夹，你可随时删除或修改，若删除后，需要重新提取%RESET%

echo %INFO%正在读取配置文件...%RESET%


set "BIND_FILE=friend\bindnumber.txt"
set "ACCOUNTID_FILE=friend\watchid.txt"
set "CHIPID_FILE=friend\openid.txt"
set "MODEL_FILE=friend\serverinner.txt"


if not exist "%BIND_FILE%" (
    echo %ERROR%未找到 %BIND_FILE% 文件%RESET%
    pause
    exit /b
)
set /p BIND=<"%BIND_FILE%"
if "!BIND!"=="" (
    echo %ERROR%%BIND_FILE% 文件内容为空%RESET%
    pause
    exit /b
)

if not exist "%ACCOUNTID_FILE%" (
    echo %ERROR%未找到 %ACCOUNTID_FILE% 文件%RESET%
    pause
    exit /b
)
set /p ACCOUNTID=<"%ACCOUNTID_FILE%"
if "!ACCOUNTID!"=="" (
    echo %ERROR%%ACCOUNTID_FILE% 文件内容为空%RESET%
    pause
    exit /b
)

if not exist "%CHIPID_FILE%" (
    echo %ERROR%未找到 %CHIPID_FILE% 文件%RESET%
    pause
    exit /b
)
set /p CHIPID=<"%CHIPID_FILE%"
if "!CHIPID!"=="" (
    echo %ERROR%%CHIPID_FILE% 文件内容为空%RESET%
    pause
    exit /b
)

if not exist "%MODEL_FILE%" (
    echo %ERROR%未找到 %MODEL_FILE% 文件%RESET%
    pause
    exit /b
)
set /p MODEL=<"%MODEL_FILE%"
if "!MODEL!"=="" (
    echo %ERROR%%MODEL_FILE% 文件内容为空%RESET%
    pause
    exit /b
)

echo %INFO%配置文件读取成功！%RESET%
echo.

:: 输入好友ID
set /p "IDS=%YELLOW%请输入被加好友的天才号（用英文逗号分隔）：%RESET%"
if "!IDS!"=="" (
    echo %ERROR%请输入有效的好友ID！%RESET%
    pause
    exit /b
)

:: 合规警告
echo.
echo %WARN%本工具仅用于合法添加已知好友，严禁用于违法违规用途，滥用后果自负！%RESET%
echo.

:: 设置请求URL和固定头部
set "URL=https://watch.okii.com/watchfriend/group/audit"
set "BASE_REQUEST_PARAM={\"accountId\":\"!ACCOUNTID!\",\"appId\":\"2\",\"deviceId\":\"!BIND!\",\"imFlag\":\"1\",\"mac\":\"unknown\",\"program\":\"watch\",\"registId\":0,\"timestamp\":\"2024-08-12 14:43:48\",\"token\":\"!CHIPID!\"}"


set "ID_COUNT=0"
for %%i in (!IDS!) do (
    set /a ID_COUNT+=1
    set "ID[!ID_COUNT!]=%%i"
)


echo %INFO%开始处理好友请求...%RESET%
echo.

for /l %%n in (1,1,!ID_COUNT!) do (
    set "FRIEND_ID=!ID[%%n]!"
    
    echo %INFO%正在处理好友ID: !FRIEND_ID! %RESET%
    

    set "REQUEST_BODY={\"friendId\":\"!FRIEND_ID!\",\"groupName\":\"我的好友\",\"imDialogId\":0,\"type\":0,\"verification\":\"\",\"watchId\":\"!ACCOUNTID!\"}"
    
    :: 使用curl发送POST请求
    curl -k -X POST "%URL%" ^
        -H "model: !MODEL!" ^
        -H "imSdkVersion: 102" ^
        -H "packageVersion: 55230" ^
        -H "packageName: com.xtc.bleaddfriend" ^
        -H "Eebbk-Sign: 0" ^
        -H "Base-Request-Param: !BASE_REQUEST_PARAM!" ^
        -H "dataCenterCode: CN_BJ" ^
        -H "Version: W_1.5.0" ^
        -H "Grey: 0" ^
        -H "Accept-Language: zh-CN" ^
        -H "Watch-Time-Zone: GMT+08:00" ^
        -H "Content-Type: application/json; charset=UTF-8" ^
        -H "Host: watch.okii.com" ^
        -H "Connection: Keep-Alive" ^
        -H "Accept-Encoding: gzip" ^
        -H "User-Agent: okhttp/3.12.0" ^
        -d "!REQUEST_BODY!"
    if errorlevel 1 echo %ERROR%请求失败：网络错误%RESET%
    
    echo.
    
    if %%n lss !ID_COUNT! (
        echo %INFO%等待3秒...%RESET%
        timeout /t 3 /nobreak >nul
        echo.
    )
)
del /Q ".\friend\*.*"
echo %INFO%所有操作执行完毕！%RESET%
pause