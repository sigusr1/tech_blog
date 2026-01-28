---
title:  "虚拟内存探究 -- 第四篇:malloc, heap & the program break"  
date:   2017-10-18
categories: [操作系统]
tags: [虚拟内存, 翻译]
---


* content
{:toc}
这是虚拟内存系列文章的第四篇。  
本文主要介绍malloc和heap相关知识，以便回答[上一篇文章](https://tech.coderhuo.tech/posts/Virtual_Memory_drawing_VM_diagram/)结尾提出的一些问题:

- 动态分配的内存为何不是从堆的起始位置`0x2050000`开始，而是偏移16个字节从`0x2050010`开始？这16个字节是什么用途?
- 堆真的是向上生长的吗?









## 一、预备知识 ##

为了方便理解本文，你需要具备以下知识：  

- C语言基础(特别是指针相关知识)
- 了解Linux的文件系统和shell命令
- 了解文件`/proc/[pid]/maps`（可参阅`man proc`或[《虚拟内存探究 -- 第一篇:C strings & /proc》](https://tech.coderhuo.tech/posts/Virtual_Memory_C_strings_proc/)中的相关介绍）

## 二、实验环境 ##
所有的脚本和程序都在下面的环境中测试过：  

- Ubuntu 
	- Linux ubuntu 4.4.0-31-generic #50~14.04.1-Ubuntu SMP Wed Jul 13 01:07:32 UTC 2016 x86_64 x86_64 x86_64 GNU/Linux
- gcc  
	- gcc (Ubuntu 4.8.4-2ubuntu1~14.04.3) 4.8.4
- glibc 2.19   
- strace (version 4.8)  


**下面提到的都是基于本系统的，其他系统可能会有差异。**  

我们会查看部分Linux内核源码。如果你使用的是Ubuntu系统，可以通过下面的命令下载对应版本的内核源码：  

```
apt-get source linux-image-$(uname -r)
```

## 三、malloc ##

`malloc`是动态分配内存常用函数，它分配的内存在堆上。

*提示:`malloc`不是系统函数。*

从`man malloc`的输出可以看到：

```
[...] allocate dynamic memory[...]
void *malloc(size_t size);
[...]
The malloc() function allocates size bytes and returns a pointer to the allocated memory.
```

### 3.1 没有malloc就没有堆 ###

我们观察一个没有调用`malloc`的进程的内存分布情况:

```c
#include <stdlib.h>
#include <stdio.h>

/**
 * main - do nothing
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    getchar();
    return (EXIT_SUCCESS);
}
```
编译运行：

```shell
julien@holberton:~/holberton/w/hackthevm3$ gcc -Wall -Wextra -pedantic -Werror 0-main.c -o 0
julien@holberton:~/holberton/w/hackthevm3$ ./0 
```

*提示：进程的内存分布情况在文件`/proc/[pid]/maps`文件中。为了查看该文件， 我们首先要知道进程的PID。可以通过命令`ps`查看进程ID。`ps aux`输出的第二列就是进程的PID，具体可参阅[《虚拟内存探究 -- 第一篇:C strings & /proc》](https://tech.coderhuo.tech/posts/Virtual_Memory_C_strings_proc/)中的相关介绍。*

```
julien@holberton:/tmp$ ps aux | grep \ \./0$
julien     3638  0.0  0.0   4200   648 pts/9    S+   12:01   0:00 ./0
```
*提示：从上面的输出可以看到我们要找的进程的id是`3638`，因此，文件`maps`位于文件夹`/proc/3638下面。*

```
julien@holberton:/tmp$ cd /proc/3638
```

*提示：文件`maps`包含进程的内存分布图，每一行的格式如下：
`address perms offset dev inode pathname`*

```
julien@holberton:/proc/3638$ cat maps
00400000-00401000 r-xp 00000000 08:01 174583                             /home/julien/holberton/w/hack_the_virtual_memory/03. The Heap/0
00600000-00601000 r--p 00000000 08:01 174583                             /home/julien/holberton/w/hack_the_virtual_memory/03. The Heap/0
00601000-00602000 rw-p 00001000 08:01 174583                             /home/julien/holberton/w/hack_the_virtual_memory/03. The Heap/0
7f38f87d7000-7f38f8991000 r-xp 00000000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f38f8991000-7f38f8b91000 ---p 001ba000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f38f8b91000-7f38f8b95000 r--p 001ba000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f38f8b95000-7f38f8b97000 rw-p 001be000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f38f8b97000-7f38f8b9c000 rw-p 00000000 00:00 0 
7f38f8b9c000-7f38f8bbf000 r-xp 00000000 08:01 136229                     /lib/x86_64-linux-gnu/ld-2.19.so
7f38f8da3000-7f38f8da6000 rw-p 00000000 00:00 0 
7f38f8dbb000-7f38f8dbe000 rw-p 00000000 00:00 0 
7f38f8dbe000-7f38f8dbf000 r--p 00022000 08:01 136229                     /lib/x86_64-linux-gnu/ld-2.19.so
7f38f8dbf000-7f38f8dc0000 rw-p 00023000 08:01 136229                     /lib/x86_64-linux-gnu/ld-2.19.so
7f38f8dc0000-7f38f8dc1000 rw-p 00000000 00:00 0 
7ffdd85c5000-7ffdd85e6000 rw-p 00000000 00:00 0                          [stack]
7ffdd85f2000-7ffdd85f4000 r--p 00000000 00:00 0                          [vvar]
7ffdd85f4000-7ffdd85f6000 r-xp 00000000 00:00 0                          [vdso]
ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]
julien@holberton:/proc/3638$ 
```

*注意：`hackthevm3` 是 `hack_the_virtual_memory/03. The Heap/`的符号链接，大家不要被误导了。*

从上面的`maps`文件可以看出，没有堆([heap])段。

### 3.2 malloc(x) ###

我们看看另一个调用`malloc`的程序(`1-main.c`):

```c
#include <stdio.h>
#include <stdlib.h>

/**
 * main - 1 call to malloc
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    malloc(1);
    getchar();
    return (EXIT_SUCCESS);
}
```

编译运行:

```
julien@holberton:~/holberton/w/hackthevm3$ gcc -Wall -Wextra -pedantic -Werror 1-main.c -o 1
julien@holberton:~/holberton/w/hackthevm3$ ./1 
```
查看`maps`文件:

```
julien@holberton:/proc/3638$ ps aux | grep \ \./1$
julien     3718  0.0  0.0   4332   660 pts/9    S+   12:09   0:00 ./1
julien@holberton:/proc/3638$ cd /proc/3718
julien@holberton:/proc/3718$ cat maps 
00400000-00401000 r-xp 00000000 08:01 176964                             /home/julien/holberton/w/hack_the_virtual_memory/03. The Heap/1
00600000-00601000 r--p 00000000 08:01 176964                             /home/julien/holberton/w/hack_the_virtual_memory/03. The Heap/1
00601000-00602000 rw-p 00001000 08:01 176964                             /home/julien/holberton/w/hack_the_virtual_memory/03. The Heap/1
01195000-011b6000 rw-p 00000000 00:00 0                                  [heap]
...
julien@holberton:/proc/3718$ 
```
可以看到, 这次有堆段。

我们检查下`malloc`的返回值, 看看该地址是不是真的在堆上(`2-main.c`)：

```c
#include <stdio.h>
#include <stdlib.h>

/**
 * main - prints the malloc returned address
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    void *p;

    p = malloc(1);
    printf("%p\n", p);
    getchar();
    return (EXIT_SUCCESS);
}
```

编译运行:

```
julien@holberton:~/holberton/w/hackthevm3$ gcc -Wall -Wextra -pedantic -Werror 2-main.c -o 2
julien@holberton:~/holberton/w/hackthevm3$ ./2 
0x24d6010
```

查看`maps`文件:

```
julien@holberton:/proc/3718$ ps aux | grep \ \./2$
julien     3834  0.0  0.0   4336   676 pts/9    S+   12:48   0:00 ./2
julien@holberton:/proc/3718$ cd /proc/3834
julien@holberton:/proc/3834$ cat maps
00400000-00401000 r-xp 00000000 08:01 176966                             /home/julien/holberton/w/hack_the_virtual_memory/03. The Heap/2
00600000-00601000 r--p 00000000 08:01 176966                             /home/julien/holberton/w/hack_the_virtual_memory/03. The Heap/2
00601000-00602000 rw-p 00001000 08:01 176966                             /home/julien/holberton/w/hack_the_virtual_memory/03. The Heap/2
024d6000-024f7000 rw-p 00000000 00:00 0                                  [heap]
...
julien@holberton:/proc/3834$ 
```
可以看到: `024d6000` < `0x24d6010` < `024f7000`， 因此`malloc`返回的内存地址是在堆上的。

另外, 和[上一篇文章](https://tech.coderhuo.tech/posts/Virtual_Memory_drawing_VM_diagram/)中看到的一样，地址并未从堆的起始地址(`024d6000`)开始分配，而是偏移了`16`个字节，从`0x24d6010`开始。我们后面再讨论这个问题。

## 四、strace, brk 和 sbrk ##

`malloc`是个常规函数（相对于系统函数而言），为了操作堆，它必须调用其他的系统函数。我们使用`strace`来寻找这个被`malloc`调用的系统函数。

`strace`用来跟踪系统调用和信号。每个程序在执行`main`函数前都会调用一些系统函数。为了方便找到`malloc`使用的是哪个系统函数，我们在调用`malloc`的代码前后都加上了系统调用`write`(`3-main.c`):

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * main - let's find out which syscall malloc is using
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    void *p;

    write(1, "BEFORE MALLOC\n", 14);
    p = malloc(1);
    write(1, "AFTER MALLOC\n", 13);
    printf("%p\n", p);
    getchar();
    return (EXIT_SUCCESS);
}
```
编译运行：
```
julien@holberton:~/holberton/w/hackthevm3$ gcc -Wall -Wextra -pedantic -Werror 3-main.c -o 3
julien@holberton:~/holberton/w/hackthevm3$ strace ./3 
execve("./3", ["./3"], [/* 61 vars */]) = 0
...
write(1, "BEFORE MALLOC\n", 14BEFORE MALLOC
)         = 14
brk(0)                                  = 0xe70000
brk(0xe91000)                           = 0xe91000
write(1, "AFTER MALLOC\n", 13AFTER MALLOC
)          = 13
...
read(0, 
```
可以看出，`malloc`调用系统函数`brk`来操作堆。从`man brk`可以看到该系统函数的介绍：

```
...
       int brk(void *addr);
       void *sbrk(intptr_t increment);
...
DESCRIPTION
       brk() and sbrk() change the location of the program  break,  which  defines
       the end of the process's data segment (i.e., the program break is the first
       location after the end of the uninitialized data segment).  Increasing  the
       program  break has the effect of allocating memory to the process; decreas‐
       ing the break deallocates memory.

       brk() sets the end of the data segment to the value specified by addr, when
       that  value  is  reasonable,  the system has enough memory, and the process
       does not exceed its maximum data size (see setrlimit(2)).

       sbrk() increments the program's data space  by  increment  bytes.   Calling
       sbrk()  with  an increment of 0 can be used to find the current location of
       the program break.
