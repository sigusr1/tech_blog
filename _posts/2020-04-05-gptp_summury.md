---
title: "AVB简介--第二篇：gPTP简介"
date: 2020-04-05 
categories: [车载以太网]
tags: [AVB, gPTP]
---

* content
{:toc}  
本文是AVB系列文章的第二篇，主要介绍AVB协议族中的精确时钟同步协议gPTP(IEEE Std 802.1AS-2011)。 




## 一、时间同步要解决的问题 ##

不知道大家还记得军训练习齐步走的场景吗？  

![齐步走](/2020-04-05-gptp_summury/walk_sync.jpg?raw=true)

齐步走的动作要领你还记得吗？  
- 教官首先发出“齐步-----走”的命令，大家听到“齐步”二字后，开始调整动作，最终所有人实现动作同步。
- 听到“走”字后，所有人开始以同样的步伐（频率、步长）行走。
- 如果这种状态得以保持，后续任何时刻，大家的动作都是同步的。然而，由于各种原因，随着时间的推移，部分同学的动作会和其他人失调，为了解决这个问题，行进过程中，教官还会喊“幺二幺、幺二幺”的口号，这是为了让动作失调的人在行进过程中动态调整。


时钟同步的原理是类似的：
- 教官类似于网络中的**主时钟**，教官发出的的“齐步走”命令就是**校时信号**。
- 各个学员类似于网络中的**从时钟**，收到校时指令后，调整自己的时钟。
- 由于时钟晶振频率受环境因素（比如温度等）影响会发生变化，随着时间的流逝，各个时钟会慢慢变得不同步。为了解决这个问题，**主时钟必须周期性的发出校时信号**（类似于教官的“幺二幺、幺二幺”口号），供失调的节点动态调整时钟。


由此可见，如果要整个网络中的节点保持时钟同步，该网络必须解决以下问题：  

1. 选取一个主时钟  
2. 主时钟动态的发出同步信号
3. 其他时钟根据同步信号同步自己的本地时钟。本地时钟的同步包含下面两个方面（通俗点讲就是，找到同步点，然后以同样的频率运行）：

   - **绝对时间同步**：如下图所示，它要求在同一时刻，A和B的显示时间一致，又称为**相位同步**。
   ![绝对时间同步](/2020-04-05-gptp_summury/phase_synchronization.jpg?raw=true)

   - **相对时间同步**：如下图所示，虽然在同一时刻A和B的绝对时间不同，但是相邻采样点之间的差值是相同的。也就是说，**A和B对时间的度量是一致的**（比如两个采样点之间的间隔A时钟测量出来是1ms，B时钟测量出来也是1ms）。它要求A和B的频率保持一致，又称为**频率同步**。  
   ![相对时间同步](/2020-04-05-gptp_summury/frequency_synchronization.jpg?raw=true)
  

gPTP就是为了解决以上问题而诞生的。和其他校时协议不同的是，通过约束网络内的节点，它可以达到ns级的精度（6跳以内任意节点间最大时钟误差不超过1us），因此在车载、工业控制等对实时性要求较高的领域得到了应用。

## 二、gPTP的主要思想 ##

先从下图直观感受下gPTP的校时机制，后面会逐步详细介绍：  

![gPTP校时示意图](/2020-04-05-gptp_summury/ptp_sync.jpg?raw=true)

### 1. 体系结构 ###

AVB域内的每一个节点都是一个时钟，由以下两个角色组成：  

- 一个**主时钟**（Grandmaster Clock），它是标准时间的来源；
- 其他的都是**从时钟**（Slave Clock），它们必须把自己的时间和主时钟调整一致。  

下图是一个简单的gPTP网络拓扑图：  


![gPTP体系结构](/2020-04-05-gptp_summury/gptp_topology.jpg?raw=true) 

