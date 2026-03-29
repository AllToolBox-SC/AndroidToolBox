if "%innermodel%"=="ND07" (
    if %version% GEQ 1.5.1 (
        ECHO.%GREEN%你的手表是V3版本%RESET%
        set isv3=1
    ) else (
        ECHO.%GREEN%你的手表不是V3版本%RESET%
        set isv3=0
    )
)
if "%innermodel%"=="ND01" (
    if %version% GEQ 3.3.2 (
        ECHO.%GREEN%你的手表是V3版本%RESET%
        set isv3=1
    ) else (
        ECHO.%GREEN%你的手表不是V3版本%RESET%
        set isv3=0
    )
)
if "%innermodel%"=="I32" (
    if %version% GEQ 3.1.0 (
        ECHO.%GREEN%你的手表是V3版本%RESET%
        set isv3=1
    ) else (
        ECHO.%GREEN%你的手表不是V3版本%RESET%
        set isv3=0
    )
)
if "%innermodel%"=="I25D" (
    if %version% GEQ 1.5.8 (
        ECHO.%GREEN%你的手表是V3版本%RESET%
        set isv3=1
    ) else (
        ECHO.%GREEN%你的手表不是V3版本%RESET%
        set isv3=0
    )
)
if "%innermodel%"=="I25C" (
    if %version% GEQ 1.9.1 (
        ECHO.%GREEN%你的手表是V3版本%RESET%
        set isv3=1
    ) else (
        ECHO.%GREEN%你的手表不是V3版本%RESET%
        set isv3=0
    )
)
if "%innermodel%"=="I25" (
    if %version% GEQ 2.5.1 (
        ECHO.%GREEN%你的手表是V3版本%RESET%
        set isv3=1
    ) else (
        ECHO.%GREEN%你的手表不是V3版本%RESET%
        set isv3=0
    )
)
if "%innermodel%"=="I20" (
    if %version% GEQ 2.8.1 (
        ECHO.%GREEN%你的手表是V3版本%RESET%
        set isv3=1
    ) else (
        ECHO.%GREEN%你的手表不是V3版本%RESET%
        set isv3=0
    )
)