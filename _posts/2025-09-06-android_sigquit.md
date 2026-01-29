---
title: "Android中信号SIGQUIT的特殊作用"
date: 2025-09-06
categories: [Android]
tags:  [SIGQUIT]
---


正常情况下，进程收到SIGQUIT信号会生成coredump然后退出。但在Android系统中，SIGQUIT有特殊用途：用来dump线程状态/调用栈。

*注：本文内容基于[android-14.0.0_r74](https://cs.android.com/android/platform/superproject/+/android-14.0.0_r74:?hl=zh-cn)。*


## 1. 背景

近期发现一个问题，执行`bugreportz`收集信息的时候，系统中有几个应用会退出，退出原因是收到了`SIGQUIT`信号。

查看Android源码发现，`bugreportz`会调用`dumpstate`收集系统中各进程的调用栈等信息，`dumpstate`是通过向目标进程发送`SIGQUIT`信号触发信息收集流程的：
![bugreportz工作流程](/assets/images/2025-09-06-android_sigquit/bugreport_workflow.jpg)


默认情况下，进程收到信号`QIGQUIT`后的预期行为是生成coredump并退出。那些不退出的进程，一定是做了特殊处理，比如：
1. 注册信号处理函数(**注意：信号处理函数在哪个线程执行是不确定的**)
2. 所有的线程都屏蔽信号`SIGQUIT`，只留一个专用线程处理该信号

Android中采用的是第2种方式，它创建了一个专门的信号处理线程`Signal Catcher`，收到`SIGQUIT`后dump其他线程的调用栈等信息。

出问题的几个进程，都有一个线程调用`pthread_sigmask`修改了`blocked signals`，导致未阻塞`SIGQUIT`，而`dumpstate`发送的`SIGQUIT`又恰好分到了该线程执行，结果就是生成coredump然后退出。


Android中`SIGQUIT`相关处理流程如下图所示：
- zygote进程初始化的时候就屏蔽了信号`SIGQUIT`，fork出来的app进程自然也屏蔽了这个信号
- 由于创建新线程的时候会继承当前线程的`sigmask`，所以app进程后续创建的线程也都自动屏蔽该信号
- app进程在执行用户代码前，创建了专门的信号处理线程`Signal Catcher`，用于处理`SIGQUIT`等信号

![SIGQUIT处理流程简图](/assets/images/2025-09-06-android_sigquit/sigquit_simple_block_workflow.jpg)

如果你只关心基本原理，本文读到这里就可以了。  
如果你想看下Android是怎么实现的（最好是参照源码阅读），请继续。

## 2. `SIGQUIT`屏蔽流程

前面提到zygote中就屏蔽了信号`SIGQUIT`，所以这块只涉及zygote的初始化过程。zygote的初始化主要分为下面几部分（下图头部横向部分所示）：

- init.rc脚本启动zygote进程主程序
- 初始化Android运行环境，这里的`AndroidRuntime`只是个抽象层，或者可以理解成更上层的运行环境（相对于下面的ART来讲）
- `JniInvocation`加载真正的运行环境实现库`libart.so`，这里可以配置加载哪种运行环境，Dalvik切换ART就是在这里实现的；如果你自己实现了一套运行环境，只需要在这里适配就行，其他地方不需要修改
- 初始化ART运行环境，创建Java虚拟机


具体调用链图中标注的比较清晰，这里就不再赘述了。[点击查看高清大图](/assets/images/2025-09-06-android_sigquit/zygote_block_sigquit.drawio.svg)。

![SIGQUIT屏蔽流程图](/assets/images/2025-09-06-android_sigquit/zygote_block_sigquit.jpg)

上图最后一步`Runtime::BlockSignals`的实现如下(看到`SIGQUIT`了吧)：
```c++
// art/runtime/runtime.cc

void Runtime::BlockSignals() {
  SignalSet signals;
  signals.Add(SIGPIPE);
  // SIGQUIT is used to dump the runtime's state (including stack traces).
  signals.Add(SIGQUIT);
  // SIGUSR1 is used to initiate a GC.
  signals.Add(SIGUSR1);
  signals.Block();
}
```

`SignalSet::Block`最终调用`pthread_sigmask64`对信号进行屏蔽：
```c++
void Block() {
  if (pthread_sigmask64(SIG_BLOCK, &set_, nullptr) != 0) {
    PLOG(FATAL) << "pthread_sigmask failed";
  }
}
```

## 3. `SIGQUIT`捕获流程

`SIGQUIT`的捕获流程如下图所示：
- `zygote`在第3步fork出应用进程后，应用进程也是屏蔽了信号`SIGQUIT`的
- 在执行用户代码前，框架层经过一番调用，最终在第10步创建了`Signal Catcher`线程，专门用来处理`SIGQUIT`等信号

具体调用链图中标注的比较清晰，这里就不再赘述了。[点击查看高清大图](/assets/images/2025-09-06-android_sigquit/app_wait_sigquit.drawio.svg)。

![SIGQUIT捕获流程](/assets/images/2025-09-06-android_sigquit/app_wait_sigquit.jpg)

`Signal Catcher`线程主函数如下：

```c++
// art/runtime/signal_catcher.cc

void* SignalCatcher::Run(void* arg) {
  SignalCatcher* signal_catcher = reinterpret_cast<SignalCatcher*>(arg);

  // ... 删减部分非相关代码

  // Set up mask with signals we want to handle.
  SignalSet signals;
  signals.Add(SIGQUIT);
  signals.Add(SIGUSR1);

  while (true) {
    int signal_number = signal_catcher->WaitForSignal(self, signals);
    switch (signal_number) {
    case SIGQUIT:
      signal_catcher->HandleSigQuit();
      break;
    case SIGUSR1:
      signal_catcher->HandleSigUsr1();
      break;
    default:
      LOG(ERROR) << "Unexpected signal %d" << signal_number;
      break;
    }
  }
}
```

其中`SignalCatcher::WaitForSignal`又调用`SignalSet::Wait`，最终阻塞在函数`sigwait64`上等待信号：

```c++
int Wait() {
  // Sleep in sigwait() until a signal arrives. gdb causes EINTR failures.
  int signal_number;
  int rc = TEMP_FAILURE_RETRY(sigwait64(&set_, &signal_number));
  if (rc != 0) {
    PLOG(FATAL) << "sigwait failed";
  }
  return signal_number;
}
```

进程收到`SIGQUIT`信号后，因为其他线程都屏蔽了该信号，只能发给`Signal Catcher`线程处理，在该线程上调用函数`SignalCatcher::HandleSigQuit`输出调用栈等信息。


## 4. 其他

不同于`SIGQUIT`，Android为`SIGSEGV`注册了信号处理函数，这个操作也是在zygote中就完成的，这样fork出来的app进程就自动继承了这个信号处理函数。

具体流程和本文第2部分[`SIGQUIT`的屏蔽流程]类似，`Runtime::Init`在调用`Runtime::BlockSignals`屏蔽了`SIGQUIT`信号之后，紧接着就为`SIGSEGV`等信号注册了处理函数，具体调用链如下：
1. `art/runtime/runtime.cc`中的`Runtime::Init`
2. `art/runtime/runtime_android.cc`中的`InitPlatformSignalHandlers`
3. `art/runtime/runtime_common.cc`中的`InitPlatformSignalHandlersCommon`注册了下面几个信号的处理函数：
  - 信号处理函数是`art/runtime/runtime_android.cc`中的`HandleUnexpectedSignalAndroid`
  - 它又调用`art/runtime/runtime_common.cc`中的`HandleUnexpectedSignalCommon`dump 调用栈等crash信息

```c++
void InitPlatformSignalHandlersCommon(void (*newact)(int, siginfo_t*, void*),
                                      struct sigaction* oldact,
                                      bool handle_timeout_signal) {
  // ...

  int rc = 0;
  rc += sigaction(SIGABRT, &action, oldact);
  rc += sigaction(SIGBUS, &action, oldact);
  rc += sigaction(SIGFPE, &action, oldact);
  rc += sigaction(SIGILL, &action, oldact);
  rc += sigaction(SIGPIPE, &action, oldact);
  rc += sigaction(SIGSEGV, &action, oldact);
#if defined(SIGSTKFLT)
  rc += sigaction(SIGSTKFLT, &action, oldact);
#endif
  rc += sigaction(SIGTRAP, &action, oldact);
}
```
