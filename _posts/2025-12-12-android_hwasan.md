---
title: "Android HWASan简介"
date: 2025-12-12
categories: [Android]
tags:  [Android, HWASan]
---


近期发现一个现象，某些在Linux平台能正常运行的程序，移植到Android上会crash，分析这些crash后发现还真是程序自身的问题。
看来不是Android没事找事，而是Linux纵容了这些潜在的Bug。  
先看个例子，示例程序中有个明显的踩内存操作：

```c++
#include <stdio.h>
#include <stdlib.h>

int main() {
    char *p = new char[16];
    printf("p:%p\n", p);
    // p合法的下标范围是[0,15]，p[16]踩内存了
    p[16] = 'a';
    return 0;
}
```

*注：本文所述硬件平台均为高通8155。*

在Linux系统运行上述程序，输出如下，无任何异常:
```
p:0x558dae2830
```

在Android14系统运行上述程序，crash了，并且输出了详细的debug信息，告诉我们尝试往地址`0x00458e3d2790`(即`p[16]`)非法写一个字节，精确指出程序的问题：
```
p:0x650000458e3d2780
==26586==ERROR: HWAddressSanitizer: tag-mismatch on address 0x00458e3d2790 at pc 0x00648e3f0120
WRITE of size 1 at 0x00458e3d2790 tags: 65/16 (ptr/mem) in thread T0
// 省略若干dump信息
```

Android是怎么做到的呢？上述crash信息中，注意两个关键字HWAddressSanitizer、tag-mismatch。没错，是HWASan检测到非法内存访问后终止了进程。


