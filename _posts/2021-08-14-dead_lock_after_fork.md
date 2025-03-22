---
title: "fork导致的死锁问题"
date: 2021-08-14
categories: [操作系统]
tags:  [死锁, fork] 
---

本文主要介绍fork导致的死锁问题及其解决方法。


先看一个示例程序，该程序有个全局对象sGlobalInstance，父进程先通过该对象执行了lock操作，然后执行fork，在子进程中，也去执行lock操作。可以先看下这个程序有没有问题:

```c++
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

class Test {
public:
    Test() {
        pthread_mutex_init(&mMutex, nullptr);
        printf("Init test instance pid:%u tid:%u\n", getpid(), gettid());
    }

    ~Test() {
        pthread_mutex_destroy(&mMutex);
    }

    void lock() {
        pthread_mutex_lock(&mMutex);
    }

    void unlock() {
        pthread_mutex_unlock(&mMutex);
    }

private:
    pthread_mutex_t mMutex;
};

static Test* sGlobalInstance = nullptr;

void* func(void* arg) {
    if (sGlobalInstance == nullptr) {
        sGlobalInstance = new Test();
    }

    printf("Before get lock pid:%u tid:%u\n", getpid(), gettid());
    sGlobalInstance->lock();
    printf("After get lock pid:%u tid:%u\n", getpid(), gettid());

    pause();
    return nullptr;
}

int main() {
    printf("In parent process. pid:%u tid:%u\n", getpid(), gettid());
    sGlobalInstance = new Test();

    pthread_t id;
    pthread_create(&id, nullptr, func, nullptr);
    // Sleep to make sure the thread get lock
    sleep(1);

    int pid = fork();
    if (pid < 0) {
        printf("Error occur while fork. errno:%d\n", errno);
        return errno;
    } else if (pid == 0) {
        // In child process
        printf("In child process. pid:%u tid:%u\n", getpid(), gettid());
        func(nullptr);
    } else {
        // In parent process
        pause();
    }
    
    return 0;
}
```

上面的程序执行结果如下，**子进程中没有拿到锁，产生了死锁**:  

```console
In parent process. pid:22287 tid:22287
Init test instance pid:22287 tid:22287
Before get lock pid:22287 tid:22288
After get lock pid:22287 tid:22288
In child process. pid:22293 tid:22293
Before get lock pid:22293 tid:22293
```


从上面的输出还可以看出, 全局对象sGlobalInstance仅在父进程中被初始化了一次，这是由于fork的**写时复制**机制导致的：子进程完全继承父进程的内存空间，仅当父进程或者子进程改变对应内存空间的内容时，才把对对应的内存空间分离（各人有各人的内存空间），否则二者会一直共用同一个内存空间。  

上面的程序之所以产生死锁，也是这个原因导致的（锁在父进程中处于lock状态，fork后，在子进程中这把锁也是lock状态）。



接下来看下谁拿了这把锁，通过gdb attach到子进程，可以看到下面的调用栈：

```console
gdb attach 22293

(gdb) bt
#0  0x0000007f9eaada30 in ?? () from /usr/lib64/libpthread.so.0
#1  0x0000007f9eaa5a2c in pthread_mutex_lock ()
   from /usr/lib64/libpthread.so.0
#2  0x0000000000400dac in ?? ()
#3  0x0000000000400c18 in ?? ()
#4  0x0000007f9e772058 in __libc_start_main () from /usr/lib64/libc.so.6
#5  0x0000000000400ca4 in ?? ()
Backtrace stopped: not enough registers or memory available to unwind further
```

由于可执行文件是release版的，缺少符号信息，调用栈并不直观。  
通过file命令加载debug版本的可执行程序，然后执行bt可以看到下面的调用栈:

