---
title:  "分析RTP码流卡顿问题的工具"  
date:   2018-07-11  
categories: [网络]
tags: [RTP]  
---

* content
{:toc}

在基于RTP的实时码流传输过程中，经常会遇到音视频卡顿、花屏的现象。对于这类问题，如何定位？  
下面这个工具可以帮助分析类似问题：  

[https://github.com/sigusr1/rtp_parse_from_pcap](https://github.com/sigusr1/rtp_parse_from_pcap)


## 一、实现思路 ##  
从传输的角度看，造成卡顿、花屏的常见原因如下：
1. 接收端收到的帧不完整（可能是发送方发的就不完整，也可能是传输过程中丢失）
2. 帧和帧之间的传输间隔太久，超过了接收端的缓存时间  

    > 当然也有其他原因导致的，比如码流兼容性问题，或者编码端/解码端处理流程有问题（我们曾经遇到过解码端处理SEI不当导致的花屏问题），不过这些原因导致的问题一般都是必现的，问题会贯穿在整个视频播放过程中。而传输过程导致的问题，则有很大的随机性。
    {: .prompt-info }


定位这类问题，最快捷的方式是通过wireshark或者tcpdump抓包，然后进行分析。这样可以看出到底是发送端的问题还是接收端的问题，缩小排查范围。
由于我实际工作中使用的都是rtp over rtsp（也就是TCP传输方式），下面的讨论仅针对rtp over rtsp进行，该工具也是针对这种场景开发的。


总体思路就是对抓包文件进行回放，回放过程中解析报文，分析RTP信息和帧间隔。
处理过程中需要考虑以下问题：
1. TCP的乱序、重传如何处理？
2. 抓包工具漏抓报文怎么办？（数据量较大时，很常见的一种现象）
3. 预览过程中抓的包怎么处理？这种报文不仅没有rtsp交互，更没有TCP三次握手过程，也就是说如何跟踪这条会话。
4. 必须保留每个报文的时间戳，这样才能分析传输过程中的耗时情况。


基于以上思路，可以用下面的数据处理流程来实现：
![数据处理流程图](/assets/images/2018-07-11-rtp_parse_tool/work_flow.JPG)

1. libpcap可以对抓包文件进行回放，从抓包文件中逐条提取报文并保留报文的时间戳信息。问题4得以解决。
2. libpcap的输出直接输入到libnids中，对TCP流进行分析处理，解决问题1、2、3。
3. libnids的输出就是原始的TCP字节流了，我们可以直接对其进行RTP解析。



## 二、使用方法 ##  

1. 进入rtp_parser/bin目录
2. 执行./rtp_parser  rtsp.pcap
   其中rtsp.pcap为抓包文件名。
3. 命令执行完毕，即可在当前目录生成形如src[192.168.43.252[554]]--dst[192.168.43.1[39535]].txt的解析文件。
   该文件是以`src[源IP[源端口]]--dst[目的IP[目的端口]].txt`的形式命名的。
   如果抓包文件中包含多条流，每条流都会生成一个独立的解析文件。
4. 文件内容如下所示：
   其中 Frm_Interval代表相邻帧的时间间隔，取值为：  
    `本帧帧尾时间 减去 上一帧帧尾时间`。  
   下图最后一行的Frm_Interval计算过程为：`1514774319.466358s - 1514774318.891198s =  575160us`


	![示例文件](/assets/images/2018-07-11-rtp_parse_tool/parse_file.JPG)

5. rtp_parser/bin目录下的analyse.py脚本可以对解析出来的txt文件进行分析  

	a. 它会以图表的形式展示传输过程中的抖动情况。  
	b. 如果帧间隔过大（超过100ms），命令行会有相应提示  
    c. 如果RTP序号不连续，命令行也会有相应提示  


   执行 `pyhton analyse.py src[192.168.43.252[554]]--dst[192.168.43.1[39535]].txt`可得到下图所示分析结果。图中横坐标代表帧，纵坐标代表帧间隔，单位是us。如下图所示，有一个帧间隔达到了500多ms，肯定会导致卡顿现象。  
   ![解析结果](/assets/images/2018-07-11-rtp_parse_tool/rtp_parse_result.png)

   同时命令行会有如下输出，提示帧间隔过大。最后一行对应的就是图中的波峰：  

   ![警告](/assets/images/2018-07-11-rtp_parse_tool/warnning.JPG)

   从上面的txt解析文件中可以看出，问题应该出在RTP序号18492 ~ 18500之间。  
   分析抓包文件，可以看到RTP序号18492和18493之间有个500多ms的间隔（*18492和18491在同一个TCP报文中，wireshark并未显示出18492*），而这期间接收端的窗口都是OK的，也就是说发送端导致了这个间隔。排查发送端代码，果然有个分支会sleep 500ms，某些情况会走到该分支。  

   ![抓包截图](/assets/images/2018-07-11-rtp_parse_tool/net_delay.JPG)



## 三、编译方法 ##  

该工具依赖开源库libpcap（本工程中的源码版本为1.8.1，未做任何修改）和libnids（本工程中的源码基于1.24做了相关修改）。
正常情况下，如无特殊需要，不需执行第1步和第2步。

1. 进入目录libpcap, 编译生成静态库libpcap.a，拷到rtp_parser/lib目录：  


	a. 执行./configure， 如果依赖库不存在，请根据提示下载安装对应的依赖组件  
	b. 执行make，生成libpcap.a  
	c. 将libpcap.a拷到rtp_parser/lib目录。  

2. 进入文件夹libnids，编译libnids，该库经过相应的修改，可通过git日志查看相关修改内容：  

	a. 执行下面命令生成makefile:  
		`./configure --with-libpcap=../libpcap --enable-tcpreasm  --disable-libglib --disable-libnet`  

    > 其中--enable-tcpreasm选项是允许跟踪不完整的tcp连接，使能了这个选项，即使抓包文件中没有tcp连接的三次握手过程，也能跟踪这条tcp数据流。
    {: .prompt-info }


	b. 执行make，生成libnids.a.  
	c. 将libnids.a拷到rtp_parser/lib目录。  
	
3. 进入文件夹rtp_parser，编译可执行文件rtp_parser：  

	a. 执行make即可在bin目录下生成可执行文件rtp_parser。  
	b. 在bin目录下执行`./rtp_parser rtsp.pcap`(rtsp.pcap为抓包文件)即可生成解析文件  

目前rtp_parser的实现比较简单，可根据需要自行修改，然后执行上面第3步的编译即可。