## 1. HWASan是什么
[HWASan(Hardware-assisted AddressSanitizer)](https://source.android.com/docs/security/test/hwasan)是一种内存错误检测工具，它利用[momory tagging](https://arxiv.org/pdf/1802.09517)技术，相比于传统的AddressSanitizer(ASan)，显著降低了内存开销。

HWASan和传统ASan一样，都能检测下面的内存错误：
- 堆栈溢出（Stack and heap buffer overflow/underflow）
- 使用释放后的堆内存（Heap use after free）
- 访问失效的栈内存（Stack use outside scope）
- 内存重复释放/野指针(Double free/wild free)

相比于传统ASan，HWASan的性能开销有所改善：
- CPU消耗类似(大约是正常程序的2倍)
- 可执行程序大小类似 (比正常程序增加40 – 50%)
- **相比传统ASan，内存消耗降低10% – 35%**


不过，HWASan对运行环境要求比较严苛：
- 硬件要求：AArch64架构，即arm64，它提供了一种叫做Top-byte Ignore (TBI)的技术，内存寻址的时候忽略指针的最高字节；这一个字节，可以用来存储元数据，[momory tagging](https://arxiv.org/pdf/1802.09517)就是利用这一个字节，为内存打了标签。
- 软件要求：
    - TBI需要kernel支持，内存寻址的时候忽略最高字节.
    - 需要编译器支持（目前仅Clang支持）：对内存操作进行插桩，比如分配内存的时候打标签，使用内存的时候检查标签是否匹配

 > Android 11开始，开发者可以使用HWASan，如何开启请参考：https://source.android.com/docs/security/test/hwasan#using-hwasan
{: .prompt-info }

## 2. HWASan工作原理
HWASan的技术基础是[momory tagging](https://arxiv.org/pdf/1802.09517):
- 每16字节分配一个随机Tag，Tag大小是1字节
- 申请内存时，内存管理模块分配内存、TAG(独立存储)，并建立二者的映射关系，并将返回指针的最高字节设为对应内存块的TAG
- 访问内存的时候，检查指针中的TAG与目标内存的TAG是否一致，不一致就终止程序

下面结合两个具体的例子看下HWASan的工作原理。
### 2.1 heap overflow的例子

下图是个内存越界的例子：
- 虽然只申请了20字节，但HWASan会以16字节对齐，所以实际上分配了32字节
- 指针p的值为`0x650000458e3d2780`，最高字节即TAG是0x65，访问内存的时候，用来和内存块实际的TAG做对比
- 代码`p[32] = 'a'`中，指针p的TAG是0x65，访问的内存块的TAG是0x23，不匹配，HWASan终止程序
- 思考下：`p[25] = 'a'`会导致程序崩溃吗？
![HWASan检测内存越界的例子](/assets/images/2025-12-12-android_hwasan/hwasan-heap_overflow.jpg)

### 2.2 use after free的例子

下图是个`use after free`的例子，内存释放后TAG会更新，再次使用会导致TAG不匹配，进而crash。
![HWASan检测use after free的例子](/assets/images/2025-12-12-android_hwasan/hwasan-use_after_free.jpg)


HWASan还可以监测栈上的非法内存使用，比如栈溢出、使用已失效的局部变量，原理是一样的，都是基于`momory tagging`机制。

### 2.3 局限性

前面提到TAG大小是1字节，能表示的范围是[0, 255]，也就是说总共有256个不同的TAG。  
如果当前进程申请未释放的内存块多于256，必然会出现不同内存块TAG相同的情况。  
如果恰好非法访问了TAG相同的内存块，HWASan是检测不出来的。  
这种漏检的概率是大约是0.4%，即1/256。

实际工程中非法内存访问，一般出现在临近区域，所以，HWASan在TAG随机化基础上，会保证相邻内存区域的TAG不同。不过，如果你写Bug的能力特别强，翻山越岭踩了别人的内存，是有可能逃过HWASan法眼的，比如下面的例子：
```c
#include <stdio.h>

#define TAG_MASK ((long)0xff << 56)
#define GET_TAG(pointer) (((long)pointer & TAG_MASK) >> 56)
#define CLEAR_TAG(pointer) ((long)pointer & ~TAG_MASK)

#define MALLOC_SIZE (32)

int main() {
    char* p1 = new char[MALLOC_SIZE];
    char p1Tag = GET_TAG(p1);
    printf("p1:%p p1Tag:0x%x\n", p1, p1Tag);

    for (;;) {
        char* p2 = new char[MALLOC_SIZE];
        char p2Tag = GET_TAG(p2);
        if (p2Tag != p1Tag) {
            // 如果新分配内存p2的tag和p1的不相等，就故意泄露，消耗TAG
            continue;
        }

        long distance = CLEAR_TAG(p2) - CLEAR_TAG(p1);
        if (distance > 0) {
            printf("p2:%p p2Tag:0x%x\n", p2, p2Tag);
            printf("distance between p1 and p2 is %ld\n", distance);
            printf("write one byte to %p, which is between p2's range [%p, %p)\n", p1 + distance,
                    p2, p2 + MALLOC_SIZE);
            p1[distance] = 'a';
            break;
        }
    }

    return 0;
}
```

运行结果如下，指针p1和p2的TAG相同，p1翻山越岭踩了p2的内存，HWASan没检测出来：
```
p1:0xb3000041e5032780 p1Tag:0xb3
p2:0xb3000041e50335a0 p2Tag:0xb3
distance between p1 and p2 is 3616
write one byte to 0xb3000041e50335a0, which is between p2's range [0xb3000041e50335a0, 0xb3000041e50335c0)
```
上述例子示意图如下所示：
![HWASan漏检示意图](/assets/images/2025-12-12-android_hwasan/hwasan-not_detect_problem.jpg)

## 3. 题外话
### 3.1 HWASan捕获的是第一现场
不知道大家有没有遇到过这样的情况，coredump文件拿到了，仍然分析不出问题原因。  
先看个踩内存的例子，指针p申请了8字节的内存，却将自己前后各512字节的内存都写成了0：

```c++
#include <stdio.h>

int main() {
    char* p = new char[8];
    for (int i = 0; i < 1024; i++) {
        *(p - 512 + i) = 0x0;
    }

    delete[] p;
    return 0;
}
```

Linux下gdb运行，crash在`delete[] p`这一行，实际上`*(p - 512 + i) = 0x0`才是真正的罪魁祸首。  
也就是说，有些情况下踩内存的coredump，并不是第一现场。  
上述demo很简单，可以一眼看出问题所在，但实际项目中，如果是跨模块/跨线程的踩内存，就很难排查了。
![coredump堆栈不准确示意图](/assets/images/2025-12-12-android_hwasan/wrong_pos_of_coredump.jpg)

作为对比，HWASan就可以准确的抓到第一现场信息，提高了问题排查效率：
![HWASan第一现场示意图](/assets/images/2025-12-12-android_hwasan/write_pos_of_hwasan.jpg)

*Tips: Android平台编译上述demo时，需要加-O0编译选项，否则会被Clang优化掉。*

### 3.2 Arm Memory Tagging Extension (MTE)

[Arm MTE](https://developer.android.com/ndk/guides/arm-mte)的功能、原理和HWASan基本一样，相当于把HWASan的部分实现硬件化了，所以性能更好，[有资料](https://www.usenix.org/system/files/login/articles/login_summer19_03_serebryany.pdf)提到CPU overhead大概只有百分之几，内存overhead 3~5%，完全可以在生产环境开启。
Android 13开始支持该功能，不过好像现在支持的硬件还不多。

## 4. 参考资料
- https://source.android.com/docs/security/test/hwasan
- https://arxiv.org/pdf/1802.09517
- https://newsroom.arm.com/blog/memory-safety-arm-memory-tagging-extension
- https://developer.android.com/ndk/guides/arm-mte
- https://www.usenix.org/system/files/login/articles/login_summer19_03_serebryany.pdf
- https://thore.io/posts/2025/09/introduction-to-arm-memory-tagging-extensions/