```

`program break`是虚拟内存中数据段的结束位置， 如下图所示：

![](/assets/images/virtual_memory/program-break-before.png)

`malloc`通过调用`brk`或`sbrk`增加`program break`的值，从而创建可以通过`malloc`动态分配的内存空间。所以堆是进程的数据段的延伸。  
![](/assets/images/virtual_memory/program-break-after.png)

上面示例中首次调用`brk`(`brk(0)`)返回的是`program break`的当前地址。第二次调用则通过移动`program break`的位置增加了内存空间(从`0xe70000` 到 `0xe91000`)。上面的例子中，堆段起始于`0xe70000`， 结束于`0xe91000`。我们看下`/proc/[pid]/maps`文件,是否真的如此：

```
julien@holberton:/proc/3855$ ps aux | grep \ \./3$
julien     4011  0.0  0.0   4748   708 pts/9    S+   13:04   0:00 strace ./3
julien     4014  0.0  0.0   4336   644 pts/9    S+   13:04   0:00 ./3
julien@holberton:/proc/3855$ cd /proc/4014
julien@holberton:/proc/4014$ cat maps 
00400000-00401000 r-xp 00000000 08:01 176967                             /home/julien/holberton/w/hack_the_virtual_memory/03. The Heap/3
00600000-00601000 r--p 00000000 08:01 176967                             /home/julien/holberton/w/hack_the_virtual_memory/03. The Heap/3
00601000-00602000 rw-p 00001000 08:01 176967                             /home/julien/holberton/w/hack_the_virtual_memory/03. The Heap/3
00e70000-00e91000 rw-p 00000000 00:00 0                                  [heap]
...
julien@holberton:/proc/4014$ 
```

堆段如下，和`brk`的返回值一致：
```
00e70000-00e91000 rw-p 00000000 00:00 0 [heap]
```

但是代码中我们只需要1个字节，为什么`malloc`一下子把堆增加了这么多（`00e91000` – `00e70000` = `0x21000` ，也就是135168字节）？


### 4.1 多次malloc ###
如果我们多次调用`malloc`将会怎么样呢(`4-main.c`)？

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * main - many calls to malloc
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    void *p;

    write(1, "BEFORE MALLOC #0\n", 17);
    p = malloc(1024);
    write(1, "AFTER MALLOC #0\n", 16);
    printf("%p\n", p);

    write(1, "BEFORE MALLOC #1\n", 17);
    p = malloc(1024);
    write(1, "AFTER MALLOC #1\n", 16);
    printf("%p\n", p);

    write(1, "BEFORE MALLOC #2\n", 17);
    p = malloc(1024);
    write(1, "AFTER MALLOC #2\n", 16);
    printf("%p\n", p);

    write(1, "BEFORE MALLOC #3\n", 17);
    p = malloc(1024);
    write(1, "AFTER MALLOC #3\n", 16);
    printf("%p\n", p);

    getchar();
    return (EXIT_SUCCESS);
}
```
编译运行：
```
julien@holberton:~/holberton/w/hackthevm3$ gcc -Wall -Wextra -pedantic -Werror 4-main.c -o 4
julien@holberton:~/holberton/w/hackthevm3$ strace ./4 
execve("./4", ["./4"], [/* 61 vars */]) = 0
...
write(1, "BEFORE MALLOC #0\n", 17BEFORE MALLOC #0
)      = 17
brk(0)                                  = 0x1314000
brk(0x1335000)                          = 0x1335000
write(1, "AFTER MALLOC #0\n", 16AFTER MALLOC #0
)       = 16
...
write(1, "0x1314010\n", 100x1314010
)             = 10
write(1, "BEFORE MALLOC #1\n", 17BEFORE MALLOC #1
)      = 17
write(1, "AFTER MALLOC #1\n", 16AFTER MALLOC #1
)       = 16
write(1, "0x1314420\n", 100x1314420
)             = 10
write(1, "BEFORE MALLOC #2\n", 17BEFORE MALLOC #2
)      = 17
write(1, "AFTER MALLOC #2\n", 16AFTER MALLOC #2
)       = 16
write(1, "0x1314830\n", 100x1314830
)             = 10
write(1, "BEFORE MALLOC #3\n", 17BEFORE MALLOC #3
)      = 17
write(1, "AFTER MALLOC #3\n", 16AFTER MALLOC #3
)       = 16
write(1, "0x1314c40\n", 100x1314c40
)             = 10
...
read(0, 
```

