@echo off
for /l %%i in (1,1,20) do set "p%%i="
set "selected_params="
set "selected_params_zh="
set "record_file="
set "max_fps="
set "record_format=mp4"
set "bit_rate="
set "window_title="
set "crop_size="
set "max_size="

:MAIN_MENU
CLS
call logo.bat
ECHO %ORANGE%SCRCPY投屏菜单%RESET%%YELLOW%
menu.exe .\menu\scrcpy-ui.xml
set /p MENU=<menutmp.txt
if defined selected_params_zh (
    ECHO %CYAN%当前已选参数: !selected_params_zh!%RESET%
    echo.
)
if "%MENU%"=="A" exit /b
if "%MENU%"=="a" exit /b
if "%MENU%"=="C" goto CLEAR_PARAMS
if "%MENU%"=="c" goto CLEAR_PARAMS
if "%MENU%"=="1" goto ADD_NO_CONTROL
if "%MENU%"=="2" goto ADD_TURN_SCREEN_OFF
if "%MENU%"=="3" goto ADD_STAY_AWAKE
if "%MENU%"=="4" goto SET_RECORD
if "%MENU%"=="5" goto ADD_NO_AUDIO
if "%MENU%"=="6" goto ADD_AUDIO
if "%MENU%"=="7" goto ADD_NO_CLIPBOARD_AUTOSYNC
if "%MENU%"=="8" goto ADD_LEGACY_PASTE
if "%MENU%"=="9" goto ADD_SHOW_TOUCHES
if "%MENU%"=="10" goto SET_MAX_FPS
if "%MENU%"=="11" goto ADD_ALWAYS_ON_TOP
if "%MENU%"=="12" goto ADD_FULLSCREEN
if "%MENU%"=="13" goto ADD_WINDOW_BORDERLESS
if "%MENU%"=="14" goto SET_RECORD_FORMAT
if "%MENU%"=="15" goto SET_BIT_RATE
if "%MENU%"=="16" goto SET_CROP
if "%MENU%"=="17" goto SET_WINDOW_TITLE
if "%MENU%"=="18" goto SET_MAX_SIZE
if "%MENU%"=="S" device_check.exe adb & ECHO. & powershell -Command "Start-Process cmd.exe -ArgumentList '/c call scrcpy.bat %p1% %p2% %p3% %p4% %p5% %p6% %p7% %p8% %p9% %p10% %p11% %p12% %p13% %p14% %p15% %p16% %p17% %p18%' -WindowStyle Hidden"
goto MAIN_MENU

:ADD_NO_CONTROL
call :add_param "--no-control" "禁用设备控制"
goto MAIN_MENU

:ADD_TURN_SCREEN_OFF
call :add_param "--turn-screen-off" "启动时关闭设备屏幕"
goto MAIN_MENU

:ADD_STAY_AWAKE
call :add_param "--stay-awake" "保持设备唤醒"
goto MAIN_MENU

:SET_RECORD
cls
ECHO %CYAN%录制屏幕到文件%RESET%
ECHO 请选择录制方式：
ECHO 1. 仅视频
ECHO 2. 视频+音频
ECHO 3. 返回主菜单
ECHO.
set /p choice=%YELLOW%请输入选择：%RESET%
if "%choice%"=="1" (
    set /p record_file=请输入录制保存路径（如：C:\video.mp4）：
    if defined record_file (
        call :add_param "--record=!record_file!" "录制到[!record_file!]"
    )
)
if "%choice%"=="2" (
    set /p record_file=请输入录制保存路径（如：C:\video.mp4）：
    if defined record_file (
        call :add_param "--record=!record_file!" "录制到[!record_file!]（含音频）"
        call :add_param "--audio" "启用音频转发"
    )
)
goto MAIN_MENU

:ADD_NO_AUDIO
call :add_param "--no-audio" "录制时不包含音频"
goto MAIN_MENU

:ADD_AUDIO
call :add_param "--audio" "启用音频转发"
goto MAIN_MENU

:ADD_NO_CLIPBOARD_AUTOSYNC
call :add_param "--no-clipboard-autosync" "禁用剪贴板自动同步"
goto MAIN_MENU

:ADD_LEGACY_PASTE
call :add_param "--legacy-paste" "使用旧版粘贴方式"
goto MAIN_MENU

:ADD_SHOW_TOUCHES
call :add_param "--show-touches" "在设备上显示触摸"
goto MAIN_MENU

:SET_MAX_FPS
cls
ECHO %CYAN%设置最大帧率%RESET%
ECHO 常用帧率：
ECHO 1. 30 FPS（标准）
ECHO 2. 60 FPS（流畅）
ECHO 3. 自定义
ECHO 4. 返回主菜单
ECHO.
set /p choice=%YELLOW%请输入选择：%RESET%
if "%choice%"=="1" (
    call :add_param "--max-fps=30" "最大帧率[30FPS]"
)
if "%choice%"=="2" (
    call :add_param "--max-fps=60" "最大帧率[60FPS]"
)
if "%choice%"=="3" (
    set /p max_fps=请输入帧率值（如：45, 90, 120）：
    if defined max_fps (
        call :add_param "--max-fps=!max_fps!" "最大帧率[!max_fps!FPS]"
    )
)
goto MAIN_MENU

