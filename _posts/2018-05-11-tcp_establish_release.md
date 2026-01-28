---
title:  "TCP连接建立、断开过程详解"  
date:   2018-05-11
categories: [网络]
tags: [TCP]  
---

* content
{:toc}

TCP连接建立过程需要经过三次握，断开过程需要经过四次挥手，为什么？  
有没有其他的连接建立、断开方式？


## 一、 TCP连接建立过程  ##

### 1. 三次握手 ###
TCP正常的建立连接过程如下图所示：

![](/assets/images/2018-05-11-tcp_establish_release/three_handshake.png)

1. 客户端发送的TCP报文中标志位SYN置1，初始序号seq=x（随机选择）。Client进入SYN_SENT状态，等待Server确认。
2. 服务器收到数据包后，根据标志位SYN=1知道Client请求建立连接，Server将标志位SYN和ACK都置为1，ack=x+1，随机产生一个初始序号seq=y，并将该数据包发送给Client以确认连接请求，Server进入SYN_RCVD状态。 
3. Client收到确认后，检查ack是否为x+1，ACK是否为1，如果正确则将标志位ACK置为1，ack=y+1，并将该数据包发送给Server。Server检查ack是否为y+1，ACK是否为1，如果正确则连接建立成功，Client和Server进入ESTABLISHED状态，完成三次握手，随后Client与Server之间可以开始传输数据了。

### 2. 同时打开 ###
同时打开连接是指通信的双方在接收到对方的SYN包之前，都进行了主动打开的操作并发出了自己的SYN包。由于一个四元组（源IP、源端口、目的IP、目的端口）标识一个TCP连接，一个TCP连接要同时打开需要通信的双方知晓对方的IP和端口信息才行，这种场景在实际情况中很少发生。同时打开的流程如下图：
![](/assets/images/2018-05-11-tcp_establish_release/open_same_time.png)


1. A的应用程序使用端口7777向B的端口8888发送TCP连接请求
2. B的应用程序使用端口8888向A的端口7777发送TCP连接请求
3. A收到B的ACK（实际上是SYN+ACK）后进入ESTABLISHED状态
4. B收到A的ACK（实际上是SYN+ACK）后也进入ESTABLISHED状态

注意：
- 对于同时打开它仅建立一条TCP连接而不是两条
- 连接建立过程需要四次握手
- 两端的状态变化都是由CLOSED->SYN_SENT->SYN_RCVD->ESTABLISHED
- 双方发出的SYN+ACK报文中，seq均未递增。比如对于A，发送SYN时seq为x，发送SYN+ACK时seq仍为x

###  3. 自连接  ###

执行下面的脚本，过一段时间通过netstat查看，是不是建立了本地连接。

```shell
while true
do
    telnet 127.0.0.1 50000 
done
```

尽管我的机器上并未监听5000端口，但是却建立了一条TCP连接。

```console
Active Internet connections (w/o servers)
Proto Recv-Q Send-Q Local Address           Foreign Address         State      
tcp        0      0 192.168.88.228:445      192.168.88.167:52324    ESTABLISHED
tcp        0    216 192.168.88.228:22       192.168.88.167:58738    ESTABLISHED
tcp        0      0 127.0.0.1:50000         127.0.0.1:50000         ESTABLISHED
```
自连接是**同时打开**的一个特例。一条TCP连接由四元组（源IP、源端口、目的IP、目的端口）来决定，在上面的例子中，源IP、目的IP、目的端口都是确定的，唯一不确定的是源端口。如果系统选择的源端口与目的端口相同,那么Client和Server（实际上不存在Server这个实体）就是相同的TCP实体。

1. Cient向127.0.0.1:50000发送SYN，进入SYN_SENT状态
2. 由于Client已经打开了端口50000，所以不会产生RST报文；相反系统以为50000端口有服务器在监听，就接收了这个SYN报文，并从SYN_SENT状态变为SYN_RCVD状态
3. 由于TCP状态从SYN_SENT状态变为SYN_RCVD状态，需要发送了SYN+ACK报文
4. 参考**同时打开**的状态图，SYN+ACK报文将TCP状态从SYN_RCVD变为ESTABLISHED



## 二、 TCP连接断开过程 ##
###  1. 四次挥手  ###

TCP连接断开过程如下图所示：
![](/assets/images/2018-05-11-tcp_establish_release/four_handshake.png)


