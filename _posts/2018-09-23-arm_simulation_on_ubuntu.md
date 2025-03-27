---
title: "Ubuntu上搭建arm虚拟运行环境"  
date: 2018-09-23
categories: [工具]
tags: [arm, 虚拟机]
---

* content
{:toc}  

没有开发板，如何调试运行arm程序？  
本文主要讲解如何在Ubuntu上搭建arm交叉编译、运行环境。


## 一、安装交叉编译工具链 ##

安装交叉编译工具链arm-linux-gnueabihf-gcc：  

```console
sudo apt-get install gcc-arm-linux-gnueabihf
```

安装完毕，可以看到系统上已经新增了这么多交叉编译工具：  

```console
helloworld@ubuntu:~$arm-linux-gnueabihf-
arm-linux-gnueabihf-addr2line     arm-linux-gnueabihf-gcov-7
arm-linux-gnueabihf-ar            arm-linux-gnueabihf-gcov-dump
arm-linux-gnueabihf-as            arm-linux-gnueabihf-gcov-dump-7
arm-linux-gnueabihf-c++filt       arm-linux-gnueabihf-gcov-tool
arm-linux-gnueabihf-cpp           arm-linux-gnueabihf-gcov-tool-7
arm-linux-gnueabihf-cpp-7         arm-linux-gnueabihf-gprof
arm-linux-gnueabihf-dwp           arm-linux-gnueabihf-ld
arm-linux-gnueabihf-elfedit       arm-linux-gnueabihf-ld.bfd
arm-linux-gnueabihf-gcc           arm-linux-gnueabihf-ld.gold
arm-linux-gnueabihf-gcc-7         arm-linux-gnueabihf-nm
arm-linux-gnueabihf-gcc-ar        arm-linux-gnueabihf-objcopy
arm-linux-gnueabihf-gcc-ar-7      arm-linux-gnueabihf-objdump
arm-linux-gnueabihf-gcc-nm        arm-linux-gnueabihf-ranlib
arm-linux-gnueabihf-gcc-nm-7      arm-linux-gnueabihf-readelf
arm-linux-gnueabihf-gcc-ranlib    arm-linux-gnueabihf-size
arm-linux-gnueabihf-gcc-ranlib-7  arm-linux-gnueabihf-strings
arm-linux-gnueabihf-gcov          arm-linux-gnueabihf-strip
```

执行下面的代码建立软链接，否则后面执行的时候会报动态库找不到的错误：  

```console
sudo ln -s /usr/arm-linux-gnueabihf/lib/libc.so.6 /lib/libc.so.6
sudo ln -s /usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 /lib/ld-linux-armhf.so.3
```

编写测试代码`main.c`：  

```c
#include <stdio.h>	
int main()
{
    printf("helloworld\n");
    return 0;
}
```

下面的命令，首先编译`main.c`生成arm平台下的可执行文件`a.out`，然后通过`file`命令可以看到，`a.out`为arm平台下的elf可执行文件：

```console
arm-linux-gnueabihf-gcc main.c 
file a.out
```

```console
a.out: ELF 32-bit LSB shared object, ARM, EABI5 version 1 (SYSV), dynamically linked, interpreter /lib/ld-, for GNU/Linux 3.2.0, BuildID[sha1]=7592a0494955ca8bb953948ea4cfbefc90b2e2e9, not stripped
```


## 二、安装arm模拟器qemu ##

安装arm模拟器qemu ：  

```console
sudo apt-get install qemu
```

执行arm平台的可执行文件`a.out`。可以看到，程序输出了正确的结果`helloworld`：  

```console
qemu-arm a.out 
```

*说明：qemu可以模拟很多平台，不限于arm。*  

## 三、通过gdb调试arm程序 ##

在Ubuntu上用gdb调试arm程序的原理：qemu端作为gdb server启动可执行程序，另一端作为gdb client连接gdb server，进行本地远程调试。  
1. 首先安装多平台的gdb工具： 
 
	```console
	sudo apt-get install gdb-multiarch
	```
2. 重新编译示例代码`main.c`，注意，这次加上了参数`--static`。加上这个参数后，生成的可执行文件为静态链接的。**如果不加这个参数，gdb调试的时候单步执行功能不正常，符号表也找不到。**  
	```console
	arm-linux-gnueabihf-gcc --static -g main.c 
	```
3. 通过下面的命令启动可执行程序`a.out`, 选项`-g`指明了gdb的监听端口，这里选择的是1234。该指令运行后，当前窗口会被阻塞住。  
	```console
	qemu-arm -g 1234 a.out
	```
4. 新开一个命令行窗口，启动gdb client,进入gdb交互界面:
	```console
	gdb-multiarch a.out
	```
5. 在gdb交互界面输入以下内容就可以连接到server端:
	```console
	target remote localhost:1234
	```
6. 接下来，就可以正常使用gdb的相关功能调试程序了:  
	```console
	(gdb) b main
	Breakpoint 1 at 0x102e8: file main.c, line 5.
	(gdb) c
	Continuing.
	
	Breakpoint 1, main () at main.c:5
	5	    printf("helloworld\n");
	```

## 四、参考文档 ##
1. [Linux下ARM程序的编译运行及调试](https://www.jianshu.com/p/dc8e263d6466)
2. [qemu相关说明文档](https://people.debian.org/~aurel32/qemu/armhf/README.txt)
3. [CREATE DEBUG ENVIRONMENT FOR ARM ARCHITECTURE ON INTEL PROCESSOR](https://hydrasky.com/linux/create-debug-environment-for-arm-architecture-on-intel-processor/)
