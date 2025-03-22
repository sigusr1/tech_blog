---
title: "Linux capability多线程权限泄露示例"
date: 2022-02-01
categories: [操作系统]
tags:  [capability, 安全] 
---

[capabilities](https://man7.org/linux/man-pages/man7/capabilities.7.html)将系统root权限按功能单元划分，使用者按需打开/关闭相关权限，比基于[UID](https://man7.org/linux/man-pages/man7/credentials.7.html)的权限控制方式更精细。  

不过，**Linux下的capabilities是线程相关的，同一个进程的不同线程可以具有不同的capabilities权限**，使用不当，可能会造成权限泄露。  

可以使用[libpsx](https://www.man7.org/linux/man-pages/man3/libpsx.3.html)规避该问题，它的原理很简单，就是把当前进程中所有线程的capabilities设置一遍。


下面以一个文件访问的例子进行说明。

**相关背景知识：**
- 如果一个非root进程的uid和gid与目标文件的uid和gid均不同，该进程是不能访问这个文件的。不过，如果该进程获取了CAP_DAC_OVERRIDE权限，则可以绕过uid、gid相关权限校验访问目标文件。
- 通过[setuid](https://man7.org/linux/man-pages/man2/setuid.2.html)等函数改变uid，作用域是**进程**，gid的作用域也是进程
- capabilities的作用域是**线程**



**本文的实验场景如下：**

1. 主进程启动后，将自己的uid设置为master并设置capabilities权限（主要是CAP_DAC_OVERRIDE权限）
2. 主进程fork出子进程作为任务进程
3. 子进程将自己的uid设置为slave
4. 子进程启动任务线程（示例中的任务是读取一个文件）
5. 子进程在**主线程**中清理capabilities权限（**仅清理了主线程的权限，子线程中权限泄露**）


**操作步骤：**

- 添加两个用户，分别用于主进程和子进程

```console
sudo adduser master
sudo adduser slave
```

- 创建一个仅当前用户可读写的测试文件：

```console
echo "Only current user can open me" > ~/test.txt
chmod 600 ~/test.txt 
```

- 通过下面的命令可以看到，仅当前用户huo对该文件具有读写权限：

```console
ls -l ~/test.txt
-rw------- 1 huo huo 26 1月  30 16:44 /home/huo/test.txt
```

- 测试代码如下：  
  [cap.c](https://github.com/sigusr1/file_server/blob/main/2022-02-01-linux_capability_example/cap.c)

- 编译测试代码：

```console
gcc cap.c -lcap -lpthread
```
注：依赖libcap，`ubuntu`可通过命令`sudo apt install libcap-dev`命令安装。

- 运行编译出来的可执行程序，由于需要修改caps相关能力，需要特权模式运行：

```console
sudo ./a.out
```

**程序运行后，可以观察到以下结果：**
- 权限清理后主线程6755已经不能访问目标文件
- 子线程6756因为是在清理权限前创建的，仍然保留了CAP_DAC_OVERRIDE权限，所以可以继续访问目标文件
- 子线程6757因为是在清理权限后创建的，不再具有CAP_DAC_OVERRIDE权限，所以无法访问目标文件


```console
main:235 pid:6755 tid:6755 Fail to open file. error:Permission denied
test_in_child_thread:182 pid:6755 tid:6756 Success to open file.
test_in_child_thread:182 pid:6755 tid:6757 Fail to open file. error:Permission denied
```

