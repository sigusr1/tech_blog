---
title: "Android模拟器如何remount"
date: 2026-03-20
categories: [Android]
tags:  [环境搭建]
---

Android开发中，如果要调试预装service，有两种方式：
- 将apk打包到系统镜像中，重新刷机
- 将现有系统的rootfs remount成可读写的，然后把apk push到`/system/priv-app/`目录

本文介绍如何remount Android模拟器。

不知道从哪个版本开始，Android模拟器默认不允许remount了。即使能adb root也不行，会报下面的错误：
```
Device must be bootloader unlocked
```


真的没有办法了吗？

根据[官方文档](https://developer.android.com/studio/run/emulator-commandline)，可以通过下面的方式remount模拟器：

- 在Android Studio中正常创建模拟器，注意必须是userdebug版本，user版本不行
- 执行 `~/Library/Android/sdk/emulator/emulator -list-avds` 查看已经创建的模拟器，其中`emulator`是Android Studio自带命令，不同平台路径不同。执行这个命令，我的输出如下：
  ```
  Pixel_6
  Pixel_9
  ```

- 用命令行启动模拟器：下面的命令，我启动了Pixel_6，注意关键参数**-writable-system**：
  ```
  emulator -avd Pixel_6 -netdelay none -netspeed full -grpc-use-token  -writable-system
  ```

- 依次执行下面的命令remount模拟器：
  ```
adb root
adb remount
adb reboot
adb remount
  ```

- 上述命令执行完毕，就成功remount了。如果要安装开机自启service，将apk push到`/system/priv-app/`目录，然后重启设备就可以了：
  ```
adb push ~/Desktop/app-debug.apk /system/priv-app/
adb reboot
  ```
