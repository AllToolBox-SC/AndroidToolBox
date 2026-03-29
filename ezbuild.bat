@echo off
chcp 65001
set PYTHONUTF8=1
python -m pip install -r requirements.txt
python build.py %1 %2 %3 %4 %5 %6 %7 %8 %9
pause