从上面`strace`跟踪结果可以看出，`malloc`并没有每次都调用系统函数`brk`。

第一次调用`malloc`的时候，`malloc`通过改变`program break`的位置增加了堆的大小。接下来的调用，`malloc`仅仅在现有的堆上给我们分配内存，这些新分配的内存都是首次调用	`malloc`的时候通过`brk`分配的。这样的话，`malloc`就不必每次都调用系统函数`brk`，也就提升了`malloc`的执行速度, 进而提高了我们的应用程序的执行速度。另外，这也有助于函数`malloc`和`free`优化内存的使用。

我们通过`maps`文件确认下是不是只有一个堆(首次调用`brk`分配的)：
```
julien@holberton:/proc/4014$ ps aux | grep \ \./4$
julien     4169  0.0  0.0   4748   688 pts/9    S+   13:33   0:00 strace ./4
julien     4172  0.0  0.0   4336   656 pts/9    S+   13:33   0:00 ./4
julien@holberton:/proc/4014$ cd /proc/4172
julien@holberton:/proc/4172$ cat maps
00400000-00401000 r-xp 00000000 08:01 176973                             /home/julien/holberton/w/hack_the_virtual_memory/03. The Heap/4
00600000-00601000 r--p 00000000 08:01 176973                             /home/julien/holberton/w/hack_the_virtual_memory/03. The Heap/4
00601000-00602000 rw-p 00001000 08:01 176973                             /home/julien/holberton/w/hack_the_virtual_memory/03. The Heap/4
01314000-01335000 rw-p 00000000 00:00 0                                  [heap]
7f4a3f2c4000-7f4a3f47e000 r-xp 00000000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f4a3f47e000-7f4a3f67e000 ---p 001ba000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f4a3f67e000-7f4a3f682000 r--p 001ba000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f4a3f682000-7f4a3f684000 rw-p 001be000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f4a3f684000-7f4a3f689000 rw-p 00000000 00:00 0 
7f4a3f689000-7f4a3f6ac000 r-xp 00000000 08:01 136229                     /lib/x86_64-linux-gnu/ld-2.19.so
7f4a3f890000-7f4a3f893000 rw-p 00000000 00:00 0 
7f4a3f8a7000-7f4a3f8ab000 rw-p 00000000 00:00 0 
7f4a3f8ab000-7f4a3f8ac000 r--p 00022000 08:01 136229                     /lib/x86_64-linux-gnu/ld-2.19.so
7f4a3f8ac000-7f4a3f8ad000 rw-p 00023000 08:01 136229                     /lib/x86_64-linux-gnu/ld-2.19.so
7f4a3f8ad000-7f4a3f8ae000 rw-p 00000000 00:00 0 
7ffd1ba73000-7ffd1ba94000 rw-p 00000000 00:00 0                          [stack]
7ffd1bbed000-7ffd1bbef000 r--p 00000000 00:00 0                          [vvar]
7ffd1bbef000-7ffd1bbf1000 r-xp 00000000 00:00 0                          [vdso]
ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]
julien@holberton:/proc/4172$ 
```