它包含两种类型的节点：
- **Time-aware end station**：这类设备可以是系统内的主时钟（时间源，Grandmaster），也可以是从时钟（被校时的设备）。图中标注了802.1AS endpoint的就是这种设备。
- **Time-aware Bridge**：它可以是主时钟，也可以仅仅是个中转设备（类似传统的交换机），连接网络内的其他设备。作为中转设备，它需要接收主时钟的时间信息并将该信息转发出去（在转发的时候，需要矫正链路传输时延和驻留时间）。图中标注了802.1AS bridge的就是这种设备。
  
从上图还可以看到，时间信息是从主时钟出发，经由各个Bridge分发到所有的从节点。  


### 2. 主时钟选取 ###

gPTP中的主时钟，既可以默认指定，也可以通过BMCA(Best Master Clock Algorithm) 动态选取。  
不过在车载或其他一些网络拓扑固定的应用场景中，一般不允许使用BMCA动态选取主时钟，而是默认指定。  
这部分内容本文不做相关介绍，有需要可以查阅规范文档。


### 3. 绝对时间同步 ###

下图包含一个主时钟（Master time）和一个从时钟(Slave time)，二者时间不同步。现在要把从时钟的时间校准到主时钟的时间，其中t1、t4为主时钟对应的时间，t2、t3为从时钟对应的时间。  

![通用校时流程](/2020-04-05-gptp_summury/general_sync_time_method.png?raw=true)

主要流程如下：  
1. 主时钟在t1时刻发送Sync命令，从时钟在t2时刻收到同步指令。这时候从时钟并不知道主时钟是在什么时候发出这个Sync命令的，但是知道自己是在t2时刻收到该命令的。
2. 主时钟发送一个Follow_Up命令，该命令中携带t1的值。从时钟收到后，知道上面的Sync指令是在t1时刻发出的。此时从时钟拥有t1、t2两个值。
3. 从时钟在t3时刻发送一个Delay_Req命令，主时钟在t4时刻收到该命令。此时从时钟知道t1、t2、t3三个值。
4. 主时钟接着发送一个Delay_Resp响应从时钟的Delay_Req，该命令中携带t4的值。从时钟收到后，知道主时钟是在t4时刻收到的Delay_Req命令的。此时从时钟知道t1、t2、t3、t4四个值。
5. 我们**假设路径传输延时是对称**的，即去程的传输延时和回程的传输延时相等。从时钟可以根据下面的公式计算路径传输延时(path_delay)，以及自己与主时钟的偏差(clock_offset)：
```
	t2 – t1 = path_delay + clock_offset  
	t4 – t3 = path_delay - clock_offset
```
  由此可以算出：
```
	path_delay = (t4 – t3 + t2 – t1) / 2 
	clock_offset =  (t3 – t4 + t2 – t1) / 2
```
6. 现在从时钟知道了自己与主时钟的时差clock_offset，就可以调整自己的时间了。另外，从时钟还知道自己与主时钟的路径传输延时path_delay，该值对于switch意义重大，因为在gPTP的P2P校时方式中，switch需要转发主时钟的校时信号，在转发的时候，需要将该值放在补偿信息中（后面章节会详细介绍）。


从上面的流程可以看到，传输延时path_delay的精度/稳定性会影响校时精度。而传输延时又可以划分为：各段链路传输时间总和 + 中间节点转发导致的驻留时间（缓存时间）。  

### 4. 相对时间同步 ###

我们首先看下时间度量的原理：时间是基于晶振的震荡周期进行度量的，如果一个晶振的震荡频率是10Hz，也就是说每100ms震荡一次，震荡10次代表过了1秒，震荡600次代表经过了1分钟。

但是晶振并非绝对稳定的，受温度等因素影响，震荡频率可能发生变化，震荡周期也就变得不准了。而晶振的使用者（时钟）并不知道这些变化，只是傻傻的计数，震荡10次为1s，震荡600次为1分钟。如果现实世界的1s内该晶振少震荡了一次（某两次震荡之间的间隔变成了200ms），那么该时钟的1s就对应现实世界中的1.1s；如果某两次震荡的时间间隔变成了50ms（多震荡了一次），那么该时钟的1s就对应现实世界中的0.95s。