:ADD_ALWAYS_ON_TOP
call :add_param "--always-on-top" "窗口置顶"
goto MAIN_MENU

:ADD_FULLSCREEN
call :add_param "--fullscreen" "全屏启动"
goto MAIN_MENU

:ADD_WINDOW_BORDERLESS
call :add_param "--window-borderless" "无边框窗口"
goto MAIN_MENU

:SET_RECORD_FORMAT
cls
ECHO %CYAN%设置录制格式%RESET%
ECHO 常用格式：
ECHO 1. MP4（默认，兼容性好）
ECHO 2. MKV（支持多音轨）
ECHO 3. 自定义
ECHO 4. 返回主菜单
ECHO.
set /p choice=%YELLOW%请输入选择：%RESET%
if "%choice%"=="1" (
    call :add_param "--record-format=mp4" "录制格式[MP4]"
)
if "%choice%"=="2" (
    call :add_param "--record-format=mkv" "录制格式[MKV]"
)
if "%choice%"=="3" (
    ECHO %CYAN%可选格式：mp4, mkv, m4a, mka, webm%RESET%
    set /p record_format=请输入格式：
    if defined record_format (
        call :add_param "--record-format=!record_format!" "录制格式[!record_format!]"
    )
)
goto MAIN_MENU

:SET_BIT_RATE
cls
ECHO %CYAN%设置视频比特率%RESET%
ECHO 常用比特率：
ECHO 1. 8Mbps（默认，平衡）
ECHO 2. 4Mbps（省流量）
ECHO 3. 16Mbps（高质量）
ECHO 4. 自定义
ECHO 5. 返回主菜单
ECHO.
set /p choice=%YELLOW%请输入选择：%RESET%
if "%choice%"=="1" (
    call :add_param "--bit-rate=8M" "比特率[8Mbps]"
)
if "%choice%"=="2" (
    call :add_param "--bit-rate=4M" "比特率[4Mbps]"
)
if "%choice%"=="3" (
    call :add_param "--bit-rate=16M" "比特率[16Mbps]"
)
if "%choice%"=="4" (
    set /p bit_rate=请输入比特率（如：2M, 10M, 20M）：
    if defined bit_rate (
        call :add_param "--bit-rate=!bit_rate!" "比特率[!bit_rate!]"
    )
)
goto MAIN_MENU

:SET_CROP
cls
ECHO %CYAN%裁剪屏幕区域%RESET%
ECHO 格式：宽度:高度:X坐标:Y坐标
ECHO 示例：800:600:100:100（从坐标100,100裁剪800x600区域）
ECHO.
set /p crop_size=请输入裁剪尺寸（直接回车跳过）：
if defined crop_size (
    call :add_param "--crop=!crop_size!" "裁剪[!crop_size!]"
)
goto MAIN_MENU

:SET_WINDOW_TITLE
cls
ECHO %CYAN%设置窗口标题%RESET%
ECHO 为空则使用设备型号作为标题
ECHO.
set /p window_title=请输入窗口标题（直接回车跳过）：
if defined window_title (
    call :add_param "--window-title=!window_title!" "窗口标题[!window_title!]"
)
goto MAIN_MENU

:SET_MAX_SIZE
cls
ECHO %CYAN%设置显示最大尺寸%RESET%
ECHO 例如：800（限制最大宽度为800，高度按比例调整）
ECHO.
set /p max_size=请输入最大尺寸（像素，直接回车跳过）：
if defined max_size (
    call :add_param "--max-size=!max_size!" "最大尺寸[!max_size!px]"
)
goto MAIN_MENU

:CLEAR_PARAMS
set "selected_params="
set "selected_params_zh="
for /l %%i in (1,1,20) do set "p%%i="
set "record_file="
set "max_fps="
set "record_format=mp4"
set "bit_rate="
set "window_title="
set "crop_size="
set "max_size="
ECHO %SUCCESS%所有参数已清除！%RESET%
timeout /t 2 >nul
goto MAIN_MENU

:add_param
set "param_en=%~1"
set "param_zh=%~2"


set "exists=false"
for %%a in (!selected_params!) do (
    if "%%a"=="!param_en!" set "exists=true"
)

if "!exists!"=="true" (
    ECHO %WARN%参数已存在，跳过添加%RESET%
    timeout /t 2 >nul
    goto :eof
)


if defined selected_params (
    set "selected_params=!selected_params! !param_en!"
) else (
    set "selected_params=!param_en!"
)


if defined selected_params_zh (
    set "selected_params_zh=!selected_params_zh!、!param_zh!"
) else (
    set "selected_params_zh=!param_zh!"
)


set "param_count=0"
for %%a in (!selected_params!) do (
    set /a param_count+=1
    set "p!param_count!=%%a"
)

ECHO %SUCCESS%参数 [!param_zh!] 已添加！%RESET%
timeout /t 2 >nul
goto :eof