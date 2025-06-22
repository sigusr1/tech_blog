---
title: "浅析elf中的.bss和.data"
date: 2024-06-18
categories: [操作系统]
tags:  [elf]  
---

elf文件中存在`.data`和`.bss`两个section，前者用来存储已经初始化的全局/静态变量，后者用来存储未初始化的全局/静态变量。所有的全局/静态变量都放在`.data`中不行吗？为何又引入了`.bss`呢？  

接下来我们通过几个例子一探究竟。

## 1. 编译期行为


基准测试程序：

```c
#include <stdio.h>
#include <stdint.h>

int32_t main() {
    return 0;
}
```

编译并strip可执行程序：
```
gcc test.c
strip a.out
ls -l a.out   // 输出为 14328 字节
```

然后执行`readelf -t a.out`查看data和bss部分的输出：


```
[Nr] Name
    Type              Address          Offset            Link
    Size              EntSize          Info              Align
    Flags

[23] .data
    PROGBITS         0000000000004000  0000000000003000  0
    0000000000000010 0000000000000000  0                 8
    [0000000000000003]: WRITE, ALLOC
[24] .bss
    NOBITS           0000000000004010  0000000000003010  0
    0000000000000008 0000000000000000  0                 1
    [0000000000000003]: WRITE, ALLOC
```

