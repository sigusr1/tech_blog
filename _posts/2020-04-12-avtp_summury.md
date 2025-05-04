---
title:  "AVB简介--第三篇：AVTP简介"  
date:   2020-04-12  
categories: [车载以太网]
tags: [AVB, AVTP, 1722-2016]
---

* content
{:toc}  
本文是AVB系列文章的第三篇，主要介绍AVB协议族中的音视频传输协议AVTP(IEEE Std 1722-2016)。







AVTP是个链路层传输协议，其主要作用有两个：

1. **音视频数据封装**：将音视频数据封装成相应的格式在**链路层**传输。
2. **媒体同步**：
	- 媒体时钟同步：不同的媒体类型有自己的媒体时钟，这些媒体时钟都映射到gPTP时间（同一个时间坐标系），接收端可以轻松进行媒体时钟恢复。
	- 展示时间同步(播放时间同步)：数据发送时指示接收方在未来的某个时间点播放，如果有多个接收者，它们就会在未来的同一时刻同时播放。


## 一、音视频数据封装 ##

AVTP是链路层的传输协议，并且是基于VLAN的，在以太网帧中的位置如下所示：    

![AVTP报文格式](/2020-04-12-avtp_summury/avtp_format.jpg?raw=true)

针对不同的音视频格式，AVTP有不同的Header和Payload格式。（*注：AVTP的Header其实是分了几个层级的，包含通用部分和随音视频格式变化部分，这里不再详细介绍。*）  

本文主要基于H264介绍AVTP。  


### 1. 头部结构 ###
下图是AVTP封装H264视频数据时的头部结构：  

![H264_AVTP_Header](/2020-04-12-avtp_summury/avtp_h264_header.jpg?raw=true)

我们结合实际报文重点关注图中编号了的几个字段，上图编号和下图抓包中的编号一一对应：
1. subtype：AVTP子类型，本例为压缩视频格式，一般简称为CVF
2. tv:它用来指示字段5是否有效， 0代表无效，1代表有效；这是因为一个视频单元（NALU）会被拆分为多个AVTP包，规范要求只需要在最后一个AVTP包中添加时间戳即可。
3. sequence_num：包序号，供接收端判断是否丢包、乱序
4. stream_id：流id，用来标识本数据流。长度为64bit, 前48字节定义和MAC地址定义规则一致，大部分直接拿MAC地址作为前48bit，后16bit根据需要自定义分配。
5. avtp_timestamp: AVTP Presentation Time，后面专门介绍
6. format: 用来表明payload承载的音视频数据是自定义格式还是RFC规范定义的格式，本例中是RFC格式的视频。
7. format_subtype: payload承载的音视频数据子类型，本例中是H264格式。
8. M标志位：代表一个NALU的结束。如果一个NALU被拆分为多个AVTP报文，只有最后一个需要把M标志填写成1。
9. h264_timestamp: h264时间戳，后面专门介绍。
10. ptv：用来指示h264_timestamp字段是否有效。本例中未填写h264_timestamp，所以ptv均为0（抓包中未标记）。

![H264_AVTP_Header_wireshark](/2020-04-12-avtp_summury/avtp_h264_header_wireshark.jpg?raw=true)


### 2. payload结构 ###  

为了便于理解后续部分，我们首先简单介绍下H264和RTP相关知识。

#### 2.1 H264基础知识 ####
H264帧由多个NALU单元组成，如下图所示，其中Start Code就是0x000001或0x00000001，NALU Header中包含该NALU的类型。

![H264帧结构](/2020-04-12-avtp_summury/frame_struct.jpg?raw=true)  

H264帧分为I帧、P帧、B帧三类，其中：  

- I帧不存在帧间依赖，可以单独解码成像；
- P帧依赖本帧前面的I帧或P帧（这种依赖是从I帧依次传递过来的，所以中间任何一帧出错都会导致后续帧出错）；
- B帧不仅依赖前面的帧，还依赖后面的帧

如果一个码流中只有I帧和P帧，这种码流属于**非交叉编码模式**（Non-interleaved mode），帧的解码顺序和显示顺序是一致的；如果码流中包含了B帧，就成为了**交叉编码模式**（Interleaved mode），帧的解码顺序和显示顺序就不一定是一致的了。  

