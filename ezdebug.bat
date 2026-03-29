@echo off
setlocal enabledelayedexpansion
chcp 65001
if "%1"=="/mode:full" (
    goto :init
    
) else if "%1"=="/mode:minimize" (
    goto :init
) else if "%1"=="" (
    goto :init
) else (
    echo Usage:
    set FILENAME=%~n0%~x0
    echo !FILENAME! ^[/mode: ^{full ^| minimize ^(default^)^}^]
    echo.
    echo ERROR: No command.
    goto :end
)

:init
setlocal
echo Generating debug source...
if not exist source (
    mkdir source
)
if not exist .\source\bin (
    mkdir .\source\bin
)
copy /y .\src\start.py .\source\bin\main.py 
copy /y .\src\run_cmd.py .\source\bin\run_cmd.py
copy /y .\src\repair.py .\source\bin\repair.py
if "%1"=="/mode:full" (
    g++.exe -Wall -static -g ./src/launch.cpp ./build/icon.o -municode -o ./source/launch.exe -finput-charset=UTF-8 -fexec-charset=GBK -lstdc++ -lpthread -Og
    cargo build --target-dir ./build/rust
    copy .\build\rust\debug\jsonutil.exe .\source\bin\jsonutil.exe
    copy .\build\rust\debug\jsonutil.pdb .\source\bin\jsonutil.pdb
    copy .\build\rust\debug\lolcat.exe .\source\bin\lolcat.exe
    copy .\build\rust\debug\lolcat.pdb .\source\bin\lolcat.pdb
    7z x -y bin.7z -o.\source\bin\
    xcopy .\src\bats .\source\bin /S /E /Y
    cd source
    start cmd.exe /c "gdb launch.exe & pause"
)
endlocal & goto :end

:end
endlocal
goto :EOF