相对时间同步，要求从时钟的频率和主时钟一致。我们可以通过下面的方式估算晶振的变化，并动态调整。
如下图所示，分别在t1、tn时刻对主时钟和从时钟进行采样，采样值分别记为t1_master、tn_master、t1_slave、tn_slave，可以得到下面的比例：

	ratio = (tn_slave – t1_slave) / (tn_master – t1_master)

理想情况下，ratio的值应该是1，如果大于1，说明从时钟走的快了，如果小于1，说明走的慢了。从时钟可以根据该值调整自己的频率。  

![谐振](/2020-04-05-gptp_summury/Synchronization.jpg?raw=true)


## 三、影响校时精度的因素 ##

其实不同的校时协议，原理都大同小异。为什么gPTP可以达到ns级别的精度？  
我们不妨先看下影响校时精度的因素以及gPTP的对策。


### 1. 传输时延不对称 ###

前面提到的校时流程中，我们假设传输时延是对称的，即报文从A传到B和从B传到A耗时相同，实际情况中，路径有可能是不对称的，如下图所示，t<sub>ms</sub>和t<sub>sm</sub>有可能是不相等的。这会导致校时误差。

![传输时延不对称](/2020-04-05-gptp_summury/path_delay_async.png?raw=true)

gPTP对策：
- 要求网络内的节点都是时间敏感的
- 传输延时分段测量（P2P方式）减少平均误差
- 中间转发节点可以计算报文的驻留时间，保证校时信号传输时间的准确性
- 如果已知链路不对称，可以将该值写在配置文件中，对于endpoint，在校时的时候会把该偏差考虑进去；对于bridge设备，在转发的时候，会在PTP报文的矫正域中（correctionField）把对应的差值补偿过来。
 
### 2. 驻留时间 ###

对于Bridge设备，从接收报文到转发报文所消耗的时间（中间可能经过缓存），称为**驻留时间**。该值会具有一定的随机性，从而影响校时精度。

gPTP对策：Bridge设备必须具有测量驻留时间的能力，在转发报文的时候，需要将驻留时间累加在PTP报文的矫正域中（correctionField）。

### 3. 时间戳采样点 ###

前面提到的t1、t2、t3、t4等采样时刻的值，应该在哪里产生呢？  

常规的做法是在应用层采样，如下图蓝色传输线路所示：在发送端，报文在应用层（PTP校时应用）产生后，需要经过协议栈缓冲，然后才发送到网络上；在接收端，报文要经过协议栈缓冲，才能到达接收者（PTP校时应用）。这样存在下面两个问题，而这都会影响时间同步的精度：

- 协议栈缓冲带来的延时是不固定的
- 操作系统调度导致的随机延时

![传输时延不对称](/2020-04-05-gptp_summury/hardware_timestamp.png?raw=true)



为了达到高精度的时间同步，必须消除软件带来的不确定因素，这就要求必须把时间采集点放在最靠近传输介质的地方。  

gPTP对策：  
- 从上图可以看到，比较合适的采集点就是MAC层：在发送方，当报文离开MAC层进入PHY层的时候记录当前时刻；在接收方，当报文离开PHY层刚到达MAC层的时候记录当前时刻。这样可以消除协议栈带来的不确定性。 
- MAC时间戳可以通过软件的方式打，也可以通过硬件的方式打，硬件方式会更精确（可以消除系统调度带来的不确定性）。gPTP中要求使用硬件方式，也就是常说的硬件时间戳。


### 4. 时钟频率 ###

晶振频率越高，误差越小，校时越精确。  
gPTP要求晶振频率不小于25MHz，误差不大于100PPM(每天8.64s误差）。  
gPTP的要求并不算高，主要是考虑到成本因素，要求太高不利于推广。

### 5. 传输路径延时测量方式 ###
IEEE 1588支持两种路径延时测量方式：End-to-End（E2E) 和 Peer-to-Peer（P2P)，二者不能在同一个网络中共存。  

在End-to-End机制中，强调的是两个支持PTP的端点（一个master port，一个slave port）之间的延时，这两个端点可能是直接相连的，也可能中间穿插了普通的交换机、时间敏感的透明时钟（TC），在通信双方看来，信息都是在master port 和slave port之间传输，所以最终slave测量到的传输延时是从master到slave的端到端延时。

