---
title: "HaloOS vbslite初探"
date: 2025-07-29
categories: [操作系统]
tags:  [HaloOS, vbslite]
---

本文主要介绍HaloOS通信中间件[vbslite](https://gitee.com/haloos/vbs/blob/master/developer-guide/quick_start.md)，代码版本`tag_V1.0.0_20250721`，运行在Ubuntu 20.04上。

* content
{:toc}

## 1. 代码规范

如果要打造一个良性的开源社区，方便大家协作，规范性是十分必要的，这其中包含：代码风格、bug提交规范，代码入库规范、测试规范、文档规范、评审机制等。

在阅读代码期间发现不少规范性的问题，不知道是vbslite缺少统一的规范，还是有规范但执行不到位，下面举几个例子（部分问题已反馈给作者）：

- `vbslitespace/examples/local_test/app_test.c`中变量未对齐：
![变量未对齐](/assets/images/2025-07-29-first_impressions_of_haloos_vbslite/not_align_main_func.png)

- `vbslitespace/mvbs/posix_aux/src/loop.c`操作符前后是否加空格风格不一致：
![操作符前后是否加空格风格不一致](/assets/images/2025-07-29-first_impressions_of_haloos_vbslite/space_before_and_after_oprator.png)

- `vbslitespace/mvbs/src/adapter/posix/src/adapter_socket.c`中tab和space混用：
![tab和space混用](/assets/images/2025-07-29-first_impressions_of_haloos_vbslite/mix_tab_space.png)

- 下面这个[提交](https://gitee.com/haloos/vbslite_mvbs/commit/3f2328e8833074c909e2bc23b90653d60d0994f4)，提交说明形同虚设，不点进去看代码都不知道改了什么：
![从提交说明看不出改了什么](/assets/images/2025-07-29-first_impressions_of_haloos_vbslite/not_kwnow_what_he_change.jpg)

- 下面这个[提交](https://gitee.com/haloos/vbslite_mvbs/commit/b0f1ce262becb0092f5ff95c6590c93a32528152)，**说是修改文档，结果一个文档没改，改了大量的代码**：
![提交说明严重不符](/assets/images/2025-07-29-first_impressions_of_haloos_vbslite/update_docs_but_change_code.jpg)


## 2. 线程模型

通信中间件的线程模型特别重要，比如api是否线程安全、回调函数从哪个线程执行，掌握了这些信息，有助于减少因为使用不当导致的bug，比如该加锁的地方没加锁。

根据官方文档，基于vbslite开发的程序一般是下面这种结构：
- 创建loop
- 阻塞在loop上循环等待事件
- 处理socket/定时事件

```c
int main(int argc, char* argv[]) {
    // 创建loop
    struct mvbs_event_loop* loop = mvbs_event_loop_create(MVBS_APP_LOOP_PERIOD_MS);

    while (true) {
        // 等待事件
        uint32_t event = mvbs_event_loop_wait(loop);

        if (event & MVBS_EV_TIMER) {
            // 处理定时任务
        }

        if (event & MVBS_EV_RECV) {
            // 处理socket事件
        }
    }
}
```

使用vbsliste框架会额外引入两个线程：
- socket io线程，处理socket收发事件
- 定时器线程：严格来讲算不上定时器，只是定时唤醒用户线程

![线程模型](/assets/images/2025-07-29-first_impressions_of_haloos_vbslite/thread_model.jpg)

函数`mvbs_event_loop_create`会创建上面提到的两个框架线程：
- 定时器线程处理函数`timer_event_handle`是while循环加sleep实现的，每隔一段时间（用户设置的阈值）通过mvbs_event_send唤醒用户线程
- socket线程通过 `socket_recv_loop` --> `adapter_socket_monitor_loop`监听socket事件，底层实现是epoll，epoll唤醒后，首先调用各socket对应的handler处理socket事件，这些handler都是框架注册的，比如收网络数据、接收新连接，**等这些io操作处理好了，再通过mvbs_event_send唤醒用户线程**
- `mvbs_event_send`唤醒用户线程是通过`pthread_cond_t`条件变量实现的，main函数中用户线程调用`mvbs_event_loop_wait`就是阻塞在`mvbs_event_wait`

```c
// mvbs/posix_aux/src/loop.c
// 函数mvbs_event_loop_create代码片段，有删减

struct mvbs_event_loop* mvbs_event_loop_create(uint32_t peroid_ms) {
    struct mvbs_event_loop* e;

    // socket线程/定时器线程唤醒用户线程的事件通道
    e->ev = mvbs_event_create();
    e->peroid_ms = peroid_ms;

    // 创建定时器线程
    pthread_create(&e->timer_thrd, &attr, timer_event_handle, e);
    pthread_detach(e->timer_thrd);

    adapter_socket_init();

    // 创建socket线程
    pthread_create(&e->sock_thrd, &attr, socket_recv_loop, e);
    pthread_detach(e->sock_thrd);

    return e;
}
```

 > 定时器线程和socket线程是不是可以合并，通过调整epoll的timeout来实现定时功能？
{: .prompt-info }

## 3. 数据收发流程

接下来以官方的rpc_test为例，看下数据收发流程。  
基于dds的pub/sub数据收发流程类似，只是序列化/反序列化、Transport部分有差异，本文不再展开，后面看情况是否单写一篇。

整体数据收发流程如下图所示([点击查看高清大图](/assets/images/2025-07-29-first_impressions_of_haloos_vbslite/rpc_data_flow.svg))，红色虚线左侧是client进程，右侧是server进程:

![rpc数据收发流程](/assets/images/2025-07-29-first_impressions_of_haloos_vbslite/rpc_data_flow.svg)

我们重点关注以下几点：
- **socket线程**收到数据后，是先放在FIFO队列的，**用户线程**收到`MVBS_EV_TIMER`事件后消费数据(不知道为啥没监听`MVBS_EV_RECV`)。以server端为例，**socket线程**在第6步通过`adapter_socket_tcp_read`接收数据后，放到FIFO队列，**用户线程**在第9步通过`rpc_server_recv_loop`调用`rpc_connection_recv`，从FIFO中取数据。因为FIFO的生产者和消费者在不同的线程，所以要有锁保护。
- rpc调用的超时机制：client端轮询实现，如果server长时间未响应，或者由于FIFO溢出导致没收到reply，`add_cb`会收到`RPC_ERRNO_TIMEOUT`
- server如何知道调用的是哪个rpc函数：client端在发送请求的时候带上模块名和函数名，服务端收到后根据模块名和函数名匹配回调函数，如下面代码所示：

    ```c
    // client端生成代码：rpc_test_client/gen/rpc_test/calculatorRpcClient.c

    // client发送前在函数MVBS_calculator_add中序列化了模块名和rpc函数名

    /*  step4: serialize the interface name */
    if (mcdr_serialize_string(&stream, "MVBS_calculator") == false)
        goto MCDR_FAIL;

    /*  step5: serialize the operation name */
    if (mcdr_serialize_string(&stream, "add") == false)
        goto MCDR_FAIL;
    ```

    ```c
    // server端生成代码：rpc_test_client/gen/rpc_test/calculatorRpcServer.c

    // server收到请求后匹配interface和operation决定调用哪个rpc函数

    static struct rpc_srv_handler srv_tab[] = {
        {
            .interface = "MVBS_calculator",
            .operation = "add",
            .handle = MVBS_calculator_add_handle,
            .svc_cb = MVBS_calculator_add_svc,
            .stream = 0,
            .active = 0,
            .sn = 0,
        },
    };
    ```
- 上面rpc_srv_handler中的`MVBS_calculator_add_svc`用**强弱符号机制**实现了类似c++中的Overriding功能，生成代码`rpc_test_client/gen/rpc_test/calculatorRpcServer.c`中的函数`MVBS_calculator_add_svc`是空壳，被添加了`__attribute__((weak))`属性，是弱符号，用户代码`examples/rpc_test/server.c`中的`MVBS_calculator_add_svc`是强符号，在编译的时候，最终链接的是用户代码中的强符号。

- client收到reply后，如何知道调用哪个callback：client在每次rpc调用前都通过`rpc_client_alloc_sn`分配了序号并附加在请求报文中，server在reply中回传该序号，client通过这个序号查找对应的callback。

 > 现在rpc仅支持tcp，并且仅支持异步调用（基于上述线程模型分析，vbslite现在的框架是无法支持同步调用的）。
{: .prompt-info }

## 4. 内存使用情况

### 4.1 拷贝次数

上述收发过程对应的内存拷贝情况如下：
- 收发双方必要的序列化、反序列化
- socket通信用户态与内核态之间的拷贝（图中的1、2、5、6四处）
- 进出FIFO的内存拷贝（图中的3、4、7、8四处）

![rpc内存拷贝次数](/assets/images/2025-07-29-first_impressions_of_haloos_vbslite/rpc_mem_operation.jpg)

上述这些拷贝，在RPC通信中都是中规中矩的，在本机IPC通信中通过其他方法存在优化的可能性（需要综合考虑系统调用的开销，比如数据量较大时使用memfd）。官方提到的零拷贝在vbslite版本中暂未看到，可能vbspro配合理想自己的vcos有这个特性，后面有时间再研究下。


### 4.2 内存预分配 

在vbslite中，内存都是预分配的。比如上面的例子rpc_test，在`app_init`中就通过`mvbs_mm_init`预分配了内存。这里的预分配并不是调用系统函数`malloc`分配一大块内存，而是通过全局静态变量的方式占用了一大块内存，在程序加载的时候内存就分配好了。

```c
// mvbs_mm_init中预分配的内存

// mvbs/src/adapter/posix/src/mvbs_adapter_base.c

#define MVBS_HEAP_SIZE_L	(256 * 1024)
static uint8_t mvbs_heap_5[MVBS_HEAP_SIZE_L];

static struct mem_region mvbs_heap_region[] = {
    {
        .mr_start = mvbs_heap_5,
        .mr_size = MVBS_HEAP_SIZE_L,
    },
    {
        .mr_start = NULL,
        .mr_size = 0,
    }
};
```

预分配的内存通过`mvbs_mm_region_register`交给vbslite自己的内存管理模块，后续动态内存的申请、释放调用`mvbs_malloc`、`mvbs_free`等函数就行了。前面提到的FIFO队列，也是从这里申请的内存。

这样做的好处是:
- 避免了调用系统内存管理函数导致的**时延不确定性**，这点对于实时操作系统很重要
- **踩内存检测机制**：
    - 为每块内存打了标记，free的时候检测被释放内存块是否被踩
    - 提供了`mvbs_mm_check_guard`函数，可以遍历整个预分配内存区，检测是否有内存块被踩。这种机制对开发同学定位问题很有用，之前我在定位一个踩内存问题时也用过[类似方式](http://tech.coderhuo.tech/posts/DMA_mem_crash/#51-hook-threadx%E8%87%AA%E5%B8%A6%E7%9A%84%E5%86%85%E5%AD%98%E6%8E%A5%E5%8F%A3)
- 便于统计内存使用情况

 > mvbs_malloc、mvbs_free并不是线程安全的，需避免多线程并发使用。
{: .prompt-info }


## 5. 总体观感
从技术层面看，`vbslite`如果跑在其他系统上进行商业化使用，`happy path`问题不大，`unhappy path`则还有很多工作要做。

不过`vbslite`毕竟刚开源，希望后面会越来越完善，成为行业的福祉。