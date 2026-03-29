:: 颜色定义
for /f %%a in ('echo prompt $E^| cmd') do set "ESC=%%a"

set "BLACK=%ESC%[30m"
set "RED_2=%ESC%[31m"
set "GREEN_2=%ESC%[32m"
set "ORANGE=%ESC%[33m"
set "BLUE_2=%ESC%[34m"
set "MAGENTA=%ESC%[35m"
set "CYAN_2=%ESC%[36m"
set "WHITE_2=%ESC%[37m"

set "GRAY=%ESC%[90m"
set "RED=%ESC%[91m"
set "GREEN=%ESC%[92m"
set "YELLOW=%ESC%[93m"
set "BLUE=%ESC%[94m"
set "PINK=%ESC%[95m"
set "CYAN=%ESC%[96m"
set "WHITE=%ESC%[97m"

::set "ORANGEPRO=%ESC%[38;5;208m"
set "ORANGEPRO=%ESC%[33m"

set "RESET=%ESC%[0m"
set "ERROR=%ESC%[31m[错误]%ESC%[91m"
set "INFO=%ESC%[94m[信息]%ESC%[97m"
set "WARN=%ESC%[33m[警告]%ESC%[93m"
set "SUCCESS=%ESC%[92m[成功]%ESC%[97m"