下图中红色为I帧，蓝色为P帧，绿色为B帧。可以看到，第一个B帧在码流中的位置是2（Number in Stream order， 即解码顺序，从0开始），而显示顺序是1（Number in Display order，即显示顺序)。也就是说，它前面的P帧先解码，但要在它之后显示。  

B帧使得解码顺序和显示顺序不再一致。记住这一点对后面理解AVTP中的两个时间戳有帮助。


![B帧解码显示示意图](/2020-04-12-avtp_summury/b_frame.jpg?raw=true)


#### 2.2 RTP基础知识 ####  

RTP封装H264数据是以NALU为单位进行的，而不是以帧为单位进行的，相应规范是RFC 6184规范（RTP Payload Format for H.264 Video）。  

RTP打包模式有下面三种：  

- Single NAL unit mode：单NALU模式，适用 H.241。
- Non-interleaved mode：非交叉模式，NALU的解码顺序和显示顺序是一致的，先解码的NALU先显示。
- Interleaved mode： 交叉模式，本模式下NALU的解码顺序和显示顺序是不一致的，比如有B帧的情况下。

RTP打包使用哪种模式，是由编码器决定的，不能随便填。

RTP包类型又包含以下几种：
- a. 单个NALU：一个数据报文包含一个完整NALU的
- b. 聚合多个NALU：一个数据报文中包含多个NALU，根据这些NALU的时间戳是否相同，又分为下面两种
	- STAP：一个数据报文包含多个NALU，这些NALU时间戳相同，又分为STAP-A方式和STAP-B方式
	- MTAP：一个数据报文包含多个NALU，这些NALU时间戳不同，又分为MTAP16方式和MTAP24方式
- c. 分片方式：NALU太大，无法用一个数据包传输，需要分片，又分为FU-A和FU-B方式

打包模式与包类型之间的关系如下，并不能随便使用：  


![每种打包模式使用的rtp包类型](/2020-04-12-avtp_summury/rtp_type.jpg?raw=true)


我们的视频数据是Non-interleaved mode模式，所以理论上可以使用上图中的NAL unit、STAP-A和FU-A三种包类型，但通常情况下不会把多个NALU聚合在一起发送（增加复杂度），所以实际只使用了NAL unit和FU-A两种包类型，前者用来封装较小不需要分片的NALU，后者用来封装需要分片的NALU。

#### 2.3 AVTP封装h264_payload ####

AVTP的h264_payload是遵循RFC 6184规范（RTP Payload Format for H.264 Video）的。  
前面提到，我们只使用了NAL unit和FU-A两种包类型，前者用来封装较小不需要分片的NALU（下图左半部分），后者用来封装需要分片的NALU（下图右半部分）。

![AVTP封装](/2020-04-12-avtp_summury/avtp_pack.jpg?raw=true)

## 二、媒体同步 ##


### 3.1 AVTP Presentation Time ###

AVTP Presentation Time的含义是呈现时间，表示接收方在该时刻需要将AVTP数据包payload中的音视频数据送到应用层进行处理，比如解码播放。  

假设报文经过下图发送参考平面（Ingress Time Reference Plan）的时刻是t1（基于gPTP时间），那么Presentation Time的值就是`t1 + Max Transit Time`。 假设该Presentation Time用gPTP表示为`AS_sec(秒) + AS_ns(纳秒)`， 实际打在AVTP头部的时间戳为：(AS_sec × 10<sup>9</sup> + AS_ns) mod 2<sup>32</sup>。  

*注：这个时间戳为什么要对gPTP时间做取模处理，规范中并未说明，猜测应该是为了节省字节。因为表示完整的gPTP时间需要占用10个字节，其中6字节用来表示秒，4字节用来表示纳秒，而现在只需要4字节即可。当然，该时间戳4秒就轮回了。*

![AVTP Presentation Time定义](/2020-04-12-avtp_summury/avtp_present_time.png?raw=true)


那么，Max Transit Time是如何定义的呢？如下图所示，如果音频源到两个扬声器的传输时间分别是t1、t2，Max Transit Time就是二者中的最大值。  

![Max Transit Time示意图](/2020-03-22-AVB_summury/avb_in_house.png?raw=true)  

Max Transit Time的通用定义如下，其中tn为Talker到第n个Listener的最大传输时间。

	Max Transit Time = MAX(t1, t2, …, tn)

