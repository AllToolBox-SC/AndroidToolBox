@echo off
:: :roottmp
:: CLS
:: if exist .\roottmp.txt goto rootspcd
call .\color.bat
del /Q /F .\nouserdata.txt >nul 2>nul
if "%1"=="" goto ROOT
set /p="1" <nul > nouserdata.txt
set nouserdata=1
ECHO.%INFO%你选择了不刷userdata，这可能导致设备出现问题
pause
:ROOT
CLS
echo %YELLOW%════════════════════════%RESET%
echo %GREEN_2%一键%ORANGE%ROOT%RESET%
echo %YELLOW%════════════════════════%RESET%
echo %GREEN_2%本工具支持XTC Q1y,Q1S,Q2,Z1,Z1S,Z2,Z3,Z5q,Z5A,Z5Pro,Z6,Z6_DFB,Z7,Z7A,Z7S,Z8,Z8A,Z9,Z9+1手表root%RESET%
echo %INFO%本功能可以完全离线运行%RESET%
pause
echo %ORANGE%免责声明：  
echo %WARN%在使用本工具对XTC电话手表系列进行ROOT、刷机、解除安装限制、安装非官方应用等操作[统称"刷机"]前，您必须仔细阅读并完全理解本声明。一旦您实施或完成刷机行为，即视为您已充分知晓、同意并自愿承担本声明所述的全部风险及责任。  
echo %WARN%本工具仅供学习，交流使用，并非"破解"XTC电话手表系列产品。严禁用于任何形式非法用途。
echo %WARN%本工具不能用于解绑手表，如您通过不正当手段获取的手表请联系公安机关归还手表！
echo.
echo %WARN%1. 设备功能与服务失效风险  
echo %WARN%刷机后您的设备将脱离官方原厂固件，可能导致以下后果：  
echo %WARN%1.1 无法使用广东XTC科技有限公司（以下简称“XTC”）提供的各项官方服务，包括但不限于系统更新、应用商店、家长端管理、定位、通话、支付、安全功能及云服务等；  
echo %WARN%1.2 设备可能出现系统不稳定、功能异常、性能下降、兼容性问题、数据丢失或硬件损坏等风险；  
echo %WARN%1.3 设备自进入9008模式起即刻丧失官方保修资格，XTC无义务恢复因刷机行为所导致的任何功能或服务损失。
echo %WARN%2. 相关用户协议条款援引  
echo %WARN%根据XTC《用户协议》（用户于手表开机时点击确认按钮即视为同意）的约定，以下行为属于明确禁止的范畴，用户须严格遵守：
echo %WARN%【禁止行为】  
echo %WARN%您可在本协议约定的范围内使用XTC产品和服务，不得从事包括但不限于以下行为：
echo.
echo %WARN%（1）复制、变更、反向工程、反汇编、反编译、拆装、企图导出其源代码、解码、其他对修改XTC产品和服务的源代码、构造、构思等进行解析或者复制的行为；  
echo %WARN%（2）删除XTC产品和服务上关于著作权的信息；  
echo %WARN%（3）对XTC产品和服务拥有知识产权的内容进行使用、出租、出借、复制、修改、链接、转载、汇编、发表、出版、建立镜像站点、录屏、剪接等；  
echo %WARN%（4）赠与、借用、租用、转让、售卖、再分发、其他再许可XTC产品和服务软件的相关行为；  
echo %WARN%（5）利用XTC产品和服务发表、传送、传播、储存危害国家安全、国家统一、社会稳定的内容，或侮辱诽谤、色情、暴力、引起他人不安及任何违反国家法律法规政策的内容或者设置含有上述内容的网名、角色名，发布、传送、传播含有上述内容的广告信息、营销信息及垃圾信息等的行为；  
echo %WARN%（6）利用XTC产品和服务侵害他人知识产权、肖像权、隐私权、名誉权等合法权利或权益的行为；  
echo %WARN%（7）恶意虚构事实、隐瞒真相以误导、欺诈他人的行为；  
echo %WARN%（8）进行任何危害计算机网络安全的行为，包括但不限于：进入未经许可访问的服务器/账号/硬件系统存储器或其他XTC和XTC用户存储数据的软硬件；没有访问权限而未经允许进入XTC和XTC用户的计算机网络、计算机系统和存储数据的系统存储器等软硬件设施；未经许可查询、删除、修改、增加存储、下载、使用XTC服务器或用户软硬件设备上的数据；未经许可，企图探查、扫描、测试XTC产品和服务或网络的弱点或其它实施破坏网络安全的行为；企图干涉、破坏XTC产品和服务或网络的正常运行，故意传播恶意程序或病毒以及其他破坏干扰正常网络信息服务的行为；伪造TCP/IP数据包名称或部分名称；利用伪造的IP地址访问XTC服务器等；  
echo %WARN%（9）进行任何破坏XTC提供服务公平性或者其他影响应用正常运行秩序的行为，如主动或被动刷积分，使用外挂或者其他的非法软件、利用BUG（又叫“漏洞”或者“缺陷”）来从XTC产品和服务中获得不正当的利益，或者利用互联网或其他方式将外挂、非法软件提供给他人或公之于众等行为；  
echo %WARN%（10）进行任何诸如发布广告、销售商品的商业行为，或者进行任何非法的侵害XTC利益的行为；  
echo %WARN%（11）从事其他法律法规、政策及公序良俗、社会公德禁止的行为以及侵犯其他个人、公司、社会团体、组织的合法权益的行为。
echo.
echo %WARN%【行为限制】  
echo %WARN%如您违反本协议约定，XTC有权依照业务规则及您的行为性质，采取包括但不限于删除您发布的信息内容、暂停账号使用、终止服务、限制使用、回收XTC账号、追究法律责任等措施。对恶意注册XTC账号或利用XTC账号进行违法活动、捣乱、骚扰、欺骗其他用户以及其他违反本协议的行为，XTC有权回收其账号。以上后果可能对您造成损失，该损失应由您自行承担，XTC不承担任何责任。XTC有权对部分违规行为进行限制。
echo %WARN%2. 功能异常与数据安全  
echo %WARN%刷机可能导致家长端功能异常、设备功能失效、数据错误或丢失。我们对此不承担责任。您需自行完成数据备份并承担全部后果。  
echo.
echo %WARN%3. 使用行为与监护人责任  
echo %WARN%设备经修改后可能具备安装非官方应用或增强网络访问的能力，您须合法合规使用。若因沉迷网络、不当使用应用、接触不良信息导致身心健康受损、财产损失或其他后果，我们不承担任何责任。若设备使用者为未成年人，其监护人须承担完全的监督与管理义务。您应在刷机后48小时内恢复XTC官方系统。  
echo.
echo %WARN%4. 操作自愿性  
echo %WARN%刷机属于您个人自愿行为。我们仅提供技术信息与文件资源，从未主动要求、诱导或强制用户进行任何刷机操作。您须对自身操作及后果负全部责任。  
echo.
echo %WARN%5. 设备所有权与非法解绑  
echo %WARN%5.1 我们严禁且不提供任何形式的“手表强制解绑”服务或技术支持；  
echo %WARN%5.2 您须确认自身为设备合法所有者或已获所有者明确授权。如拾获他人设备，须依法联系公安机关（110）或通过官方途径归还失主；  
echo %WARN%5.3 对非本人所有或未授权设备进行刷机、解除挂失锁、解绑等操作属违法行为，可能构成犯罪。我们严厉谴责此类行为，行为人须自行承担民事赔偿、行政处罚及刑事责任，我们不承担任何关联责任。  
echo.
echo %WARN%6. 责任范围限定  
echo %WARN%我们作为技术爱好者，仅提供技术交流与文件资源（限研究学习目的）。您因使用本平台资源刷机导致的直接或间接损失（包括设备损坏、数据丢失、功能异常、保修失效、第三方索赔等），我们不承担责任。  
echo.
echo %WARN%7. 操作禁令  
echo %WARN%严禁代刷、强迫或教唆他人刷入非官方系统。任何此类行为均违背本声明原则，相关责任由行为人独立承担。  
echo.
echo %WARN%8. 恶意行为责任  
echo %WARN%您承诺不利用修改后的设备实施以下行为：  
echo %WARN%8.1 恶意破坏设备或数据（无论归属）；  
echo %WARN%8.2 未授权访问、窃取、滥用他人个人信息或账户；  
echo %WARN%8.3 实施欺诈、盗窃、网络攻击等违法犯罪；  
echo %WARN%8.4 未授权操作他人账号。  
echo %WARN%因上述行为导致他人财产损失、隐私泄露等后果，行为人须承担全部法律责任及赔偿。我们保留追究其法律责任的权利。  
echo.
echo %WARN%9. 隐私保护禁令  
echo %WARN%获取XTC用户信息属违法行为，请立即卸载非法抓包工具（如HttpCanary）。我们严禁任何侵犯隐私行为，违者将依法承担法律责任。  
echo.
ECHO %INFO%请在阅读完毕《免责声明》，并同意自行承担一切后果后按任意键继续%RESET%
pause >nul
echo %WARN%请确认你的手表已经拔出sim卡%RESET%
pause
echo %INFO%你可以用以下两种方案你把你的设备连接到电脑%RESET%
echo %BLUE%[提示]%WHITE%1.手表打开拨号盘输入*#0769651#*打开ADB开关,随后用数据线连接电脑%RESET%
echo %BLUE%[提示]%WHITE%2.打开手表卡槽,用金属物品短接触点,随后用数据线连接电脑%RESET%
echo %YELLOW%════════════════════════════════════════════════%RESET%
ECHO.%WARN%如果提示未授权、离线等请重新连接
echo %YELLOW%════════════════════════════════════════════════%RESET%
device_check.exe adb qcom_edl&&ECHO.
busybox sleep 2
for /f "delims=" %%i in ('type tmp.txt') do set devicestatus=%%i
if not "%devicestatus%"=="qcom_edl" goto adb_run
:EDL_run
CLS
call logo
echo %ORANGE%请选择需要root的型号%YELLOW%
menu.exe .\menu\root.xml
set /p MENU=<menutmp.txt
if "%MENU%"=="1" set innermodel=I12&&call qmmi otherpash
if "%MENU%"=="2" set innermodel=IB&&call qmmi otherpash
if "%MENU%"=="3" set innermodel=I13C&&call qmmi otherpash
if "%MENU%"=="4" set innermodel=I13&&call qmmi otherpash
if "%MENU%"=="5" set innermodel=I19&&call qmmi otherpash
if "%MENU%"=="6" set innermodel=I18&&call qmmi otherpash
if "%MENU%"=="7" set innermodel=I20&&call qmmi v3pash
if "%MENU%"=="8" set innermodel=I25&&call qmmi v3pash
if "%MENU%"=="9" set innermodel=I25C&&call qmmi v3pash
if "%MENU%"=="10" set innermodel=I25D&&call qmmi v3pash
if "%MENU%"=="11" set innermodel=I32&&call qmmi v3pash
if "%MENU%"=="12" set innermodel=ND07&&call qmmi v3pash
if "%MENU%"=="13" set innermodel=ND01&&call qmmi v3pash
if "%MENU%"=="14" set innermodel=ND03&&echo.%info%你选择了ND03，即将为你开始root&&call nd03root&&exit /b
ECHO %ERROR%输入错误，请重新输入！%RESET%
timeout /t 2 >nul
goto EDL_run
:adb_run
echo %INFO%等待adb连接...
device_check.exe adb&&ECHO.
call adbdevice.bat more
for /f "delims=" %%i in ('adb shell getprop ro.product.innermodel') do set innermodel=%%i
echo %INFO%您的设备innermodel为:%innermodel%
del /Q /F .\smodel.txt >nul 2>nul
del /Q /F .\innermodel.txt >nul 2>nul
set /p="%innermodel%" <nul > innermodel.txt
if "%innermodel%"=="I25C" (
   set smodel=1
   set /p="1" <nul > smodel.txt
   echo %WARN%此型号ROOT可能存在不稳定性问题，是否继续？
   pause
)
if "%innermodel%"=="ND03" (
   echo.%info%检测到型号为ND03,即将为你开始root
   adb reboot edl
   call nd03root
   exit /b
)
for /f "delims=" %%i in ('adb shell getprop ro.product.model') do set model=%%i
echo %INFO%手表型号:%model%
for /f "delims=" %%i in ('adb shell getprop ro.build.version.release') do set androidversion=%%i
echo %INFO%安卓版本:%androidversion%
for /f "delims=" %%i in ('adb shell getprop ro.build.version.sdk') do set sdkversion=%%i
echo %INFO%SDK版本号:%sdkversion%
for /f "delims=" %%i in ('adb shell getprop ro.product.current.softversion') do set version=%%i
echo %INFO%版本号:%version%
call isv3
del /Q /F .\isv3.txt >nul 2>nul
set /p="%isv3%" <nul > isv3.txt
del /Q /F tmp.txt >nul 2>nul
del /Q /F .\*.img >nul 2>nul
del /Q /F .\tmp\boot.img >nul 2>nul
del /Q /F .\header >nul 2>nul
del /Q /F .\kernel_dtb >nul 2>nul
del /Q /F .\kernel >nul 2>nul
del /Q /F .\ramdisk.cpio >nul 2>nul
del /Q /F .\port_trace.txt >nul 2>nul
del /Q /F .\EDL\rooting\*.* >nul 2>nul
rd /Q /S .\EDL\rooting\xtcpatch >nul 2>nul
rd /Q /S .\EDL\rooting\magiskfile >nul 2>nul
md .\EDL\rooting 1>nul 2>>nul
ECHO %INFO%正在为您从本地拷贝文件
ECHO %INFO%文件更新时间：
type "%cd%\EDL\time.txt"
if %errorlevel% neq 0 (
   ECHO %WARN%未下载过root文件，正常没有影响
)
copy /Y "%cd%\EDL\%innermodel%.zip" "%cd%\EDL\rooting\root.zip"
if %errorlevel% neq 0 (
   ECHO %WARN%找不到文件，可能是不支持的型号
   ECHO %INFO%按任意键退出
   pause >nul
   exit /b
)
ECHO %INFO%开始解压文件
7z x EDL\rooting\root.zip -o.\EDL\rooting -aoa >nul 2>&1
if %errorlevel% neq 0 (
   ECHO %ERROR%解压文件时出现错误，错误值:%errorlevel%
   ECHO %INFO%按任意键退出
   pause >nul
   exit /b
)
if "%sdkversion%"=="19" (
call ROOT-SDK19
exit /b
)
if "%sdkversion%"=="25" (
call ROOT-SDK25
exit /b
)
if "%sdkversion%"=="27" (
call ROOT-SDK27
exit /b
)
if "%androidversion%"=="11" (
ECHO %INFO%触发彩蛋：安卓11！
)
ECHO.%ERROR%出错了，不支持的机型
pause
exit /b

:: :rootspcd
:: echo.%WARN%“断点续刷”是一个测试性功能
:: echo.%WARN%可能会出现未知问题，如果出现问题，请超级恢复
:: echo.%YELLOW%你上一次的一键root可能没有完成，需要从断开位置继续吗?
:: set /p rootyesno=%YELLOW%输入yes继续输入no将删除未完成记录：%RESET%
:: set /p roottmp=<roottmp.txt
:: if "!rootyesno!"=="yes" goto rootspcd-yes
:: if "!rootyesno!"=="no" del /Q /F .\roottmp.txt
:: if "!rootyesno!"=="y" goto rootspcd-yes
:: if "!rootyesno!"=="n" del /Q /F .\roottmp.txt
:: goto roottmp
:: :rootspcd-yes
:: device_check.exe adb qcom_edl fastboot&&ECHO.
:: for /f "delims=" %%i in ('type tmp.txt') do set devicestatus=%%i
:: if "%devicestatus%"=="qcom_edl" (
:: ECHO.%INFO%获取9008端口...
:: call edlport
:: )
:: goto !roottmp!