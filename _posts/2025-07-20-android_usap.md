---
title: "Android USAP简介"
date: 2025-07-20
categories: [Android]
tags:  [USAP]
---

* content
{:toc}


本文主要介绍Android中的[USAP(unspecialized app processes)](https://source.android.com/docs/core/runtime/zygote)机制。 

*注：本文内容基于[android-14.0.0_r74](https://cs.android.com/android/platform/superproject/+/android-14.0.0_r74:?hl=zh-cn)，并且只关注64位程序。*

## 1. 背景

众所周知，Android中所有的app进程都是zygote fork出来的：
- 默认情况下，来一个启动app的请求，zygote fork一个进程，并在这个进程中执行app代码，如下图黑色路径所示。  
- 那么，是否可以预先fork一些进程，请求到来的时候直接拿来用，以此加快app启动速度？USAP就是做这个事情的。如下图红色路径所示，zygote先fork一些进程放在**USAP进程池**，要启动app的时候，就从池子里拿一个使用。

![zygote fork usap示意图](http://data.coderhuo.tech/2025-07-20-android_usap/usap_overview.jpg)

 > **USAP进程是一次性的，用完之后不能再放回池子里了**，zygote进程会fork新的进程填充USAP进程池。
{: .prompt-info }

## 2. 功能开关

Android上默认关闭了USAP功能，如果要使用，可以在编译时修改下面几个property的值，或者在运行时通过`setprop`命令动态修改（修改后无需重启zygote进程，下一次启动app时自动生效）。
- dalvik.vm.usap_pool_enabled: 功能开关
- dalvik.vm.usap_pool_size_max：最多几个USAP进程
- dalvik.vm.usap_pool_size_min：最少几个USAP进程
- dalvik.vm.usap_refill_threshold：触发zygote填充USAP进程池的阈值，填充USAP进程池有两个策略：
    - 延迟填充：**(dalvik.vm.usap_pool_size_max - 当前USAP进程数 > dalvik.vm.usap_refill_threshold)**时，延时dalvik.vm.usap_pool_refill_delay_ms再创建USAP进程
    - 立即填充：**当前USAP进程数 < dalvik.vm.usap_pool_size_min**时，立即创建USAP进程
- dalvik.vm.usap_pool_refill_delay_ms：延时多久再去创建USAP进程

```makefile
# build/make/target/product/runtime_libart.mk

# Properties for the Unspecialized App Process Pool
PRODUCT_SYSTEM_PROPERTIES += \
    dalvik.vm.usap_pool_enabled?=false \
    dalvik.vm.usap_refill_threshold?=1 \
    dalvik.vm.usap_pool_size_max?=3 \
    dalvik.vm.usap_pool_size_min?=1 \
    dalvik.vm.usap_pool_refill_delay_ms?=3000
```

 > Google默认关闭了USAP功能，是不是效果不理想？简单测了下，从system_server发起请求到Activity.onCreate，没看到明显的优化。如果要在USAP中做一些定制化的预加载，就另当别论了。
{: .prompt-info }

## 3. USAP进程生命周期

### 3.1 USAP进程创建流程

zygote创建USAP进程，有以下触发源：
- system_server请求zygote检查USAP状态（你是不是准备好了）
- USAP进程退出（死了一个，再fork一个补上）
- USAP进程被实例化为app(用掉一个，再fork一个补上)

![USAP进程创建流程图](http://data.coderhuo.tech/2025-07-20-android_usap/usap_create.jpg)

system_server在请求zygote创建进程的的时候，先调用`ZygoteProcess.informZygotesOfUsapPoolStatus`通知zygote准备USAP进程，这个调用是进程间的IPC，要考虑开销，system_server在下面两种情况检查`ZygoteConfig.USAP_POOL_ENABLED`的值是否发生了变化，只有变化的时候才调用上述函数：
- 首次请求zygote创建进程
- 调用间隔超过了Zygote.PROPERTY_CHECK_INTERVAL（默认60s）

```java
// system_server请求zygote创建进程时先调用informZygotesOfUsapPoolStatus

// frameworks/base/core/java/android/os/ZygoteProcess.java

// Start a new process.
public final Process.ProcessStartResult start(...) {
    // 判断是否需要通知zygote准备USAP进程
    // TODO (chriswailes): Is there a better place to check this value?
    if (fetchUsapPoolEnabledPropWithMinInterval()) {
        informZygotesOfUsapPoolStatus();
    }

    try {
        return startViaZygote(processClass, niceName, uid, gid, gids,
                runtimeFlags, mountExternal, targetSdkVersion, seInfo,
                abi, instructionSet, appDataDir, invokeWith, /*startChildZygote=*/ false,
                packageName, zygotePolicyFlags, isTopApp, disabledCompatChanges,
                pkgDataInfoMap, allowlistedDataInfoList, bindMountAppsData,
                bindMountAppStorageDirs, bindOverrideSysprops, zygoteArgs);
    } catch (ZygoteStartFailedEx ex) {
        Log.e(LOG_TAG,
                "Starting VM process through Zygote failed");
        throw new RuntimeException(
                "Starting VM process through Zygote failed", ex);
    }
}
```

上述IPC通信是通过名为`Zygote.PRIMARY_SOCKET_NAME`(`/dev/socket/zygote`)的`Unix Domain Socket`完成的，这是zygote进程和system_server进程之间的一个长连接：
```java
// informZygotesOfUsapPoolStatus通知zygote准备USAP进程并等待结果

// frameworks/base/core/java/android/os/ZygoteProcess.java

/**
* Sends messages to the zygotes telling them to change the status of their USAP pools.  If
* this notification fails the ZygoteProcess will fall back to the previous behavior.
*/
private void informZygotesOfUsapPoolStatus() {
    final String command = "1\n--usap-pool-enabled=" + mUsapPoolEnabled + "\n";

    synchronized (mLock) {
        try {
            // 通过本地套接字连接zygote
            attemptConnectionToPrimaryZygote();

            // 发送请求
            primaryZygoteState.mZygoteOutputWriter.write(command);
            primaryZygoteState.mZygoteOutputWriter.flush();
        } catch (IOException ioe) {
            // ...
        }

        // ...

        // Wait for the response from the primary zygote here so the primary/secondary zygotes
        // can work concurrently.
        try {
            // 等待结果
            // Wait for the primary zygote to finish its work.
            primaryZygoteState.mZygoteInputStream.readInt();
        } catch (IOException ioe) {
            // ...
        }
    }
}
```
上述socket请求会唤醒`ZygoteServer.runSelectLoop`中的`Os.poll`, 通过下面的调用链fork USAP进程：
      `ZygoteConnection.handleUsapPoolStatusChange` --> `ZygoteServer.setUsapPoolStatus`  --> `ZygoteServer.fillUsapPool` --> `Zygote.forkUsap`

`Zygote.forkUsap`最终通过系统函数fork创建新进程，有下面几点需要注意：
- fork前Zygote必须是单线程的，否则会有问题（参考[fork导致的死锁问题](http://tech.coderhuo.tech/posts/dead_lock_after_fork/)），这一点`ZygoteServer.fillUsapPool`中的`ZygoteHooks.preFork()`已经保证了。
- USAP进程从zygote进程继承了域套接字`ZygoteServer.mUsapPoolSocket`（名字是`Zygote.USAP_POOL_PRIMARY_SOCKET_NAME`，即`/dev/socket/usap_pool_primary`），USAP进程监听这个socket，用来接收system_server实例化app的请求。zygote在这个套接字上做了个小文章：
    - zygote进程已经执行了listen操作，但是没有accept，这样新创建的USAP进程直接在这个fd上执行accept就行了，减少了系统调用；
    - 另外，每个USAP进程都从zygote进程继承这个fd，所以它们是监听在同一个套接字上的，system_server作为客户端发起请求的时候，kernel决定分配给谁，并且保证只唤醒一个USAP进程。
- zygote进程和每个USAP进程间都有一个pipe（通过`Os.pipe2`创建），zygote持有读端，USAP进程持有写端，USAP在实例化为app时通过它告知zygote进程可以补充新进程了。
- fork出来的USAP进程调用`Zygote.childMain`执行预加载代码，并等待system_server的实例化app请求，后面会展开介绍。

```java
// frameworks/base/core/java/com/android/internal/os/Zygote.java

/**
 * Fork a new unspecialized app process from the zygote. Adds the Usap to the native
 * Usap table.
 *
 * @param usapPoolSocket  The server socket the USAP will call accept on
 * @param sessionSocketRawFDs  Anonymous session sockets that are currently open.
 *         These are closed in the child.
 * @param isPriorityFork Raise the initial process priority level because this is on the
 *         critical path for application startup.
 * @return In the child process, this returns a Runnable that waits for specialization
 *         info to start an app process. In the sygote/parent process this returns null.
 */
static @Nullable Runnable forkUsap(LocalServerSocket usapPoolSocket,
                                    int[] sessionSocketRawFDs,
                                    boolean isPriorityFork) {
    FileDescriptor readFD;
    FileDescriptor writeFD;

    try {
        // 创建pipe，zygote持有读端，USAP持有写端
        FileDescriptor[] pipeFDs = Os.pipe2(O_CLOEXEC);
        readFD = pipeFDs[0];
        writeFD = pipeFDs[1];
    } catch (ErrnoException errnoEx) {
        throw new IllegalStateException("Unable to create USAP pipe.", errnoEx);
    }

    // 调用native代码执行真正的fork
    int pid = nativeForkApp(readFD.getInt$(), writeFD.getInt$(),
                            sessionSocketRawFDs, /*argsKnown=*/ false, isPriorityFork);
    if (pid == 0) {
        IoUtils.closeQuietly(readFD);
        return childMain(null, usapPoolSocket, writeFD);
    } else if (pid == -1) {
        // Fork failed.
        return null;
    } else {
        // readFD will be closed by the native code. See removeUsapTableEntry();
        IoUtils.closeQuietly(writeFD);
        nativeAddUsapTableEntry(pid, readFD.getInt$());
        return null;
    }
}
```

USAP进程退出（上图3.1的流程）或者实例化为app(上图3.2的流程)都会告知zygote，唤醒`ZygoteServer.runSelectLoop`中的`Os.poll`，重新填充USAP进程池。zygote为了避免在app初始化的时候创建USAP进程，影响app启动速度，会根据设置决定是否执行延时填充策略。

### 3.2 USAP进程实例化流程

整体流程如下所示：

![USAP进程实例化示意图](http://data.coderhuo.tech/2025-07-20-android_usap/usap_to_app.jpg)

system_server在下面的代码中调用`ZygoteProcess.attemptUsapSendArgsAndGetResult`请求USAP执行app代码，该请求是通过套接字`Zygote.USAP_POOL_PRIMARY_SOCKET_NAME`发送的。可以看到system_server先尝试使用USAP，不行的话再使用zygote。

```java
// frameworks/base/core/java/android/os/ZygoteProcess.java

/**
 * Sends an argument list to the zygote process, which starts a new child
 * and returns the child's pid. Please note: the present implementation
 * replaces newlines in the argument list with spaces.
 *
 * @throws ZygoteStartFailedEx if process start failed for any reason
 */
@GuardedBy("mLock")
private Process.ProcessStartResult zygoteSendArgsAndGetResult(
        ZygoteState zygoteState, int zygotePolicyFlags, @NonNull ArrayList<String> args)
        throws ZygoteStartFailedEx {

    // ...

    if (shouldAttemptUsapLaunch(zygotePolicyFlags, args)) {
        try {
            // zygoteState中包含套接字Zygote.USAP_POOL_PRIMARY_SOCKET_NAME
            return attemptUsapSendArgsAndGetResult(zygoteState, msgStr);
        } catch (IOException ex) {
            // If there was an IOException using the USAP pool we will log the error and
            // attempt to start the process through the Zygote.
            Log.e(LOG_TAG, "IO Exception while communicating with USAP pool - "
                    + ex.getMessage());
        }
    }

    // 如果USAP失败了，再通过zygoye创建进程
    return attemptZygoteSendArgsAndGetResult(zygoteState, msgStr);
}
```

USAP进程被fork出来后，就阻塞在`Zygote.childMain`中的`usapPoolSocket.accept()`中，这里的`usapPoolSocket`就是从zygote继承来的`ZygoteServer.mUsapPoolSocket`：

```java
// frameworks/base/core/java/com/android/internal/os/Zygote.java

// 函数childMain代码片段

while (true) {
    try {
        // 阻塞在这里等待system_server的请求
        sessionSocket = usapPoolSocket.accept();
        break;
    } catch (Exception ex) {
        Log.e("USAP", ex.getMessage());
    }
    IoUtils.closeQuietly(sessionSocket);
}

// 关闭监听套接字，后续不再接受system_server的请求，因为USAP进程是一次性的。

// Since the raw FD is created by init and then loaded from an environment
// variable (as opposed to being created by the LocalSocketImpl itself),
// the LocalSocket/LocalSocketImpl does not own the Os-level socket. See
// the spec for LocalSocket.createConnectedLocalSocket(FileDescriptor fd).
// Thus closing the LocalSocket does not suffice. See b/130309968 for more
// discussion.
FileDescriptor fd = usapPoolSocket.getFileDescriptor();
usapPoolSocket.close();
Os.close(fd);
```

收到system_server请求后，上述循环被打破，继续往下执行：
 - 首先关闭监听套接字usapPoolSocket(相关操作在上面代码块中)，后续不再接受system_server的请求，因为USAP进程是一次性的。
 - 然后通过pipe告知zygoye我即将被实例化为app，以后就和你没关系了（关闭了和zygote之间的pipe）。这会唤醒zygote的`ZygoteServer.runSelectLoop`，触发USAP创建流程，重新填充USAP进程池。zygote为了避免在app初始化的时候创建USAP进程，影响app启动速度，会根据设置决定是否执行延时填充策略。
 - USAP进程脱离了父进程的控制，可以为所欲为了，最终调用specializeAppProcess蜕变成了一个app。


```java
// frameworks/base/core/java/com/android/internal/os/Zygote.java

// 函数childMain代码片段

// 写pipe通知zygote进程
if (writePipe != null) {
    try {
        ByteArrayOutputStream buffer =
                new ByteArrayOutputStream(Zygote.USAP_MANAGEMENT_MESSAGE_BYTES);
        DataOutputStream outputStream = new DataOutputStream(buffer);

        // This is written as a long so that the USAP reporting pipe and USAP pool
        // event FD handlers in ZygoteServer.runSelectLoop can be unified.  These two
        // cases should both send/receive 8 bytes.
        // TODO: Needs tweaking to handle the non-Usap invoke-with case, which expects
        // a different format.
        outputStream.writeLong(pid);
        outputStream.flush();
        Os.write(writePipe, buffer.toByteArray(), 0, buffer.size());
    } catch (Exception ex) {
        Log.e("USAP",
                String.format("Failed to write PID (%d) to pipe (%d): %s",
                        pid, writePipe.getInt$(), ex.getMessage()));
        throw new RuntimeException(ex);
    } finally {
        IoUtils.closeQuietly(writePipe);
    }
}

// 实例化为app
specializeAppProcess(args.mUid, args.mGid, args.mGids,
                        args.mRuntimeFlags, rlimits, args.mMountExternal,
                        args.mSeInfo, args.mNiceName, args.mStartChildZygote,
                        args.mInstructionSet, args.mAppDataDir, args.mIsTopApp,
                        args.mPkgDataInfoList, args.mAllowlistedDataInfoList,
                        args.mBindMountAppDataDirs, args.mBindMountAppStorageDirs,
                        args.mBindMountSyspropOverrides);
```

### 3.3 USAP进程退出流程

USAP进程退出事件是由zygote进程监听到的，分两种情况处理：
- 实例化为app前，zygote需要重新填充USAP进程池（奇怪的是zygote也通知了system_server，按说没必要，因为这时候USAP进程和system_server还没建立任何联系）。
- 实例化为app后，zygote通知system_server应用进程退出了

![USAP进程退出流程](http://data.coderhuo.tech/2025-07-20-android_usap/usap_exit.jpg)

zygote进程监听了子进程退出的信号：
```c
// frameworks/base/core/jni/com_android_internal_os_Zygote.cpp

static void SetSignalHandlers() {
    struct sigaction sig_chld = {.sa_flags = SA_SIGINFO, .sa_sigaction = SigChldHandler};

    // 监听子进程退出信号，子进程退出会触发SigChldHandler函数
    if (sigaction(SIGCHLD, &sig_chld, nullptr) < 0) {
        ALOGW("Error setting SIGCHLD handler: %s", strerror(errno));
    }

  struct sigaction sig_hup = {};
  sig_hup.sa_handler = SIG_IGN;
  if (sigaction(SIGHUP, &sig_hup, nullptr) < 0) {
    ALOGW("Error setting SIGHUP handler: %s", strerror(errno));
  }
}
```

子进程退出会触发zygote回调SigChldHandler：
 - 首先调用sendSigChildStatus告知system_server有进程退出了，这个IPC通信是通过域套接字`kSystemServerSockAddr`进行的，zygote是客户端，system_server是server
 - 然后通过`RemoveUsapTableEntry`判断退出的进程是不是USAP进程，如果是的话，说明该USAP进程还没被实例化为app，就通过`gUsapPoolEventFD`唤醒`ZygoteServer.runSelectLoop`，重新填充USAP进程池。

```c
// frameworks/base/core/jni/com_android_internal_os_Zygote.cpp

// 子进程退出事件处理函数，有删减

// This signal handler is for zygote mode, since the zygote must reap its children
NO_STACK_PROTECTOR
static void SigChldHandler(int /*signal_number*/, siginfo_t* info, void* /*ucontext*/) {
    pid_t pid;
    int status;
    int64_t usaps_removed = 0;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // 通知system_server有进程退出了（如果退出的是未实例化的USAP进程，是不是没必要通知???）
        // Notify system_server that we received a SIGCHLD
        sendSigChildStatus(pid, info->si_uid, status);

        if (WIFEXITED(status)) {
            // Check to see if the PID is in the USAP pool and remove it if it is.
            if (RemoveUsapTableEntry(pid)) {
                ++usaps_removed;
            }
        } else if (WIFSIGNALED(status)) {
            // If the process exited due to a signal other than SIGTERM, check to see
            // if the PID is in the USAP pool and remove it if it is.  If the process
            // was closed by the Zygote using SIGTERM then the USAP pool entry will
            // have already been removed (see nativeEmptyUsapPool()).
            if (WTERMSIG(status) != SIGTERM && RemoveUsapTableEntry(pid)) {
                ++usaps_removed;
            }
        }

        // 如果退出的进程是system_server，zygote就自杀了
        // If the just-crashed process is the system_server, bring down zygote
        // so that it is restarted by init and system server will be restarted
        // from there.
        if (pid == gSystemServerPid) {
            async_safe_format_log(ANDROID_LOG_ERROR, LOG_TAG,
                                  "Exit zygote because system server (pid %d) has terminated", pid);
            kill(getpid(), SIGKILL);
        }
    }

    // 如果退出的是未实例化的USAP进程，唤醒ZygoteServer.runSelectLoop，重新填充USAP进程池
    if (usaps_removed > 0) {
        if (TEMP_FAILURE_RETRY(write(gUsapPoolEventFD, &usaps_removed, sizeof(usaps_removed))) ==
            -1) {
            // If this write fails something went terribly wrong.  We will now kill
            // the zygote and let the system bring it back up.
            async_safe_format_log(ANDROID_LOG_ERROR, LOG_TAG,
                                  "Zygote failed to write to USAP pool event FD: %s",
                                  strerror(errno));
            kill(getpid(), SIGKILL);
        }
    }
}
```

## 4. 参考文档
- [About the Zygote processes](https://source.android.com/docs/core/runtime/zygote)
- [一种新型的应用启动机制:USAP](https://juejin.cn/post/6922704248195153927)