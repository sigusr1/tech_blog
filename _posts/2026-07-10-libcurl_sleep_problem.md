---
title: "libcurl blind sleep导致的耗时问题"
date: 2026-07-10
categories: [网络]
tags:  [libcurl]
---


最近在分析链路耗时的时候，发现一个奇怪的问题，同一次http请求，server端统计的耗时和client端统计的耗时差别较大，最大接近1s。本机通信，传输的数据量不大，这个耗时明显不合理。
最终发现，系统中的[libcurl](https://github.com/curl/curl)版本较老（2018年的7.61.0），在特定场景下会进入sleep（最长sleep 1s），导致数据接收不及时。

## 1. 问题分析
从下图trace可以看出，http client侧统计的耗时是8.02s，http server侧统计的耗时是7.51s，二者相差0.51s：
![耗时统计差异](/assets/images/2026-07-10-libcurl_sleep_problem/trace.jpg)

从下图抓包可以看出，在网络层面这个http请求确实7.51s就结束了:
- client在报文264发起http请求
- server在报文642发送了reply
- client在报文643确认收到了报文642（tcp协议层的ack，并不是应用层的）
- 报文264到报文643耗时7.51s，和trace中server侧的统计能对上。也就是说，多出来的0.51s需要从client侧找原因

![抓包](/assets/images/2026-07-10-libcurl_sleep_problem/capture.jpg)

client侧进一步添加trace/日志，最终发现问题出在**libcurl的blind sleep机制**。

client调用链如下：
```c
http_client.cpp
  └─ curl_easy_perform()          ← 应用层调用
       └─ easy_transfer()         ← lib/easy.c
            ├─ curl_multi_wait()  ← lib/multi.c（poll fds，无fd事件）
            └─ Curl_wait_ms()     ← lib/select.c（blind sleep！）
```

`easy_transfer()` 相关代码如下：
- 步骤① `curl_multi_wait()`中通过poll监听fd，如果1s内无事件，返回值mcode为0，出参rc也为0
- 接下来进入步骤② `Curl_wait_ms(sleep_ms)`执行sleep，最多sleep 1s
- 如果网络数据在sleep期间到达，curl是没法及时处理的，只能等睡醒后再次进入curl_multi_wait执行poll才能处理数据

```c
// lib/easy.c
while(!done && !mcode) {
    int still_running = 0;
    int rc;

    mcode = curl_multi_wait(multi, NULL, 0, 1000, &rc);   // ← 步骤①

    if(!mcode) {
      if(!rc) {
        long sleep_ms;
        curl_multi_timeout(multi, &sleep_ms);
        if(sleep_ms) {
          if(sleep_ms > 1000) sleep_ms = 1000;
          Curl_wait_ms((int)sleep_ms);                     // ← 步骤②：blind sleep
        }
      }
      mcode = curl_multi_perform(multi, &still_running);
    }
    ...
}
```

结合前面抓包报文，server在回复`100 Continue`后就没再发数据，直到7.5s后才发送reply，相关时序如下：
- 0~1s执行 curl_multi_wait，1s超时无fd事件
- 1~2s执行 sleep
- 2~3s执行 curl_multi_wait，1s超时无fd事件
- 3~4s执行 sleep
- 4~5s执行 curl_multi_wait，1s超时无fd事件
- 5~6s执行 sleep
- 6~7s执行 curl_multi_wait，1s超时无fd事件
- 7~8s执行 sleep  （**← 7.5s的时候server的reply到达，无法立即处理， 浪费了0.5s**）
- 8~9s执行 curl_multi_wait，fd上有数据，立马返回，进入数据处理流程

从上面的时序可以看出，这个问题不是必现的:
- 如果数据在curl_multi_wait的时候到达，会被立即处理，client和server统计的时长基本一致
- 如果数据在sleep的时候到达，因为要等sleep结束，client和server统计的时长就有差别了，差异在0~1s

## 2. 如何解决

要解决这个问题，必须先了解libcurl为啥引入这个blind sleep机制。  
问题在于函数curl_multi_wait，它检测到没有fd需要监控时会立即返回。这种情况下，easy_transfer中的while如果立即进入下一次循环，会出现类似while死循环的情况，导致cpu飙升到100%。
为了解决这个问题，libcurl引入了这个sleep机制。

> 什么情况下会没有fd需要监控？libcurl中提到有多种场景，其中一个是速率限制，比如某个套接字速率太快超过阈值了，libcurl会把对应的fd从poll的监听列表中拿下来，暂不处理这个套接字的数据，降低速率。

```c
CURL_EXTERN CURLMcode curl_multi_wait(CURLM *multi_handle,
                                      struct curl_waitfd extra_fds[],
                                      unsigned int extra_nfds,
                                      int timeout_ms,
                                      int *ret);
```
`curl_multi_wait`的出参`ret`代表**事件就绪 fd 的数量**，但`ret`为0时有歧义:
- 情况 A：没有需要监控的fd，curl_multi_wait立即返回
- 情况 B：有需要监控的fd，但1000ms内无事件，poll超时返回


对于情况A，进入sleep是合理的；对于情况B，就不应该进入sleep了，应该继续交给poll去监听fd事件。  
但现在无论是情况A还是情况B都会进入sleep流程。我遇到的问题就是在情况B进入了sleep流程。

这个问题的解法也比较明确，让情况A和B分别进入不同的后续流程就行了，具体可参考[官方修复patch](https://github.com/curl/curl/commit/02346abc32a3995299fa9c2f35b9f0a1d091b045)。


## 3. 题外话

### 3.1 系统中公共库的维护更新机制
- 这不是一个简单的技术问题，只要没遇到问题，研发没动力做，项目更不想让你瞎变更
- 最终的结果就是，大家更倾向于就问题解问题，没人主动同步上游变更，开源社区踩过的坑，一个不拉的再踩一遍
- 如果系统中的公共库因为版本太老实在满足不了需求，那就不用它，自己搞个新版本内置在自己的模块中，避免影响其他模块 🤷

### 3.2 AI相关

在这个问题排查过程中，我和AI是通过下面的方式合作的，基本遵循AI干活我把关的模式：
- 我：问题描述、对应的trace、抓包文件丢给AI分析
- AI：得出结论问题在http客户端
- 我：人工确认AI的结论OK，然后把http客户端、curl源码丢给AI分析
- AI：分析后直接给出结论：sleep机制导致无法及时处理数据
- 我：人工确认AI的结论OK，让它修复
- AI：修复版本1：它从最新版的curl中抠出了一部分代码作为补丁
- 我：人工检查发现也没问题，但是因为我的libcurl实在太老了，所以看起来改动有点大，让他继续调研看有没有适合我的版本的直接可用的patch
- AI：修复版本2：还真从github上找到了可以直接使用的patch
- 我：人工确认OK

我的收获：在这之前我对curl的具体实现并不怎么了解，但经过上述确认过程，和AI讨论了大量的curl实现原理、背景，现在对curl中http相关部分有了一定的了解。