从上面可以看出，我们只有一个堆，它的地址和`brk`的返回值(`0x1314000` & `0x1335000`)是一致的。

### 4.2 自行实现malloc ###
根据上面学到的知识，假设我们不需要释放内存，我们可以编写自己的`malloc`函数。如下代码所示，这个`malloc`每次都移动`program break`的位置：
```
#include <stdlib.h>
#include <unistd.h>

/**                                                                                            
 * malloc - naive version of malloc: dynamically allocates memory on the heap using sbrk                         
 * @size: number of bytes to allocate                                                          
 *                                                                                             
 * Return: the memory address newly allocated, or NULL on error                                
 *                                                                                             
 * Note: don't do this at home :)                                                              
 */
void *malloc(size_t size)
{
    void *previous_break;

    previous_break = sbrk(size);
    /* check for error */
    if (previous_break == (void *) -1)
    {
        /* on error malloc returns NULL */
        return (NULL);
    }
    return (previous_break);
}
```

## 五、丢失的16字节 ##

从上面程序(`4-main.c`)的输出可以看到，首次调用`malloc`返回的地址（`0x1314010`）并不是堆的起始地址(`0x1314000`)，而是在相对于堆的起始地址偏移了16个字节。另外，当我们第二次调用`malloc(1024)`的时候，按说返回地址应该是`1314410`(首次调用malloc的返回地址`0x1314010` + `1024`字节。译者注:作者这里写成了`0x1318010`, 有笔误)，但实际上返回的是`0x1318010`，我们又丢失了16字节！而且接下来每次调用`malloc`都会丢失16字节。

我们看看这丢失的16个字节里面是什么(`5-main.c`) ：

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**                                                                                            
 * pmem - print mem                                                                            
 * @p: memory address to start printing from                                                   
 * @bytes: number of bytes to print                                                            
 *                                                                                             
 * Return: nothing                                                                             
 */
void pmem(void *p, unsigned int bytes)
{
    unsigned char *ptr;
    unsigned int i;

    ptr = (unsigned char *)p;
    for (i = 0; i < bytes; i++)
    {
        if (i != 0)
        {
            printf(" ");
        }
        printf("%02x", *(ptr + i));
    }
    printf("\n");
}

/**
 * main - the 0x10 lost bytes
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    void *p;
    int i;

    for (i = 0; i < 10; i++)
    {
        p = malloc(1024 * (i + 1));
        printf("%p\n", p);
        printf("bytes at %p:\n", (void *)((char *)p - 0x10));
        pmem((char *)p - 0x10, 0x10);
    }
    return (EXIT_SUCCESS);
}
```

编译运行：

```
julien@holberton:~/holberton/w/hackthevm3$ gcc -Wall -Wextra -pedantic -Werror 5-main.c -o 5
julien@holberton:~/holberton/w/hackthevm3$ ./5
0x1fa8010
bytes at 0x1fa8000:
00 00 00 00 00 00 00 00 11 04 00 00 00 00 00 00
0x1fa8420
bytes at 0x1fa8410:
00 00 00 00 00 00 00 00 11 08 00 00 00 00 00 00
0x1fa8c30
bytes at 0x1fa8c20:
00 00 00 00 00 00 00 00 11 0c 00 00 00 00 00 00
0x1fa9840
bytes at 0x1fa9830:
00 00 00 00 00 00 00 00 11 10 00 00 00 00 00 00
0x1faa850
bytes at 0x1faa840:
00 00 00 00 00 00 00 00 11 14 00 00 00 00 00 00
0x1fabc60
bytes at 0x1fabc50:
00 00 00 00 00 00 00 00 11 18 00 00 00 00 00 00
0x1fad470
bytes at 0x1fad460:
00 00 00 00 00 00 00 00 11 1c 00 00 00 00 00 00
0x1faf080
bytes at 0x1faf070:
00 00 00 00 00 00 00 00 11 20 00 00 00 00 00 00
0x1fb1090
bytes at 0x1fb1080:
00 00 00 00 00 00 00 00 11 24 00 00 00 00 00 00
0x1fb34a0
bytes at 0x1fb3490:
00 00 00 00 00 00 00 00 11 28 00 00 00 00 00 00
julien@holberton:~/holberton/w/hackthevm3$ 
```
从上面的打印可以看出一个规律：请求分配的内存大小总是在前16个字节中出现。比如，第一次请求分配`1024`(`0x400`)个字节，前16个字节的后8个字节是`11 04 00 00 00 00 00 00`， 也就是数字(小端序)`0x 00 00 00 00 00 00 04 11` = `0x400` (1024) + `0x10`(丢失的16字节) + `1`(后面我们再讨论这1个字节)。  

可以看出，`malloc`返回地址的前面`16`个字节，都包含被分配内存块的大小，即:
```
用户请求分配内存的大小 + 0x10 + 1
```

至此我们大概可以猜测到这16个字节中存放的可能是`malloc`和`free`为了维护堆所使用的数据结构。尽管目前我们还不了解这个结构的细节，只要我们知道堆的起始地址，我们就可以利用这个结构遍历`malloc`分配的内存块（不调用`free`的前提下），如下代码所示（`6-main.c`）：

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**                                                                                            
 * pmem - print mem                                                                            
 * @p: memory address to start printing from                                                   
 * @bytes: number of bytes to print                                                            
 *                                                                                             
 * Return: nothing                                                                             
 */