1. Client发送一个FIN，用来关闭Client到Server的数据传送，Client进入FIN_WAIT_1状态。
2. Server收到FIN后，发送一个ACK给Client，确认序号为u + 1（与SYN相同，一个FIN占用一个序号），Server进入CLOSE_WAIT状态。
3. Server发送一个FIN，用来关闭Server到Client的数据传送，Server进入LAST_ACK状态。
4. Client收到FIN后，Client进入TIME_WAIT状态（主动关闭方才会进入该状态），接着发送一个ACK给Server，确认序号为w + 1，Server进入CLOSED状态，完成四次挥手。


###  2. 同时关闭连接  ###

同时关闭和前面**同时打开**的四次握手过程基本类似，流程如下：  

![](/assets/images/2018-05-11-tcp_establish_release/close_same_time.png)

注意：
1. 两端的状态变化都是由ESTABLISHED->FIN_WAIT_1->CLOSING->TIME_WAIT->CLOSED
2. 两端都需要经历TIME_WAIT状态



## 三、 常见问题 ##

### 1. 为什么要三次握手建立连接 ###  

TCP连接是可靠的双工通信，在连接建立阶段必须确认双向通信都是OK的。理论上来讲这需要至少四次交互：
1. Client发送SYN
2. Server响应ACK
3. Server发送SYN
4. Client响应ACK（如果没有这一步，Server无法知道Client能否收到自己的消息）

1、2两步让Client知道自己和Server之间的双向通信是OK的，3、4两步让Server知道自己和Client之间的双向通信是OK的。
实际应用中，2、3两步合并了，所以最终就只有三次握手。

三次握手还可以解决**网络中延迟的重复分组问题**。假设TCP连接建立过程只有两次握手：
1. Client发送SYN
2. Server响应ACK

如果出现下面的情况，服务端就会出问题：
1. Client发送SYN
2. Client端超时未收到Server的ACK，重发SYN
3. Server端收到Client重发的SYN，响应ACK
4. Client收到ACK后，和Server正常数据交互，然后关闭连接
5. Client第一次发送的SYN并未丢失，而是由于网络延迟，现在才到达Server端
6. Server发送ACK（Server认为TCP连接已建立）
7. Client收到Server的ACK，由于Client认为自己并未请求连接，所以会忽略该ACK（不同于SYN，ACK报文不需要回复）
8. 这时Server认为连接已经建立，一直等待客户端数据；客户端却根本不知道有这么一条连接

### 2. 为什么要四次挥手断开连接 ###  
TCP连接是全双工的，因此每个方向都必须单独进行关闭:当一方完成它的数据发送任务后就发送一个FIN来终止这个方向的连接，对端收到后回复一个ACK报文，这样双向就需要四次交互。  

Client主动关闭的情况下，Server收到Client的FIN报文时，仅仅表示Client没有数据发送给Server了；但Server可能还有数据要发送给Client，所以Server可能并不会立即关闭SOCKET，而是先回复一个ACK报文，告诉Client**“你发的FIN报文我收到了”**。只有等到Server所有的报文都发送完了，才发送FIN报文。也就是说，被动关闭方的ACK和FIN报文多数情况下都是分开发送的，所以需要四次交互。

### 3. 为什么TIME_WAIT状态需要经过2MSL才能返回到CLOSE状态###  

- 为了保证主动关闭方发送的最后一个ACK报文能够到达被动关闭方。因为这个ACK有可能无法到达对端，这样对端会重发FIN报文，这时候主动关闭方需要重发ACK。
- 保证本连接的所有报文在网络上消失。如果没有这个机制，可能会对新连接产生干扰。举例如下：
	1. A和B正常建立TCP连接，数据传输，然后断开连接。但是由于网络传输原因，A发给B的seq为100的报文滞留在了网络上。
	2. A和B再次建立连接，所用IP和端口与1中相同，二者数据传输过程中，B正好请求A发送seq为100的数据，这时1中滞留的报文到达B，TCP认为该报文合法，就接收了这个报文。
	

## 四、 参考资料 ##

1. [http://sgros.blogspot.hk/2013/08/tcp-client-self-connect.html](http://sgros.blogspot.hk/2013/08/tcp-client-self-connect.html)
2. [https://networkengineering.stackexchange.com/questions/24068/why-do-we-need-a-3-way-handshake-why-not-just-2-way](https://networkengineering.stackexchange.com/questions/24068/why-do-we-need-a-3-way-handshake-why-not-just-2-way)
3. [https://www.quora.com/Network-Protocols-In-TCP-3-way-handshake-why-we-need-the-third-ACK](https://www.quora.com/Network-Protocols-In-TCP-3-way-handshake-why-we-need-the-third-ACK)