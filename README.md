# spresensewong
spresense玩具

硬件：
spresense 开发板 主板加扩展板
资料：https://developer.sony.com/spresense/development-guides/sdk_set_up_zh
DFROBOT LCD12864 shield 显示屏模块
资料：https://wiki.dfrobot.com.cn/_SKU_DFR0287_LCD12864_shield_%E5%85%BC%E5%AE%B9Arduino

实现功能：
1.帮忙写一套代码使用SPRESENSE开发板控制其板载的 GNSS获取定位数据，来实时显示在屏幕上的落盘、经纬度，做一个码表应用；
参考：https://developer.sony.com/spresense/development-guides/arduino_tutorials_zh#_gps_%E7%A7%BB%E5%8A%A8%E8%B7%9F%E8%B8%AA%E5%99%A8
2.再利用SPRESENSE的音频播放功能，使用扩展板上的SD卡存储高品质音频，做一个MP3播放器，显示屏显示歌曲信息、歌词等，使用DFROBOT的扩展版上的按键来控制播放器选取歌曲、调节音量等功能，菜单里加上上面的码表应用。
参考论坛代码：https://forum.developer.sony.com/category/7104c629-fa1e-46c9-aebd-d8dbf7e58d9f
搭建一套代码框架，写出具体代码。


再改下实现方式：
1.SPRESENSE开发板部分能用SPRESENSE SDK来实现么？不用Arduino？
2.MP3部分和码表部分拆开不同文件来实现；
3.MP3部分添加循环方式，单曲循环还是随机播放；
4.MP3部分添加EQ调整，重低音啊，高音低音可自定义啊这种看板子支持啥；
5.MP3部分的菜单里添加查看内存卡剩余空间功能；
6.MP3部分功能界面添加电池剩余电量展示；
7.常规MP3需要的功能尽量都添加上来;
8.GNSS模块的菜单功能添加定位数据采集频率调整功能；
9.GNSS模块添加经纬度、当前时间、指南针界面；
10.GNSS模块添加行程中的速度、里程计算和展示；
11.GNSS模块需记录详细数据到本地SD卡，待后期添加4G模块后直接上传至远程服务器；
12.GNSS模块需添加路段分析数据，可分析百米加速时间、总计行程等等在菜单上可查看的功能模块。