void pmem(void *p, unsigned int bytes)
{
    unsigned char *ptr;
    unsigned int i;

    ptr = (unsigned char *)p;
    for (i = 0; i < bytes; i++)
    {
        if (i != 0)
        {
            printf(" ");
        }
        printf("%02x", *(ptr + i));
    }
    printf("\n");
}

/**
 * main - using the 0x10 bytes to jump to next malloc'ed chunks
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    void *p;
    int i;
    void *heap_start;
    size_t size_of_the_block;

    heap_start = sbrk(0);
    write(1, "START\n", 6);
    for (i = 0; i < 10; i++)
    {
        p = malloc(1024 * (i + 1)); 
        *((int *)p) = i;
        printf("%p: [%i]\n", p, i);
    }
    p = heap_start;
    for (i = 0; i < 10; i++)
    {
        pmem(p, 0x10);
        size_of_the_block = *((size_t *)((char *)p + 8)) - 1;
        printf("%p: [%i] - size = %lu\n",
              (void *)((char *)p + 0x10),
              *((int *)((char *)p + 0x10)),
              size_of_the_block);
        p = (void *)((char *)p + size_of_the_block);
    }
    write(1, "END\n", 4);
    return (EXIT_SUCCESS);
}
```

编译运行：

```
julien@holberton:~/holberton/w/hackthevm3$ gcc -Wall -Wextra -pedantic -Werror 6-main.c -o 6
julien@holberton:~/holberton/w/hackthevm3$ ./6 
START
0x9e6010: [0]
0x9e6420: [1]
0x9e6c30: [2]
0x9e7840: [3]
0x9e8850: [4]
0x9e9c60: [5]
0x9eb470: [6]
0x9ed080: [7]
0x9ef090: [8]
0x9f14a0: [9]
00 00 00 00 00 00 00 00 11 04 00 00 00 00 00 00
0x9e6010: [0] - size = 1040
00 00 00 00 00 00 00 00 11 08 00 00 00 00 00 00
0x9e6420: [1] - size = 2064
00 00 00 00 00 00 00 00 11 0c 00 00 00 00 00 00
0x9e6c30: [2] - size = 3088
00 00 00 00 00 00 00 00 11 10 00 00 00 00 00 00
0x9e7840: [3] - size = 4112
00 00 00 00 00 00 00 00 11 14 00 00 00 00 00 00
0x9e8850: [4] - size = 5136
00 00 00 00 00 00 00 00 11 18 00 00 00 00 00 00
0x9e9c60: [5] - size = 6160
00 00 00 00 00 00 00 00 11 1c 00 00 00 00 00 00
0x9eb470: [6] - size = 7184
00 00 00 00 00 00 00 00 11 20 00 00 00 00 00 00
0x9ed080: [7] - size = 8208
00 00 00 00 00 00 00 00 11 24 00 00 00 00 00 00
0x9ef090: [8] - size = 9232
00 00 00 00 00 00 00 00 11 28 00 00 00 00 00 00
0x9f14a0: [9] - size = 10256
END
julien@holberton:~/holberton/w/hackthevm3$ 
```
现在我们可以回答上一篇文章中的一个问题：`malloc`每分配一块内存，就在这块内存前面的16字节中存放该内存块的大小。内存被释放的时候，`free`函数会使用这个结构，并将这块内存放在空闲列表中，以待后续`malloc`的时候使用。

![](/assets/images/virtual_memory/0x10-malloc.png)

但是问题又来了：这16个字节的前8个字节是干嘛的呢？看起来它们总是0，难道是填充字节吗？

### 5.1 从源码找答案（RTFSC）###

我们看下`malloc`的源码（glibc中的`malloc.c`)：

```
1055 /*
1056       malloc_chunk details:
1057    
1058        (The following includes lightly edited explanations by Colin Plumb.)
1059    
1060        Chunks of memory are maintained using a `boundary tag' method as
1061        described in e.g., Knuth or Standish.  (See the paper by Paul
1062        Wilson ftp://ftp.cs.utexas.edu/pub/garbage/allocsrv.ps for a
1063        survey of such techniques.)  Sizes of free chunks are stored both
1064        in the front of each chunk and at the end.  This makes
1065        consolidating fragmented chunks into bigger chunks very fast.  The
1066        size fields also hold bits representing whether chunks are free or
1067        in use.
1068    
1069        An allocated chunk looks like this:
1070    
1071    
1072        chunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
1073                |             Size of previous chunk, if unallocated (P clear)  |
1074                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
1075                |             Size of chunk, in bytes                     |A|M|P|
1076          mem-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
1077                |             User data starts here...                          .
1078                .                                                               .
1079                .             (malloc_usable_size() bytes)                      .
1080                .                                                               |
1081    nextchunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
1082                |             (size of chunk, but used for application data)    |
1083                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
1084                |             Size of next chunk, in bytes                |A|0|1|
1085                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
1086    
1087        Where "chunk" is the front of the chunk for the purpose of most of
1088        the malloc code, but "mem" is the pointer that is returned to the
1089        user.  "Nextchunk" is the beginning of the next contiguous chunk.
```
我们猜对了。在`malloc`返回给用户地址的前面，有两个变量：

- 前一个内存块（如果该内内存块尚未被分配的话）的大小；例子中我们并未`free`内存，所以我们看到的这一区域总是0。
- 当前块的大小。

我们释放一些内存来看下前8个字节的变化（`7-main.c`）：

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**                                                                                            
 * pmem - print mem                                                                            
 * @p: memory address to start printing from                                                   
 * @bytes: number of bytes to print                                                            
 *                                                                                             
 * Return: nothing                                                                             
 */