在Peer-to-Peer机制中，要求网络内所有节点必须支持P2P，所以它强调的是相邻相邻节点间的通信，最终测量的是相邻节点间的传输延时。  

二者主要区别如下图所示：
- P2P测量的是相邻节点间的延时，路径测量报文不会跨节点传输，有利于网络扩展；E2E测量的是master port和slave port之间的，中间节点（如TC、普通switch）需要转发延时测量报文，网络规模较大时，报文可能泛滥，master节点压力较大。
- master节点变更时，E2E需要重新测量到新master节点的路径延时，P2P只需关心相邻节点。
- E2E方式允许网络中有普通的switch（透传PTP报文即可，由于驻留时间随机，会影响测量精度），而P2P要求网络中的switch必须全部支持P2P。
- E2E机制中，校时报文和路径测量报文是耦合在一起的（第二章第3部分描述的就是典型的End-to-End的流程，它使用Sync、Follow_Up、Delay_Req、Delay_Resp四个消息，同时计算时钟偏差和路径测量）；P2P机制中有独立的报文负责路径测量，把校时和路径测量解耦了。
![e2e_vs_p2p](/2020-04-05-gptp_summury/e2e_vs_p2p.jpg?raw=true)

gPTP要求使用P2P方式，并且要求网络中所有设备都支持PTP协议，路径传输延时测量只在相邻节点间进行。它使用Pdelay_Req、Pdelay_Resp、Pdelay_Resp_Follow_Up消息来测量路径传输延时。 

*注意Peer-to-Peer中没有使用Sync报文，而是专门为路径测量新建了几个报文，降低了复杂度。*  

![p2p延时测量](/2020-04-05-gptp_summury/p2p.png?raw=true)


具体流程如下：  

1. 节点A在t1时刻发送路径测量请求命令Pdelay_Req，并记录下时刻t1
2. 节点B在t2时刻收到Pdelay_Req
3. 节点B将t2放在报文Pdelay_Resp中，并在t3时刻将该报文发给节点A
4. 节点B将t3放在报文Pdelay_Resp_Follow_Up中发给节点A
5. 节点A在t4时刻收到Pdelay_Resp_Follow_Up。至此，节点A拥有t1、t2、t3、t4四个参数，平均路径传输延时可以通过下面的公式计算出来：  
```
	path_delay = (t4 – t3 + t2 – t1) / 2 
```  

在Peer-to-Peer机制中，不仅节点A会主动发起测量请求，节点B也会主动发起测量请求，也就是说，每个节点都知道和自己紧挨着的节点的传输延时（Peer-to-Peer的名字也是这样来的）。不过有的场景下（比如固定主时钟的情况），可能会禁止master port进行路径测量。

### 6. 时钟类型 ###

PTP时钟可以分为两类：One-Step Clock和Two-Step Clock。  

还记得下图Follow_Up消息的作用吗？ 它只是为了把t1的值传给slave节点。这种时钟就是Two-Step Clock， 它的事件报文（Sync等）中不携带时间信息，需要用另外一条普通报文传输时间信息（用来描述上一条事件报文是在什么时候发送的）。  

![two-step-clock](/2020-04-05-gptp_summury/general_sync_time_method.png?raw=true)  

如果t1能在Sync报文本身中传递给slave节点，就节省了一条报文，如下图所示，这是One-step clock的做法。这种时钟对硬件要求比Two-step clocks高，成本也比较高。

![one-step-clock](/2020-04-05-gptp_summury/one-step-clock.png?raw=true)


理论上来讲，同一个网络内可以存在两种类型的时钟，并且时钟类型不会影响校时精度。  
gPTP要求使用Two-step时钟，因为这种机制对硬件要求较低，方便后续扩展，以及在现有的网络中普及。

## 四、gPTP校时过程 ##

### 1. 绝对时钟同步 ###