接下来以H264为例讲解AVTP的媒体同步机制，下图是H264 Over AVTP典型的处理流程：  

![avtp_timestamp示意图](/2020-04-12-avtp_summury/avtp_timestamp.jpg?raw=true)

### 3.2 展示时间同步（播放时间同步） ###
结合`AVTP Presentation Time`和`Max Transit Time`的定义，可以看到：它可以指示接收端在未来的某一时刻处理音视频数据；数据可以提前到（提前到的要等待，直到时刻AVTP Presentation Time到来才能被处理），但绝不能迟到（你说你在时间点AVTP Presentation Time到达，结果迟到了，只有被丢弃）。**就像是一次准时开始的会议，提前到的要等待会议开始，迟到者无法听到前面的内容**。在这种机制保障下，考虑下面的两个场景，是不是都可以达到同步效果？

![同步播放示意图](/2020-04-12-avtp_summury/Sync_by_avtp_timestamp.jpg?raw=true)  

### 3.3 媒体时钟同步 ###

媒体时钟同步，解决的是按采集速度和播放速度一致的问题（相对时间同步的问题）。  

视频的媒体时钟一般都是90KHz，理想情况下，大家以同样的频率震荡，但是随着时间的流逝或者环境影响，会漂移，这样就会导致talker和Listener的媒体时钟不同步，进而表现为播放不正常（播放的太快或太慢）。  

媒体时钟恢复，是指Listener根据AVTP Presentation Time重建媒体时钟，使之和采集端保持同步，进而指导音视频**以采集时的速率播放**，流程如下：

1. AVTP假设网络中各个节点的媒体时钟都是自由运行的（也就是相互之间不同步）。为了便于接收端恢复媒体时钟，在发送端，Talker把媒体时钟嵌入在展示时间戳中的（采样点对应gPTP的某个时刻），如下图所示：  
![talker_media_clock](/2020-04-12-avtp_summury/talker_media_clock.png?raw=true)  

2. 在接收端，媒体时钟从展示时间戳中恢复（AVTP Presentation Time和本地gPTP时间对比，二者同步的时刻对应一个Media Clock的采样点），进而控制音视频的播放。
![listener_media_clock](/2020-04-12-avtp_summury/listener_media_clock.png?raw=true)  

3. 媒体时钟恢复模块示意图如下所示：  
![媒体时钟恢复模块示意图](/2020-04-12-avtp_summury/media_clock_recovery.gif?raw=true)

AVTP中也可以定义专门的Media Clock Stream，用来同步相关节点的媒体时钟，这里不再展开介绍。

### 3.4 h264_timestamp ###

AVTP中有了展示时间戳，为什么还要加上h264_timestamp时间戳？  

在交叉编码模式（Interleaved mode）下，解码顺序和显示顺序是不一致的。如下图所示，视频数据是按照Frame0、Frame1的顺序依次采集的，接收端也要按这个顺序显示。  


![采集顺序](/2020-04-12-avtp_summury/pts.jpg?raw=true)  

但是，由于存在B帧，编码器实际的输出顺序如下，接收端也要按照下面的顺序解码：  

![编码输出顺序](/2020-04-12-avtp_summury/dts.jpg?raw=true)



从上面的章节可以了解到，AVTP Presentation Time的作用是**DTS（Decoding Time Stamp）**，在非交叉模式（Non-interleaved mode）下，是可以正常工作的；但是在交叉模式（Interleaved mode）下，由于解码顺序和显示顺序不一致，虽然能按正确的顺序解码，但是不能按正确的顺序显示。  

为了解决这个问题，才加上了h264_timestamp，它也是遵循RFC 6184规范的（其实就是RTP头部的时间戳）。它充当的是**PTS（Presentation Time Stamp）**的角色，用以指示正确的显示顺序。  

在非交叉模式下，该值可填充也可不填充。

## 三、参考资料 ##

1. [H264 over RTP 的打包](https://blog.csdn.net/u010178611/article/details/82592393)
2. [Understanding IEEE’s deterministic AV bridging standards](https://www.embedded.com/understanding-ieees-deterministic-av-bridging-standards/)
3. [参考报文：gstream工具生成的报文](/2020-04-12-avtp_summury/gstream_avtp.pcap)