void pmem(void *p, unsigned int bytes)
{
    unsigned char *ptr;
    unsigned int i;

    ptr = (unsigned char *)p;
    for (i = 0; i < bytes; i++)
    {
        if (i != 0)
        {
            printf(" ");
        }
        printf("%02x", *(ptr + i));
    }
    printf("\n");
}

/**
 * main - confirm the source code
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    void *p;
    int i;
    size_t size_of_the_chunk;
    size_t size_of_the_previous_chunk;
    void *chunks[10];

    for (i = 0; i < 10; i++)
    {
        p = malloc(1024 * (i + 1));
        chunks[i] = (void *)((char *)p - 0x10);
        printf("%p\n", p);
    }
    free((char *)(chunks[3]) + 0x10);
    free((char *)(chunks[7]) + 0x10);
    for (i = 0; i < 10; i++)
    {
        p = chunks[i];
        printf("chunks[%d]: ", i);
        pmem(p, 0x10);
        size_of_the_chunk = *((size_t *)((char *)p + 8)) - 1;
        size_of_the_previous_chunk = *((size_t *)((char *)p));
        printf("chunks[%d]: %p, size = %li, prev = %li\n",
              i, p, size_of_the_chunk, size_of_the_previous_chunk);
    }
    return (EXIT_SUCCESS);
}
```
编译运行：

```
julien@holberton:~/holberton/w/hackthevm3$ gcc -Wall -Wextra -pedantic -Werror 7-main.c -o 7
julien@holberton:~/holberton/w/hackthevm3$ ./7
0x1536010
0x1536420
0x1536c30
0x1537840
0x1538850
0x1539c60
0x153b470
0x153d080
0x153f090
0x15414a0
chunks[0]: 00 00 00 00 00 00 00 00 11 04 00 00 00 00 00 00
chunks[0]: 0x1536000, size = 1040, prev = 0
chunks[1]: 00 00 00 00 00 00 00 00 11 08 00 00 00 00 00 00
chunks[1]: 0x1536410, size = 2064, prev = 0
chunks[2]: 00 00 00 00 00 00 00 00 11 0c 00 00 00 00 00 00
chunks[2]: 0x1536c20, size = 3088, prev = 0
chunks[3]: 00 00 00 00 00 00 00 00 11 10 00 00 00 00 00 00
chunks[3]: 0x1537830, size = 4112, prev = 0
chunks[4]: 10 10 00 00 00 00 00 00 10 14 00 00 00 00 00 00
chunks[4]: 0x1538840, size = 5135, prev = 4112
chunks[5]: 00 00 00 00 00 00 00 00 11 18 00 00 00 00 00 00
chunks[5]: 0x1539c50, size = 6160, prev = 0
chunks[6]: 00 00 00 00 00 00 00 00 11 1c 00 00 00 00 00 00
chunks[6]: 0x153b460, size = 7184, prev = 0
chunks[7]: 00 00 00 00 00 00 00 00 11 20 00 00 00 00 00 00
chunks[7]: 0x153d070, size = 8208, prev = 0
chunks[8]: 10 20 00 00 00 00 00 00 10 24 00 00 00 00 00 00
chunks[8]: 0x153f080, size = 9231, prev = 8208
chunks[9]: 00 00 00 00 00 00 00 00 11 28 00 00 00 00 00 00
chunks[9]: 0x1541490, size = 10256, prev = 0
julien@holberton:~/holberton/w/hackthevm3$ 
```
从上面的打印可以看出，如果前面的内存块被释放了，`malloc`返回地址前面16个字节中的前8个字节代表的就是前一内存块的大小。所以，`malloc`分配的内存块的完整示意图如下：

![](/assets/images/virtual_memory/malloc-chunk.png)

另外，16个字节中后8字节（代表本内存块大小的8个字节）中的第一个比特位(源码中的标识位`P`)代表上一个内存块是否被占用（`1`-被占用，`0`-未占用）。所以更新后的实验程序如下 (`8-main.c`)：

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**                                                                                            
 * pmem - print mem                                                                            
 * @p: memory address to start printing from                                                   
 * @bytes: number of bytes to print                                                            
 *                                                                                             
 * Return: nothing                                                                             
 */
void pmem(void *p, unsigned int bytes)
{
    unsigned char *ptr;
    unsigned int i;

    ptr = (unsigned char *)p;
    for (i = 0; i < bytes; i++)
    {
        if (i != 0)
        {
            printf(" ");
        }
        printf("%02x", *(ptr + i));
    }
    printf("\n");
}