以下图为例介绍gPTP时间同步过程，为了表述方便，这里做两点假设：
- 假设下面的三个设备都是One-Step的Clock，即Sync报文发出后，不需要额外的Follow_Up报文告知Sync报文是在哪个时刻发送的。（实际802.1AS要求时钟必须是Two-Step的）
- 假设各设备已通过前面介绍的Peer-to-Peer机制测量出路径传输延时path_delay1、path_delay2

![gptp_work_flow](/2020-04-05-gptp_summury/gptp_work_flow.jpg?raw=true)

校时流程如下：
1. Grandmaster时钟在t1时刻发送时间同步报文Sync到Bridge，报文Sync的originTimestamp中填充时间信息t1，矫正域correction填充ns的小数部分（Sync报文的时间戳部分只能表示秒和纳秒，不足1纳秒的只能放在矫正域）。
2. Bridge收到Sync报文后，不仅要矫正自己的时钟，还要把Sync报文转发出去
3. Bridge根据Sync报文调整自己的时钟：  
Bridge在t2时刻收到Sync报文，并从中解析出Grandmaster是在t1时刻发送该报文的，以及Grandmaster填充的矫正值correction。在t2时刻，Grandmaster的时钟显示的值应该是：
```	
	t1 + correction + path_delay1
```
由此可以计算出Bridge的时钟偏差，并调整自己的时钟：
```
	clock_offset = t1 + correction + path_delay1 – t2
```
4. Bridge转发Sync报文：  
如下图所示，收到Sync报文后，Bridge将自己与上级节点的路径延时（path_delay1）和Sync报文在自己这里的驻留时间（rEsidence_time）累加到Sync报文的矫正域，并转发出去。此时矫正域correction值如下：
```
	correction = old_value_of_ correction + path_delay1 + residence_time 
```
**注意：Bridge不修改Sync报文的originTimestamp字段（该字段为Grandmaster发出Sync报文的时间）。**
![Bridge work flow](/2020-04-05-gptp_summury/gptp_sync_in_switch.png?raw=true)

5. End-Point在t4时刻收到Sync报文，并从中解析出Grandmaster是在t1时刻发送该报文的，以及Bridge矫正后的correction。在t4时刻，Grandmaster的时钟显示的值应该是：  
```
	t1 + correction + path_delay2
```
由此可以计算出End-Point和Grandmaster的时钟偏差，并调整自己的时钟：
```
	clock_offset = t1 + correction + path_delay2 – t4 
```

由上面的校时流程可以看出，整个校时过程像水面的波纹一样从Grandmaster开始向外一层层的扩散，每个节点只关注自己和上级节点的传输延时，Bridge负责将中间路径的传输延时和缓存时间逐段累加到矫正域。

### 2. 相对时钟同步 ###

如下图所示，主时钟在时刻master_t1发出校时信号Sync_1，从时钟接收到该信号的时候，记录两个值：

- **slave_t1**：接收到Sync_1信号时，slave本地时钟的值，这个值是当前时刻在slave时间坐标系下的采样
- **slave_t1<sup>'</sup>**：接收到Sync_1信号时master时间坐标系的值，该值可以用下面的公式算出：
```
	master_t1 + 传输时延 + 矫正域
```
  其中，master_t1和矫正域的值在Sync报文中携带，传输延时可以通过前面的方法测量。

![相对时钟同步](/2020-04-05-gptp_summury/Synchronization_frequency.jpg?raw=true)

根据前面介绍的相对时钟同步原理，可以通过下面的公式判断自己的频率和主时钟是否保持一致：  

	ratio = (slave_tn – slave_t1) / (slave_tn’ – slave_t1’)

理想情况下，ratio的值应该是1，如果大于1，说明从时钟走的快了，如果小于1，说明走的慢了。从时钟可以根据该值调整自己的频率。

## 五、参考资料 ##

1. [A_Simulation_Model_of_IEEE_802.1AS_gPTP_for_Clock_Synchronization_in_OMNeT.pdf](https://easychair.org/publications/open/Q4kL)
2. [时钟精度](https://www.zhihu.com/question/38772405)
2. [PTP_Basics.pdf](https://www.nettimelogic.com/resources/PTP%20Basics.pdf)