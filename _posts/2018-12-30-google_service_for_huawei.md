---
title:  "华为手机安装Google服务的方法"  
date:   2018-12-30  
categories: [工具]
tags: [环境搭建, Google]
---

近期换了华为手机，系统是EMUI 8.2.0，在安装google服务的时候颇费了一番周折。


之前能用的一些google服务安装器在该系统上均无法工作，最后从应用汇上下载了一个gms安装软件，可是华为系统提示“该应用会破坏系统，禁止安装”。

开启设置里面的**未知来源应用下载**，并禁用**外部来源应用检查**，还是不允许安装。

后来从[花粉俱乐部](https://cn.club.vmall.com/thread-15131985-1-3.html)了解到可以通过以下方式安装：

1. 把该apk拷贝到电脑上
2. 进入设置→开发人员选项，关闭**监控ADB安装应用**
3. 通过adb安装应用。（也可以在USB连接的情况下通过360手机助手、豌豆荚等软件安装该应用）


安装完成后手机会重启，然后根据提示开启google服务的各项权限，后面再配合vpn（本人使用的是影梭，Shadowsocks），就可以方便使用google系列应用。

可以从下面的链接下载gms安装器：
[gms.apk](/assets/images/2018-12-30-google_service_for_huawei/gms.apk)

## 参考资料 ##
1. [https://cn.club.vmall.com/thread-15131985-1-3.html](https://cn.club.vmall.com/thread-15131985-1-3.html)