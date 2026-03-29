FIND_FILE="/data/data/com.solohsu.android.edxp.manager/shared_prefs/enabled_modules.xml"
FIND_STR="com.huanli233.systemplus"
if [ `grep -c "$FIND_STR" $FIND_FILE` -ne '0' ];then
    echo "0"
    exit 0
fi
echo "1"