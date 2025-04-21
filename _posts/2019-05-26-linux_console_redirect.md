---
title:  "Linux控制台重定向方法"  
date:   2019-05-26
categories: [计算机基础]  
tags: [stdin/stdout, 重定向]
---

* content
{:toc}

本文介绍一种通过文件描述符重定向终端输入/输出的方法。




## 一、背景 ##

一些嵌入式设备，一般都会留有调试串口，经由RS232/485标准与PC的COM口相连，将打印输出在PC上显示，并可以接收PC端的输入，如下图所示：  

![连接示意图](https://github.com/sigusr1/blog_assets/blob/master/2019-05-26-linux_console_redirect/pc_com_dev.png?raw=true)

设备出厂部署后，不方便接调试串口，查看设备输出就变得比较困难，不利于问题定位。  
如果设备具有联网能力，我们可以通过telnet或者ssh登录到设备上，进行远程调试。  
这时候就面临一个问题：如何把设备的打印信息显示出来？  

常见做法有以下几种：  

- 如果设备有日志文件，可以直接读取日志文件。但是一般情况下并非所有打印都写日志，这会导致部分内容看不到；另外，如果日志文件有加密，就不利于实时查看。
- 类似dmesg做个日志缓存，直接读取缓存日志。这需要消耗额外的内存，并且要修改现有的打印系统。
- hook打印接口，把打印内容向telent/ssh终端分发一份。这也需修改现有的打印机制。

以上几种做法各有优劣，下面介绍一种通过文件描述符重定向终端输入/输出的方法。   

## 二、原理 ##  

下图展示了Linux系统中标准输入/输出（STDIN/STDOUT）与控制终端的关系，其中ttyS0即串口:

![标准输入输出与串口设备的关系](https://github.com/sigusr1/blog_assets/blob/master/2019-05-26-linux_console_redirect/std_with_ttyS.png?raw=true)

用户通过telnet或者ssh登录后，会动态生成一个控制终端(比如/dev/pts/0)，如下图所示：

![未重定向的网络终端](https://github.com/sigusr1/blog_assets/blob/master/2019-05-26-linux_console_redirect/net_console_no_redirect.png?raw=true)

我们是否可以把标准输入/输出（STDIN/STDOUT）从ttyS0解绑，重新映射到pts0上呢？答案是肯定的。  

如下图所示，重新绑定后，打印就可以直接输出到telnet或者ssh对应的控制台，经由网络传输到PC上；同时，也可以从PC上接收输入（如果应用程序监听了STDIN，PC上的输入就可以直接被应用程序读取到，不重定向的话是收不到的）。  

*注：在某个控制终端执行的命令（启动的程序），默认绑定当前终端，所以正常情况下telnet或者ssh到设备后，执行`ls`等命令，输出都是在当前终端。*

![重定向后的网络终端](https://github.com/sigusr1/blog_assets/blob/master/2019-05-26-linux_console_redirect/net_console_redirect.png?raw=true)

## 三、实现 ##  

具体实现代码可以参考[https://github.com/sigusr1/redirect_console](https://github.com/sigusr1/redirect_console)。  

如下图所示，应用程序中需要集成一个Server，用来接收Client发送来的重定向指令。

![重定向实现方式](https://github.com/sigusr1/blog_assets/blob/master/2019-05-26-linux_console_redirect/implement.png?raw=true)

相关过程说明如下：

1. 在telnet或者ssh对应的终端上，执行可执行程序Client。
2. Client调用系统函数ttyname获取当前控制终端名称(一般为/dev/pts/0)，并将相关信息发送给Server。
3. Client和Server之间的通信方式可以是本地域套接字，也可以是本地网络套接字。不过应用程序不能直接监听STDIN，因为默认只能收到串口终端上的输入，telnet/ssh终端上的输入它收不到。
4. Server收到重定向指令后，执行下面的代码段，将STDOUT重定向到telnet/ssh对应的控制终端（/dev/pts/0）。

    ```c
    int fd_out = open("/dev/pts/0", O_WRONLY);
    if (fd_out < 0)
    {
        printf("Fail to open /dev/pts/0.error:%s", strerror(errno));
        return;
    }
    dup2(fd_out, STDOUT_FILENO);
    close(fd_out);
    ```

5. 同理，STDIN和STDERR也可以这样重定向。
6. 在重定向前，可以通过下面的代码将标准输入/输出绑定的终端备份下，这样执行`dup2(fd_out_bak, STDOUT_FILENO)`就可以还原原来的终端，达到以下效果：一个telnet已经把打印拉过来了，当它不想用的时候，发送还原指令，打印就又跑到原来的终端那边了。
    ```c
    fd_out_bak = dup(STDOUT_FILENO);
    fd_in_bak = dup(STDIN_FILENO);
    ```

## 四、优劣点分析 ##  

优点：  

1. 利用Linux系统特性实现，不需要修改原日志模块功能，基本不影响原系统性能
2. STDIN/STDOUT/STDERR均可重定向，方便实时查看、交互，并且可恢复到原终端

缺点：  

1. 依赖Linux系统，其他系统（比如一些RTOS）不一定适用
2. 需要集成一个client、server的本地通信框架
3. 只能重定向某个进程的输入/输出，其他进程、内核的打印无法重定向（直接执行`cat /proc/kmsg`命令可以远程实时查看内核打印）