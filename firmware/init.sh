安装SDK
https://developer.sony.com/spresense/development-guides/sdk_set_up_zh


# 安装驱动后
ls /dev/{tty,cu}.*


# 刷入引导
最新下载链接

https://sonydw-prod-dupont-files.s3.eu-west-1.amazonaws.com/2vkl4atWlo/uploads/sites/19/2025/03/spresense-binaries-v3.4.1.zip?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Content-Sha256=UNSIGNED-PAYLOAD&X-Amz-Credential=AKIAXTMQWOK7L6M5DQNV%2F20250512%2Feu-west-1%2Fs3%2Faws4_request&X-Amz-Date=20250512T135744Z&X-Amz-Expires=3600&X-Amz-Signature=534999448e5cf2c40e5df750496d7b2553d7eca1a0ff5c43c9b990c159ea4a1f&X-Amz-SignedHeaders=host&x-id=GetObject

https://sonydw-prod-dupont-files.s3.eu-west-1.amazonaws.com/2vkl4atWlo/uploads/sites/19/2025/03/spresense-binaries-v3.4.1.zip?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Content-Sha256=UNSIGNED-PAYLOAD&X-Amz-Credential=AKIAXTMQWOK7L6M5DQNV%2F20250512%2Feu-west-1%2Fs3%2Faws4_request&X-Amz-Date=20250512T135744Z&X-Amz-Expires=3600&X-Amz-Signature=534999448e5cf2c40e5df750496d7b2553d7eca1a0ff5c43c9b990c159ea4a1f&X-Amz-SignedHeaders=host&x-id=GetObject
# 刷入
cd /Users/wangcw/Documents/github/spresensewong/firmware/
sh /Users/wangcw/Documents/github/spresense/sdk/tools/flash.sh -e /Users/wangcw/Documents/github/spresensewong/firmware/spresense-binaries-v3.4.1.zip

cd /Users/wangcw/Documents/github/spresense/sdk

sh /Users/wangcw/Documents/github/spresense/sdk/tools/flash.sh -l /Users/wangcw/Documents/github/spresense/firmware/spresense -c /dev/cu.SLAB_USBtoUART

./tools/flash.sh -l /Users/wangcw/Documents/github/spresense/firmware/spresense -c /dev/cu.SLAB_USBtoUART

✦ 22:03:22 ➜ cd /Users/wangcw/Documents/github/spresense/sdk
✧ github/spresense/sdk
✦ 22:04:08 ➜ ./tools/flash.sh -l /Users/wangcw/Documents/github/spresense/firmware/spresense -c /dev/cu.SLAB_USBtoUART
>>> Install files ...
install -b 115200
Install /Users/wangcw/Documents/github/spresense/firmware/spresense/loader.espk
|0%-----------------------------50%------------------------------100%|
######################################################################

130080 bytes loaded.
Package validation is OK.
Saving package to "loader"
updater# install -b 115200
Install /Users/wangcw/Documents/github/spresense/firmware/spresense/AESM.espk
|0%-----------------------------50%------------------------------100%|
######################################################################

28944 bytes loaded.
Package validation is OK.
Saving package to "AESM"
updater# install -b 115200
Install /Users/wangcw/Documents/github/spresense/firmware/spresense/gnssfw.espk
|0%-----------------------------50%------------------------------100%|
######################################################################

454528 bytes loaded.
Package validation is OK.
Saving package to "gnssfw"
updater# install -b 115200
Install /Users/wangcw/Documents/github/spresense/firmware/spresense/dnnrt-mp.espk
|0%-----------------------------50%------------------------------100%|
######################################################################

109808 bytes loaded.
Package validation is OK.
Saving package to "dnnrt-mp"
updater# install -b 115200
Install /Users/wangcw/Documents/github/spresense/firmware/spresense/mp3dec.spk
|0%-----------------------------50%------------------------------100%|
######################################################################

48016 bytes loaded.
Package validation is OK.
Saving package to "mp3dec"
updater# install -b 115200
Install /Users/wangcw/Documents/github/spresense/firmware/spresense/sysutil.spk
|0%-----------------------------50%------------------------------100%|
######################################################################

283152 bytes loaded.
Package validation is OK.
Saving package to "sysutil"
updater# sync
updater# Restarting the board ...
reboot