```console
(gdb) file /data/dead_lock
(gdb) bt
#0  0x0000007f9eaada30 in ?? () from /usr/lib64/libpthread.so.0
#1  0x0000007f9eaa5a2c in pthread_mutex_lock ()
   from /usr/lib64/libpthread.so.0
#2  0x0000000000400dac in Test::lock (this=<optimized out>)
    at framework/libs/base/log/dead_lock.cpp:39
#3  func (arg=arg@entry=0x0) at framework/libs/base/log/dead_lock.cpp:39
#4  0x0000000000400c18 in main () at framework/libs/base/log/dead_lock.cpp:62
```

通过上面的调用栈，可以知道是sGlobalInstance中的mMutex发生了死锁。  
接下来看下谁持有了这把锁：

```console
(gdb) p sGlobalInstance->mMutex
$1 = {__data = {__lock = 2, __count = 0, __owner = 22288, __nusers = 1,
    __kind = 0, __spins = 0, __list = {__prev = 0x0, __next = 0x0}},
  __size = "\002\000\000\000\000\000\000\000\020W\000\000\001", '\000' <repeats 34 times>, __align = 2}
```

通过上面mMutex信息看到锁的owner是22287，正是父进程中创建的线程。

*注意：由于fork的**写时复制**机制，即使父进程后续释放了这把锁，子进程也感知不到（父进程释放锁时会写对应的标志位，这会导致父子进程的内存空间分离，在父进程中锁是已释放状态，在子进程中锁仍然被22287持有）。*

那么，这种情况怎么处理呢？  
pthread_atfork函数可以用来处理这种情况，该函数原型如下：
- 回调函数prepare在fork前调用
- fork后在父进程中调用回调函数parent
- fork后在子进程中调用回调函数child

```c++
int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));
```

利用pthread_atfork将上面的问题程序改造下，fork后通过pthread_atfork的回调函数child释放父进程中持有的锁：

```c++
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

class Test {
public:
    Test() {
        pthread_mutex_init(&mMutex, nullptr);
        printf("Init test instance pid:%u tid:%u\n", getpid(), gettid());
    }

    void lock() {
        pthread_mutex_lock(&mMutex);
    }

    ~Test() {
        pthread_mutex_destroy(&mMutex);
    }

    void unlock() {
        pthread_mutex_unlock(&mMutex);
    }

    void reset() {
        unlock();
    }

private:
    pthread_mutex_t mMutex;
};

static Test* sGlobalInstance = nullptr;

void* func(void* arg) {
    if (sGlobalInstance == nullptr) {
        sGlobalInstance = new Test();
    }

    printf("Before get lock pid:%u tid:%u\n", getpid(), gettid());
    sGlobalInstance->lock();
    printf("After get lock pid:%u tid:%u\n", getpid(), gettid());

    pause();
    return nullptr;
}

void child(void) {
    if (sGlobalInstance) {
        sGlobalInstance->reset();
    }
}

int main() {
    printf("In parent process. pid:%u tid:%u\n", getpid(), gettid());
    sGlobalInstance = new Test();

    pthread_t id;
    pthread_create(&id, nullptr, func, nullptr);
    // Sleep to make sure the thread get lock
    sleep(1);

    pthread_atfork(NULL, NULL, child);
    int pid = fork();
    if (pid < 0) {
        printf("Error occur while fork. errno:%d\n", errno);
        return errno;
    } else if (pid == 0) {
        // In child process
        printf("In child process. pid:%u tid:%u\n", getpid(), gettid());
        func(nullptr);
    } else {
        // In parent process
        pause();
    }
    return 0;
}
```

*注意：这里利用了pthread_mutex_t的一个特点，可以在A线程/进程lock然后在B线程/进程unlock。*


改进后的程序执行结果如下，这次子进程中没有产生死锁问题：
```console
In parent process. pid:23042 tid:23042
Init test instance pid:23042 tid:23042
Before get lock pid:23042 tid:23043
After get lock pid:23042 tid:23043
In child process. pid:23044 tid:23044
Before get lock pid:23044 tid:23044
After get lock pid:23044 tid:23044
```

*Tips: 如果同一进程内的不同线程间发生了死锁：可以在gdb中输入`thread apply all bt`把所有线程的调用栈打印出来，根据调用栈判断哪些线程发生了死锁。*


