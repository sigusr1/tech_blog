---
title: "如何用模拟器调试Android Framework"
date: 2026-04-18
categories: [Android]
tags:  [环境搭建]
---


近期在看Android上的某个新特性，需要自行搭建环境编译镜像，记录下折腾过程。

在Android模拟器上调试apk是很方便的，调试Framework就没那么方便了。本文介绍如何在模拟器上调试Framework。由于手头资源受限，我的调试环境如下：
- Ubuntu 20.04上下载AOSP源码，交叉编译**arm64**镜像
- Mac M4 Pro上运行模拟器加载自行编译的镜像

## 1. 下载AOSP源码

下载编译Android源码的Host机器，需满足以下条件：
- Ubuntu 20.04及以上
- 可用磁盘空间**300GB以上**（我就是被这一条卡住了，否则直接本机编译本机运行会方便很多）
- 能翻墙从Google官网下载，如果不能，需使用国内镜像

下载AOSP代码，这里指定了具体的分支**android-16.0.0_r4**：
```
repo init -u https://android.googlesource.com/platform/manifest -b android-16.0.0_r4
repo sync -c --no-tags --no-clone-bundle
```

接下来是漫长等待，估计要大半天才能下载好。一方面看你的网络好不好，另一方面是Google或者其他镜像站点都有限流机制。

## 2. 编译

注意lunch的参数：
- **sdk_phone64_arm64**：product的名字，我编译的是arm64的，`list_products`可查看所有支持的product
- **userdebug**：版本类型，`list_variants`可查看所有类型
- **bp4a**：版本代号，`list_releases`可查看全部的，从[Android版本信息](https://source.android.com/docs/setup/reference/build-numbers)可以查到bp4a对应android-16.0.0_r4
```
source build/make/envsetup.sh
lunch sdk_phone64_arm64-bp4a-userdebug
m -j64
```

接下来又是漫长的等待，看个人机器性能。我的机器由于是机械硬盘，io不行，这一步又是大半天。


## 3. 打包镜像

由于要在别的机器上使用这个镜像，按照[官方文档](https://source.android.com/docs/setup/test/avd#share)的说明打包：

```
make emu_img_zip
```

这个过程还是比较快的，成功后，会生成下面的压缩包：
```
out/target/product/emu64a/sdk-repo-linux-system-images.zip
```

## 4. 创建模拟器

官方给的共享方案是在Android Studio的SDK Manager中添加[SDK Update Sites](https://developer.android.com/studio/intro/update#adding-sites)，需要将sdk-repo-linux-system-images.zip和repo-sys-img.xml托管为http服务。问题是**现在没有命令可以生成repo-sys-img.xml了，需要自己找模版手写！！！**

我没有用这种方法，直接狸猫换太子：先在Android Studio中创建对应版本的模拟器（创建过程中根据提示下载官网镜像），然后在模拟器详情页面找到镜像存储路径，用自己的镜像替换。
如下图所示，镜像路径是相对目录`system-images/android-36.1/google_apis/arm64-v8a`, MacOS上全路径是`~/Library/Android/sdk/system-images/android-36.1/google_apis/arm64-v8a`。

![镜像路径](/assets/images/2026-04-18-android_framework_debug/emulator_image_path.jpg)

替换后，启动模拟器，通过下面的命令查看编译时间，确认是否替换成功：

```
adb shell getprop ro.vendor_dlkm.build.date
```

这个过程还是有点麻烦的。后续每次修改，都要这样搞吗？  
如果改动不涉及对外接口，大部分情况是可以**局部替换**的，下面介绍我经常使用的两种场景。

## 5. framework.jar如何替换
比如修改了`frameworks/base/core/java/android/app/Activity.java`，首先重编它所属的模块：
```
make -j64 framework-minus-apex
```

生成的产物是`out/target/product/emu64a/system/framework/framework.jar`。
先根据[Android模拟器如何remount](https://tech.coderhuo.tech/posts/remount_android/)把模拟器remount，然后push到设备：

```
adb push framework.jar /system/framework/
```

保险起见，删除AOT预编译产物：

```
rm -rf /system/framework/arm
rm -rf /system/framework/arm64
```

然后重启system_server，或者重启设备：

```
killall system_server
```

## 6. 二进制so如何替换
比如修改了`system/core/libutils/Looper.cpp`, 首先编译：
```
m -j64 libutils
```

产物是`out/target/product/emu64a/system/lib64/libutils.so`。

remount设备后直接push，然后重启设备即可：
```
adb push libutils.so /system/lib64/
```

## 7. 参考文档

- https://source.android.com/docs/setup/test/avd
- https://developer.android.com/studio/intro/update#adding-sites
- https://tech.coderhuo.tech/posts/remount_android