/**
 * main - updating with correct checks
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    void *p;
    int i;
    size_t size_of_the_chunk;
    size_t size_of_the_previous_chunk;
    void *chunks[10];
    char prev_used;

    for (i = 0; i < 10; i++)
    {
        p = malloc(1024 * (i + 1));
        chunks[i] = (void *)((char *)p - 0x10);
    }
    free((char *)(chunks[3]) + 0x10);
    free((char *)(chunks[7]) + 0x10);
    for (i = 0; i < 10; i++)
    {
        p = chunks[i];
        printf("chunks[%d]: ", i);
        pmem(p, 0x10);
        size_of_the_chunk = *((size_t *)((char *)p + 8));
        prev_used = size_of_the_chunk & 1;
        size_of_the_chunk -= prev_used;
        size_of_the_previous_chunk = *((size_t *)((char *)p));
        printf("chunks[%d]: %p, size = %li, prev (%s) = %li\n",
              i, p, size_of_the_chunk,
              (prev_used? "allocated": "unallocated"), size_of_the_previous_chunk);
    }
    return (EXIT_SUCCESS);
}
```

编译运行：

```
julien@holberton:~/holberton/w/hackthevm3$ gcc -Wall -Wextra -pedantic -Werror 8-main.c -o 8
julien@holberton:~/holberton/w/hackthevm3$ ./8 
chunks[0]: 00 00 00 00 00 00 00 00 11 04 00 00 00 00 00 00
chunks[0]: 0x1031000, size = 1040, prev (allocated) = 0
chunks[1]: 00 00 00 00 00 00 00 00 11 08 00 00 00 00 00 00
chunks[1]: 0x1031410, size = 2064, prev (allocated) = 0
chunks[2]: 00 00 00 00 00 00 00 00 11 0c 00 00 00 00 00 00
chunks[2]: 0x1031c20, size = 3088, prev (allocated) = 0
chunks[3]: 00 00 00 00 00 00 00 00 11 10 00 00 00 00 00 00
chunks[3]: 0x1032830, size = 4112, prev (allocated) = 0
chunks[4]: 10 10 00 00 00 00 00 00 10 14 00 00 00 00 00 00
chunks[4]: 0x1033840, size = 5136, prev (unallocated) = 4112
chunks[5]: 00 00 00 00 00 00 00 00 11 18 00 00 00 00 00 00
chunks[5]: 0x1034c50, size = 6160, prev (allocated) = 0
chunks[6]: 00 00 00 00 00 00 00 00 11 1c 00 00 00 00 00 00
chunks[6]: 0x1036460, size = 7184, prev (allocated) = 0
chunks[7]: 00 00 00 00 00 00 00 00 11 20 00 00 00 00 00 00
chunks[7]: 0x1038070, size = 8208, prev (allocated) = 0
chunks[8]: 10 20 00 00 00 00 00 00 10 24 00 00 00 00 00 00
chunks[8]: 0x103a080, size = 9232, prev (unallocated) = 8208
chunks[9]: 00 00 00 00 00 00 00 00 11 28 00 00 00 00 00 00
chunks[9]: 0x103c490, size = 10256, prev (allocated) = 0
julien@holberton:~/holberton/w/hackthevm3$ 
```
结合上面的输出，注意下面计算本内存块大小的代码，你是否已经明白多出来的`1`个字节了是怎么回事了？

```c
size_of_the_chunk = *((size_t *)((char *)p + 8));
prev_used = size_of_the_chunk & 1;
size_of_the_chunk -= prev_used;
```

## 六、堆真的是向上生长吗 ###
这是最后一个未解决的问题。从`man brk`看以看到：
```
DESCRIPTION
       brk() and sbrk() change the location of the program break, which defines the end  of  the
       process's  data  segment  (i.e., the program break is the first location after the end of
       the uninitialized data segment).  Increasing the program break has the effect of allocat‐
       ing memory to the process; decreasing the break deallocates memory.
```

我们通过程序验证下（`9-main.c`）：

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * main - moving the program break
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    int i;

    write(1, "START\n", 6);
    malloc(1);
    getchar();
    write(1, "LOOP\n", 5);
    for (i = 0; i < 0x25000 / 1024; i++)
    {
        malloc(1024);
    }
    write(1, "END\n", 4);
    getchar();
    return (EXIT_SUCCESS);
}
```
运行过程中我们使用`strace`跟踪，看下系统调用`brk`：
```
julien@holberton:~/holberton/w/hackthevm3$ strace ./9 
execve("./9", ["./9"], [/* 61 vars */]) = 0
...
write(1, "START\n", 6START
)                  = 6
brk(0)                                  = 0x1fd8000
brk(0x1ff9000)                          = 0x1ff9000
...
write(1, "LOOP\n", 5LOOP
)                   = 5
brk(0x201a000)                          = 0x201a000
write(1, "END\n", 4END
)                    = 4
...
julien@holberton:~/holberton/w/hackthevm3$ 
```

可以看出，`malloc`共调用了两次`brk`来增加堆大小，并且第二次调用的时候传给`brk`的参数（`0x201a000`）比第一次大（`0x1ff9000`）。第二次调用`brk`是因为第一次分配的堆不够`malloc`使用了。

我们通过`/proc`文件再次确认下：

```
julien@holberton:~/holberton/w/hackthevm3$ gcc -Wall -Wextra -pedantic -Werror 9-main.c -o 9
julien@holberton:~/holberton/w/hackthevm3$ ./9
START
```

```
julien@holberton:/proc/7855$ ps aux | grep \ \./9$
julien     7972  0.0  0.0   4332   684 pts/9    S+   19:08   0:00 ./9
julien@holberton:/proc/7855$ cd /proc/7972
julien@holberton:/proc/7972$ cat maps
...
00901000-00922000 rw-p 00000000 00:00 0                                  [heap]
...
julien@holberton:/proc/7972$ 
```

此时的堆区段是：
```
00901000-00922000 rw-p 00000000 00:00 0 [heap]
```

我们按下回车键让程序继续运行，并查看堆区段：

```
LOOP
END
```

```
julien@holberton:/proc/7972$ cat maps
...
00901000-00943000 rw-p 00000000 00:00 0                                  [heap]
...
julien@holberton:/proc/7972$ 
```

此时的堆区段是：
```
00901000-00943000 rw-p 00000000 00:00 0 [heap]
```

可以看到，堆的起始地址没变，但是结束地址已经从`00922000`变为 `00943000`。由此可见，**堆确实是向上生长的。**

## 七、地址空间布局随机化(ASLR) ##

你可能已经注意到`/proc/pid/maps`中的奇怪事情：

`program break`是数据段的当前结束位置，因此应该（在进程虚拟地址空间中）紧挨着可执行文件。这么一来，堆也应该从可执行文件结束的地址开始。但是从之前的打印可以看到，事实并不是这样的。唯一正确的是在内存虚拟空间中，堆总是可执行文件的下一个区段，这是合理的，因为堆是可执行文件数据段的一部分。另外，仔细观察发现，可执行区段和堆之间的间隔总是不同的:

*下面列举了几个例子，说明可执行区和堆之间的间隔。每一行的格式是：  
  `[进程号] 堆起始地址 - 可执行区结束地址 = 间隔`*

- `[3718]:01195000 – 00602000 = b93000`
- `[3834]:024d6000 – 00602000 = 1ed4000`
- `[4014]:00e70000 – 00602000 = 86e000`
- `[4172]:01314000 – 00602000 = d12000`
- `[7972]:00901000 – 00602000 = 2ff000`

看起来二者之间的间隔是随机的，事实上，确实是随机的。通过查看ELF二进制加载相关的源码（`fs/binfmt_elf.c`）可以发现：
```c
if ((current->flags & PF_RANDOMIZE) && (randomize_va_space > 1)) {
        current->mm->brk = current->mm->start_brk =
                arch_randomize_brk(current->mm);
#ifdef compat_brk_randomized
        current->brk_randomized = 1;
#endif
}
```
其中的`current->mm->brk`就是`program break`的地址。函数`arch_randomize_brk`可以在文件`arch/x86/kernel/process.c`中找到：

```c
unsigned long arch_randomize_brk(struct mm_struct *mm)
{
        unsigned long range_end = mm->brk + 0x02000000;
        return randomize_range(mm->brk, range_end, 0) ? : mm->brk;
}
```

其中`randomize_range`返回一个起始地址，`randomize_range`源码如下：
```c
/*
 * randomize_range() returns a start address such that
 *
 *    [...... <range> .....]
 *  start                  end
 *
 * a <range> with size "len" starting at the return value is inside in the
 * area defined by [start, end], but is otherwise randomized.
 */
