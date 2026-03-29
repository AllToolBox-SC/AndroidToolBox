@echo off
setlocal enabledelayedexpansion
set "scrcpy_params="
set "param_count=0"


:read_params
set /a param_count+=1
if defined p%param_count% (
    set "param_value=!p%param_count%!"
    if defined param_value (
        set "scrcpy_params=!scrcpy_params! !param_value!"
    )
    goto read_params
)


if defined scrcpy_params (
    set "scrcpy_params=!scrcpy_params:~1!"
)

scrcpy.exe !scrcpy_params!