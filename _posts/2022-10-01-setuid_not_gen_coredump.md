---
title: "uid/gid变更导致无法生成coredump的问题"
date: 2022-10-01
categories: [操作系统]
tags:  [capability, 安全] 
---


项目中遇到一个问题，某个进程已经通过`prctl(PR_SET_DUMPABLE, 1)`将其设置为dumpable，但还是无法生成coredump，其他进程能生成，说明系统配置没问题。最终查下来发现，是因为该进程变更uid/gid导致的。

下面用一个简化后的例子说明这个问题：

1. 在系统中创建一个新用户`test`:
    ```
    sudo adduser test
    ```
2. 执行命令 `cat /etc/passwd` 查看用户`test`的`uid`, 我的是1002
    ```
    test:x:1002:1002:,,,:/home/test:/bin/bash
    ```
3. 简化后的示例程序如下，注意用实际的`uid`替换宏TEST_UID的值:

    ```c++
    #include <stdio.h>
    #include <sys/prctl.h>
    #include <unistd.h>

    #define TEST_UID (1002)

    int main() {
        // 强制设置允许coredump
        prctl(PR_SET_DUMPABLE, 1);

        // 获取并打印当前dumpable状态
        int dumpable = prctl(PR_GET_DUMPABLE);
        printf("Before change uid, dumpable state of process %u is %d\n", getpid(), dumpable);

        // 在此之前通过 kill -11 pid 可以生成coredump
        getchar();

        // 改变进程的用户ID
        setuid(TEST_UID);

        // 获取并打印当前dumpable状态
        // dumpable的值被重置为用户1002的配置/proc/sys/fs/suid_dumpable
        dumpable = prctl(PR_GET_DUMPABLE);
        printf("After change uid, dumpable state of process %u is %d\n", getpid(), dumpable);

        // 现在还能不能生成coredump取决于用户test的配置/proc/sys/fs/suid_dumpable
        getchar();

        return 0;
    }
    ```

4. 编译运行，注意用root权限执行，因为上面代码中调用了`setuid`，普通用户是没这个权限的：
    ```
    gcc gen_coredump.c
    sudo ./a.out
    ```

5. 输出如下，可以看到dumpable的值在setuid后确实变了：

    ```
    Before change uid, dumpable state of process 1191147 is 1

    After change uid, dumpable state of process 1191147 is 0
    ```

6. 上述程序通过getchar设置了两个暂停点，感兴趣的同学可以试下，是不是前半段能生成coredump，后半段就无法生成了。

7. 这是因为setuid后，`dumpable`被设置成新用户`test`中`/proc/sys/fs/suid_dumpable`的值，参考[ptrace(2)](http://man.he.net/man2/prctl):  
![dumpable flag受用户变更影响](/assets/images/2022-10-01-setuid_not_gen_coredump/dumpable_change_small.jpg)
8. 通过下面的命令可以查看用户`test`中`/proc/sys/fs/suid_dumpable`的值，我的设备上确实是0（不同系统可能有差异）
    ```
    su test
    cat /proc/sys/fs/suid_dumpable
    ```
9. 系统的这种行为，主要是出于安全考虑：
    - 一般执行setuid/setgid的程序，都是先运行在特权模式（比如以root运行），执行一些只有特权用户才能执行的任务，然后修改uid/gid降权，运行在普通模式。
    - 特权模式下执行的代码，可能在内存中留下了痕迹（比如将密码读取到了内存），如果生成了coredump，或者允许gdb attach上去，可能会造成信息泄露。
    - 所以系统在uid/gid变更后，默认禁止生成coredump，也不能通过gdb attach进行debug。

10. 比较典型的例子是Android系统中zygote进程fork应用进程的场景：
    - zygote进程刚fork出来的应用进程是root权限，此时它可以执行普通用户无法执行的任务，比如给应用挂载数据目录等。
    - 执行完这些任务，在进入用户代码之前就降权，降权之后无法再提权，从而避免用户代码干坏事。
