---
title:  "利用空口抓包分析Wi-Fi问题"  
date:   2018-11-29  
categories: [网络]
tags: [空口抓包]  
---

* content
{:toc}
随着IoT的兴起，越来越多的嵌入式设备内置Wi-Fi模块，具备了网络接入能力。    
在开发过程中，难免会遇到各种各样的网络问题，而抓包无疑是分析网络问题最直接、最有效的手段。因为通过抓包可以明确问题是处于发送端还是接收端，迅速缩小排查范围。    
然而，许多嵌入式设备上运行的可能不是Linux系统，而是一些实时操作系统（RTOS），甚至根本就没有操作系统。而和设备通信的对端也不一定能运行tcpdump或者wireshark等抓包工具。这使得常见的点对点抓包手段失效。







## 一、空口抓包 ##
### 1. 原理 ###

我们知道，无线网络信号在传播过程中是以发射点为中心，像波纹一样往外辐射。所以理论上来讲，如果一个接收器处于无线信号经过的地方，它可以收到（“听到”）任何经过它的信号，只是它可能“听不懂”（无法解析报文内容）。    
以下图为例，IPAD如果和最左侧的台式机通信，MacBook完全有能力监听他们的通信。


![无线网络拓扑](/2018-11-29-wifi_package_capture/network_topology.png?raw=true)

空口抓包就是基于这个原理工作的。如果我们想要抓某个嵌入式设备的无线报文，只需在它附近运行一个具有监听功能的PC。


### 2. 操作方法 ### 

在Windows上可以使用Omnipeek，但是该软件需要特殊无线网卡支持，还需要特殊的驱动。具体安装方法、操作步骤可以参考Omnipeek官网介绍。    

在Linux上可以用aircrack。下面简单介绍Ubuntu 16.04上的操作方法：



1. 确认网卡是否支持monitor模式，输入`iw list`命令，如果输出中有monitor说明支持。否则无法进行空口抓包

	```
	software interface modes (can always be added):
		 * AP/VLAN
		 * monitor
	```

2. 安装aircrack工具集(aircrack包含一系列工具)    
	```
	sudo apt-get install aircrack-ng
    ```    

3. 环境清理，主要是停止一些影响抓包的服务    
	```
	sudo airmon-ng check kill
	```
4. 建立虚拟监听网卡    
	```
	sudo airmon-ng start wlp2s0 4
	```

	- 上面命令中的wlp2s0为无线网卡名称，4为信道号。    
	- 上面的命令执行后，将生成一个虚拟网卡wlp2s0mon，该网卡处于monitor模式，并且监听信道4。    
	- 如果上面的命令不加通道号，则监听所有信道。（airodump-ng工具会以一定的频率扫描信道）。    
5. 通过airodump-ng抓包    
	```
	sudo airodump-ng -c 4 wlp2s0mon –w huo.pcap
	```
	- 上面命令指明只监听4信道，如果不加-c参数，airodump-ng默认以一定的周期扫描所有信道，这样会出现漏抓报文的情况。所以建议指定信道抓包。    

	- -w参数可以将抓包文件写入文件，该文件可以用wireshark或者Omnipeek打开分析，建议使用Omnipeek软件，因为该软件分析无线报文功能更强大。

	但是airodump-ng不会把信号强度、信道号、速率等信息写入抓包文件中，在部分场景下会影响问题定位。可以使用tcpdump解决该问题。只需要将步骤5中的命令换成下面的命令即可(tcpdump命令的其他选项仍然适用)：
	```
	sudo tcpdump -i wlp2s0mon –w huo.pcap
	```


## 二、实例分析 ##
### 1. 手机兼容性问题 ###


在项目中发现设备和不同品牌手机对接时，用iperf测试吞吐量差异较大。    
比如：OPPO R11的iperf吞吐量在13Mbps左右，而Xiaomi 5X的吞吐量在25Mbps左右。

如下图所示，设备和Xiaomi 5X交互时，TCP数据包载荷都是1460字节：
![小米5x抓包](/2018-11-29-wifi_package_capture/xiaomi_5x_1420.png?raw=true)

而设备和OPPO R11交互时，则是一个1420字节的大包和一个40字节的小包交替出现。

![OPPO R11抓包](/2018-11-29-wifi_package_capture/oppo_r11_1460.png?raw=true)


从上面报文中的TCP握手阶段可以看出，**OPPO R11（192.168.42.2）的MSS是1420**。这个字段是最大报文段的意思，如果数据长度超过该字段，需要拆包发送。    

这个值一般取自MTU, adb 进入OPPO手机执行ifconfig发现该值是1460，减去20字节的IP头部，再减去20字节的TCP头部，有效数据长度就只有1420了(也就是MSS)。    

再看下**Xiaomi 5X的MTU, 发现是1500，所以MSS为1460**。

再反查设备端的iperf代码，发现每次调用send函数固定发送1460字节。这样就造成了：**同一个数据包，和Xiaomi 5X交互的时候一次就发送完了，和OPPO R11交互的时候，被设备端拆成了1420和40两个包分开发送。**    
这就解释通了，为什么Xiaomi 5X的吞吐量将近是OPPO R11的两倍。    

*注：正常情况下，如果没有开启TCP_NODELAY选项，Nagle算法会在一定程度上将小包聚合成大包的。但是该设备使用的协议栈并无此功能。*

### 2. Wi-Fi省电模式 ###
在Wi-Fi稳定性测试过程中，发现速率每过十分钟左右就会下降到5Mbps左右，3s左右才能恢复。    
用Omnipeek打开抓包文件，在流量图中找到一个速率下降的区间：    

![省电模式抓包](/2018-11-29-wifi_package_capture/power_save_flow.png?raw=true)

分析波谷附近报文，发现PC的网卡进入了省电模式。    
禁用PC网卡省电模式后再测试，Wi-Fi速率一直比较平稳。    

![省电模式标志](/2018-11-29-wifi_package_capture/power_save_flag.png?raw=true)

### 3. 其他适用场景 ###


- 定位手机无法连接AP的问题
- 定位手机连接AP慢的问题
- 定位搜索不到AP的问题
- 定位业务交互过程中的问题。    
*注意：如果要分析报文内容，建议将AP设置为开放模式，这样抓到的内容无需解密。如果AP是有密码的，为了解密报文，必须把连接AP的过程抓到才能还原报文。*

​                
