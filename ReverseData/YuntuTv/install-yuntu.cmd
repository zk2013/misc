@echo off

adb push inject /data/local/tmp
adb shell sh -c 'chmod 755 /data/local/tmp/inject'

adb push libyuntu.so /data/local/tmp
adb shell su -c '/data/local/tmp/inject org.fungo.fungolive /data/local/tmp/libyuntu.so init'

pause