> 尽管我们没有定义全局/静态变量，data和bss却不为空，这是因为链接系统库引入的，参考[ambiguous-behaviour-of-bss-segment-in-c-program](https://stackoverflow.com/questions/40678999/ambiguous-behaviour-of-bss-segment-in-c-program)。
{: .prompt-info }

执行`readelf -x .data  a.out`可以查看data的内容(后面对比使用):
```
Hex dump of section '.data':
  0x00004000 00000000 00000000 08400000 00000000 .........@......
```

### 1.1 data section

在上述程序基础上添加一个初始化的整型数组，占用空间1024字节。

```c
#include <stdio.h>
#include <stdint.h>

int32_t a[256] = {1, 2, 3}; // 1024字节

int32_t main() {
    return 0;
}
```

编译并strip可执行程序：
```
gcc test.c
strip a.out
ls -l a.out   // 输出为 15368 字节
```

我们关注两个点：

- 和基准程序相比，可执行文件增加了1040字节（15368 - 14328 = 1040），而不是 1024
- 这1040字节都在data section吗

执行`readelf -t a.out`查看data部分的输出：

```
[Nr] Name
    Type              Address          Offset            Link
    Size              EntSize          Info              Align
    Flags

[23] .data
    PROGBITS         0000000000004000  0000000000003000  0
    0000000000000420 0000000000000000  0                 32
    [0000000000000003]: WRITE, ALLOC
```

可以看到data大小是0x420，即1056，比基准程序的0x10多了1040字节。  
**这说明新增加的数组a确实位于data section。**

但是数组a只有1024字节，却占用了1040的空间，为什么？  
我们再通过`readelf -x .data  a.out`看下data的内存布局（只截取了一部分）:
```
Hex dump of section '.data':
  0x00004000 00000000 00000000 08400000 00000000 .........@......
  0x00004010 00000000 00000000 00000000 00000000 ................
  0x00004020 01000000 02000000 03000000 00000000 ................
  0x00004030 00000000 00000000 00000000 00000000 ................
```

可以看到，新增加的数组a的起始地址是0x00004020，和基准程序中的data内容留了16字节的填充区，这是因为**新程序中data是以32字节对齐的**。

> 总结下，初始化的全局变量确实是放在data section，并且**会影响可执行程序(elf文件)的大小**。
{: .prompt-info }

静态变量的情况类似，不再赘述。

另外，data section的对齐方式、内存布局（变量在内存中的顺序不一定按定义顺序排列）和编译优化选项有关，感兴趣的同学可以多添加几个变量，用O2、Os做下实验。

### 1.2 bss section

同样在基准程序的基础上添加一个整型数组，占用空间1024字节，不同的是该数组未初始化。

```c
#include <stdio.h>
#include <stdint.h>

int32_t a[256];  // 1024字节

int32_t main() {
    return 0;
}
```

编译并strip可执行程序：
```
gcc test.c
strip a.out
ls -l a.out   // 输出为 14328 字节
```

可以看到，可执行程序的大小仍然是14328字节，和基准程序一样大。  
难道数组a没被放到可执行文件吗？  
执行`readelf -t a.out`查看bss部分的输出：

```
[Nr] Name
    Type              Address          Offset            Link
    Size              EntSize          Info              Align
    Flags

[24] .bss
    NOBITS           0000000000004020  0000000000003010  0
    0000000000000420 0000000000000000  0                 32
    [0000000000000003]: WRITE, ALLOC
```

可以看到，bss的大小已经变成0x420字节了，即1056字节，包含了占用空间1024字节的数组a，多出来的32字节和data是一样的，内存对齐导致的。  
也就是说，elf中只记录了bss占用空间的大小，并未给它赋值（确实也没必要赋值，因为现在也不知道该赋何值），这样做带来的好处是，elf文件的大小不随bss大小变化，节约了存储空间。

> 总结下，未初始化的全局变量确实是放在bss section，并且**不会影响可执行程序(elf文件)的大小**。
{: .prompt-info }


问题自然而然的来了，既然编译期对data和bss处理方式不同，那运行期又是如何处理的呢？

## 2. 运行期行为

示例程序如下：数组a初始化了，数组b未初始化。

```c
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

char a[1024] = {'x'};
char b[1024];

int32_t main() {
    printf("value of a[0]:%d start address of a:%p\n", a[0], a);
    printf("value of b[0]:%d start address of b:%p\n", b[0], b);

    pause();

    return 0;
}
```

### 2.1 静态分析

编译可执行程序并通过size命令查看bss和data大小(为了在运行期查看符号，这次没有strip)：
```
gcc test.c
size a.out

   text	   data	    bss	    dec	    hex	filename
   1791	   1648	   1056	   4495	   118f	a.out
   
```

执行`readelf -l a.out`可以看到（仅截取了一部分）：
- `.data`和`.bss`在运行期都位于编号为05的Segment（类型为LOAD），也就是运行期的**可读写数据区**
- 编号为05的Segment虚拟内存总大小（MemSiz）为0x0000000000000a90（2704字节），FileSiz为0x0000000000000670（1648字节）
  - FileSiz对应data Section，在加载的时候直接映射（mmap）到进程的内存空间
  - MemSiz - FileSiz = 1056字节，对应bss Section，因为这部分在elf文件中只有大小信息，没有实际内容，所以没法像data Section那样进行映射，只能为其分配内存，然后将这块内存区域初始化为0（这可以解释为啥未初始化的全局/静态变量默认值为0）

```
Program Headers:
  Type           Offset             VirtAddr           PhysAddr
                 FileSiz            MemSiz              Flags  Align

  (下面这个Segment的index是05)
  LOAD           0x0000000000002db0 0x0000000000003db0 0x0000000000003db0
                 0x0000000000000670 0x0000000000000a90  RW     0x1000

 Section to Segment mapping:
  Segment Sections...
   03     .init .plt .plt.got .plt.sec .text .fini
   04     .rodata .eh_frame_hdr .eh_frame
   05     .init_array .fini_array .dynamic .got .data .bss
```

> elf有两个视图，上图可以看出不同视图各区段的映射关系
- Section View：主要是编译期链接使用
- Segment View：主要是运行期加载使用
{: .prompt-info }

### 2.2 动态分析

接下来我们把这个程序用gdb跑起来看下。

执行`gdb a.out`输入 `r` 可以看到数组a和b的地址：

```
value of a[0]:120 start address of a:0x555555558020
value of b[0]:0 start address of b:0x555555558440
```

接下来执行`ctrl + c`打断程序执行（并未退出），通过 `info symbol`可以看到数组a确实位于.data section，数组b确实位于.bss section。
```
(gdb) info symbol 0x555555558020
a in section .data of /home/hjy284533/workspace/hjy/docs/tech_blog/_posts/a.out
(gdb) info symbol 0x555555558440
b in section .bss of /home/hjy284533/workspace/hjy/docs/tech_blog/_posts/a.out
```

接下来执行`maintenance info sections`(仅截取部分)，可以看到：
- 运行期.data和.bss的内存地址是紧挨着的（实际上位于同一个Segment，即可读写的数据区）
- .data是HAS_CONTENTS的(即在elf文件中有内容)，LOAD说明在程序加载时会被拷贝到内存
- .bss则没有上述flag，因为它没有内容，所以只需要分配虚拟内存就行了（ALLOC flag），这也为运行期的优化埋下了伏笔（写时分配：如果只是读取，是不是都不用分配实际的物理内存，只有当需要写入时再分配物理内存）

```
 [24]     0x555555558000->0x555555558420 at 0x00003000: .data ALLOC LOAD DATA HAS_CONTENTS
 [25]     0x555555558420->0x555555558840 at 0x00003420: .bss ALLOC
```

> 啰嗦这么多，只为了解释bss和data分开的原因：
- elf文件占用的存储空间变小了
- 运行期优化：写时分配，仅在写入的时候才分配物理内存
{: .prompt-info }
