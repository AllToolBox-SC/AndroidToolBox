call adbdevice adb
adb push %1 /sdcard/temp_module.zip
adb shell "echo -e 'chmod 777 /data/adb/magisk/busybox\nDATABIN=\"/data/adb/magisk\"\nBBPATH=\"/data/adb/magisk/busybox\"\nUTIL_FUNCTIONS_SH=\"$DATABIN/util_functions.sh\"\nexport OUTFD=1\nexport ZIPFILE=\"/sdcard/temp_module.zip\"\nexport ASH_STANDALONE=1\n\"$BBPATH\" sh -c \". \\\"$UTIL_FUNCTIONS_SH\\\"; install_module\"' > /sdcard/temp_module_installer.sh"
adb shell su -c "sh /sdcard/temp_module_installer.sh"
adb shell rm /sdcard/temp_module.zip