unsigned long
randomize_range(unsigned long start, unsigned long end, unsigned long len)
{
        unsigned long range = end - len - start;

        if (end <= start + len)
                return 0;
        return PAGE_ALIGN(get_random_int() % range + start);
}
```
这样就导致，可执行文件数据段和`program break`初始位置之间就存在一个随机间隔，间隔大小可能是`0`到`0x02000000`之间的任一数值。这一随机值被称为地址空间布局随机化(ASLR)。ASLR是一项计算机安全技术，用来防止内存攻击。例如，为了防止黑客跳转到内存中的特定函数，ASLR将进程的堆、栈等关键数据区域在内存地址空间中的位置随机化。

## 八、进程地址空间示意图 ##

结合前面我们学到的知识，可以画出新的进程地址空间图：

![](/assets/images/virtual_memory/virtual_memory_diagram_v2.png)

## 九、malloc(0) ##

你有没有想过`malloc` `0`字节内存会发生什么？示例代码如下（`10-main.c`）：

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**                                                                                            
 * pmem - print mem                                                                            
 * @p: memory address to start printing from                                                   
 * @bytes: number of bytes to print                                                            
 *                                                                                             
 * Return: nothing                                                                             
 */
void pmem(void *p, unsigned int bytes)
{
    unsigned char *ptr;
    unsigned int i;

    ptr = (unsigned char *)p;
    for (i = 0; i < bytes; i++)
    {
        if (i != 0)
        {
            printf(" ");
        }
        printf("%02x", *(ptr + i));
    }
    printf("\n");
}

/**
 * main - moving the program break
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    void *p;
    size_t size_of_the_chunk;
    char prev_used;

    p = malloc(0);
    printf("%p\n", p);
    pmem((char *)p - 0x10, 0x10);
    size_of_the_chunk = *((size_t *)((char *)p - 8));
    prev_used = size_of_the_chunk & 1;
    size_of_the_chunk -= prev_used;
    printf("chunk size = %li bytes\n", size_of_the_chunk);
    return (EXIT_SUCCESS);
}
```

```
julien@holberton:~/holberton/w/hackthevm3$ gcc -Wall -Wextra -pedantic -Werror 10-main.c -o 10
julien@holberton:~/holberton/w/hackthevm3$ ./10
0xd08010
00 00 00 00 00 00 00 00 21 00 00 00 00 00 00 00
chunk size = 32 bytes
julien@holberton:~/holberton/w/hackthevm3$ 
```
可以看出，`malloc(0)`实际上占用了32个字节（包含`malloc`返回地址前的16个字节）。不过，请注意，并非所有平台上都如此。从`man malloc`可以看到：
```
NULL may also be returned by a successful call to malloc() with a size of zero
```


## 十、结尾 ##
本文我们学校了有关`malloc`和堆的许多知识。但实际上远不止`brk`和`sbrk`，你可以试着分配一大块内存，并用`strace`跟踪下，然后查看`/proc`系列文件，这都是有待进一步探究的。

另外，`free`函数是怎么和`malloc`函数协作的，这也是有待进一步探究的。 这会让你明白为什么最小的内存块是`32`字节而不是`16`字节，或者`0`字节。

## 十一、继续阅读 ##

- 第一篇:[虚拟内存探究 -- 第一篇:C strings & /proc](https://tech.coderhuo.tech/posts/Virtual_Memory_C_strings_proc/)
- 第二篇:[虚拟内存探究 -- 第二篇:Python 字节](https://tech.coderhuo.tech/posts/Virtual_Memory_python_bytes/)
- 第三篇:[虚拟内存探究 -- 第三篇:一步一步画虚拟内存图](https://tech.coderhuo.tech/posts/Virtual_Memory_drawing_VM_diagram/)
- 第四篇:[虚拟内存探究 -- 第四篇:malloc, heap & the program break](https://tech.coderhuo.tech/posts/Virtual_Memory_malloc_and_heap/)
- 第五篇:[虚拟内存探究 -- 第五篇:The Stack, registers and assembly code](https://tech.coderhuo.tech/posts/Virtual_Memory_malloc_and_heap_stack_and_register/)

## 十二、原文链接 ##
[Hack the Virtual Memory: malloc, the heap & the program break](https://blog.holbertonschool.com/hack-the-virtual-memory-malloc-the-heap-the-program-break/)
