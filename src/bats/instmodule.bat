call adbdevice adb
adb push %1 /sdcard/temp_module.zip
adb shell "su -c magisk --install-module /sdcard/temp_module.zip"
adb shell rm /sdcard/temp_module.zip