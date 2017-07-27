@echo off

adb push inject /data/local/tmp
adb shell sh -c 'chmod 755 /data/local/tmp/inject'

adb push libpass.so /data/local/tmp
adb shell su -c '/data/local/tmp/inject com.venustv.myapplication /data/local/tmp/libpass.so init'

pause