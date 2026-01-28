---
title: "虚拟内存探究 -- 第一篇:C strings & /proc"
date:   2017-10-12
categories: [操作系统]
tags: [虚拟内存, 翻译]
---

* content
{:toc}
这是虚拟内存系列文章的第一篇。  
本文通过实验的手段， 带大家了解一些计算机科学相关的基础知识。   
在本文，我们将利用`/proc`查找进程虚拟内存中的ASCII字符串, 然后修改该字符串。  
在这一过程中，我们将学到很多有趣的东西。


## 一、实验环境 ##
所有的脚本和程序都在下面的环境中测试过：  

- Ubuntu 14.04 LTS  
	- Linux ubuntu 4.4.0-31-generic #50~14.04.1-Ubuntu SMP Wed Jul 13 01:07:32 UTC 2016 x86_64 x86_64 x86_64 GNU/Linux
- gcc  
	- gcc (Ubuntu 4.8.4-2ubuntu1~14.04.3) 4.8.4
- Python 3  
	- Python 3.4.3 (default, Nov 17 2016, 01:08:31)  
	- [GCC 4.8.4] on linux

## 二、预备知识 ##
阅读本文所需知识：  

- C语言基础
- 些许Python知识
- 了解Linux的文件系统和shell命令

## 三、虚拟内存 ##
在计算机领域， 虚拟内存是通过软硬件结合实现的一种内存管理技术， 它将程序所使用的内存地址（虚拟内存地址）映射到计算机的物理内存上（物理内存地址），这使得每个程序看到的内存地址空间都是连续的（或是一些连续地址空间的集合）。操作系统管理虚拟地址空间， 以及虚拟地址空间到物理内存的映射。CPU中的地址转换硬件(通常被称为内存管理单元, MMU)自动将虚拟内存地址转换成物理内存地址。操作系统可以提供比实际物理内存更多的虚拟内存，这一行为是通过操作系统中的软件来实现的。  

虚拟内存的主要好处包含以下几点:  

- 将应用程序从内存管理中解放出来, 应用程序只需关心自己的逻辑
- 不同应用程序间的虚拟内存是相互隔离的, 所以安全性增加了
- 结合内存分页管理技术, 应用程序理论上可使用比物理内存更多的内存空间  

有关虚拟内存的知识, 可进一步阅读维基百科上的相关介绍：[虚拟内存](https://en.wikipedia.org/wiki/Virtual_memory)。

在[虚拟内存探究 -- 第三篇:一步一步画虚拟内存图](https://tech.coderhuo.tech/posts/Virtual_Memory_drawing_VM_diagram/)中，我们将探索虚拟内存的更多细节，并且看下虚拟内存中都有些什么， 以及这些东西分别位于虚拟内存的什么地方。  

继续阅读本文前, 你需要知道以下几点:

- 每个进程都有自己独立的虚拟内存
- 虚拟内存大小依赖于计算机系统架构
- 不同的操作系统对虚拟内存的处理会有所不同, 对于现代的大多数操作系统来说, 虚拟内存如下所示:

![虚拟内存示意图](/assets/images/virtual_memory/virtual_memory.jpg)

在虚拟内存的高地址空间，我们可以看到(下面仅列出了部分内容，并非全部)：

- 命令行参数和环境变量
- “向下”生长的栈。咋看之下这是违反直觉的，但这确实是虚拟内存中栈的实现方式。


在虚拟内存的低地址空间, 我们可以看到：

- 可执行程序(实际上远比这复杂，但对于理解本文剩余内容足够了)
- “向上”生长的堆

堆是虚拟内存的一部分，动态分配的内存(比如用malloc分配的内存)位于堆中。

请时刻记住, **虚拟内存和物理内存是不同的**。  


## 四、剖析一个简单的C程序 ##



我们从一个简单的C程序开始:

```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * main - uses strdup to create a new string, and prints the
 * address of the new duplcated string
 *
 * Return: EXIT_FAILURE if malloc failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    char *s;

    s = strdup("Holberton");
    if (s == NULL)
    {
        fprintf(stderr, "Can't allocate mem with malloc\n");
        return (EXIT_FAILURE);
    }
    printf("%p\n", (void *)s);
    return (EXIT_SUCCESS);
}
```
### strdup ###
继续阅读本文前，请想一想`strdup`是如何复制字符串`Holberton`的？又该如何证明呢？

**.**

**.**

**.**  

`strdup`必须创建一个新的字符串，所以它首先会为新字符串手分配内存， 并且可能使用`malloc`分配内存。这可以通过`strdup`的man手册确认：

```
DESCRIPTION
       The  strdup()  function returns a pointer to a new string which is a duplicate of the string s.
       Memory for the new string is obtained with malloc(3), and can be freed with free(3).

```

*等等，基于上文关于虚拟内存的知识，你认为新创建的字符串位于虚拟内存的哪里呢？是高地址空间还是低地址空间？*


**.**

**.**

**.**  

应该是位于低地址空间(堆中)。为了验证我们的推断，让我们编译并执行这个C程序：


```shell
julien@holberton:~/holberton/w/hackthevm0$ gcc -Wall -Wextra -pedantic -Werror main.c -o holberton
julien@holberton:~/holberton/w/hackthevm0$ ./holberton 
0x1822010
julien@holberton:~/holberton/w/hackthevm0$ 
```

新创建的字符串地址是`0x1822010`。但它究竟是高地址还是低地址呢？  


### 进程的虚拟内存空间多大 ###

进程虚拟地址空间的大小依赖于计算机系统架构。我运行本例使用的是64位机器，所以理论上每个进程的虚拟内存是2^64字节，内存最高地址是`0xffffffffffffffff` (1.8446744e+19)，最低地址是`0x0`。  

`0x1822010`小于`0xffffffffffffffff`，所以新字符串可能是位于低地址。借助`proc`文件系统，我们可以确认这一点。

### proc文件系统 ###

由`man proc`可见：

```
The proc filesystem is a pseudo-filesystem which provides an interface to kernel data structures.  It is commonly mounted at `/proc`.  Most of it is read-only, but some files allow kernel variables to be changed.
```

`/proc`目录包含一系列文件，我们只关注其中的两个：

- `/proc/[pid]/mem`
- `/proc/[pid]/maps`

#### mem文件 ####

由`man proc`可见：

```
      /proc/[pid]/mem
              This file can be used to access the pages of a process's memory
          through open(2), read(2), and lseek(2).
```
太棒了！如此看来， 我们可以访问并修改任一进程的虚拟内存。

#### maps文件 ####

由`man proc`可见：

```
      /proc/[pid]/maps
              A  file containing the currently mapped memory regions and their access permissions.
          See mmap(2) for some further information about memory mappings.

              The format of the file is:

       address           perms offset  dev   inode       pathname
       00400000-00452000 r-xp 00000000 08:02 173521      /usr/bin/dbus-daemon
       00651000-00652000 r--p 00051000 08:02 173521      /usr/bin/dbus-daemon
       00652000-00655000 rw-p 00052000 08:02 173521      /usr/bin/dbus-daemon
       00e03000-00e24000 rw-p 00000000 00:00 0           [heap]
       00e24000-011f7000 rw-p 00000000 00:00 0           [heap]
       ...
       35b1800000-35b1820000 r-xp 00000000 08:02 135522  /usr/lib64/ld-2.15.so
       35b1a1f000-35b1a20000 r--p 0001f000 08:02 135522  /usr/lib64/ld-2.15.so
       35b1a20000-35b1a21000 rw-p 00020000 08:02 135522  /usr/lib64/ld-2.15.so
       35b1a21000-35b1a22000 rw-p 00000000 00:00 0
       35b1c00000-35b1dac000 r-xp 00000000 08:02 135870  /usr/lib64/libc-2.15.so
       35b1dac000-35b1fac000 ---p 001ac000 08:02 135870  /usr/lib64/libc-2.15.so
       35b1fac000-35b1fb0000 r--p 001ac000 08:02 135870  /usr/lib64/libc-2.15.so
       35b1fb0000-35b1fb2000 rw-p 001b0000 08:02 135870  /usr/lib64/libc-2.15.so
       ...
       f2c6ff8c000-7f2c7078c000 rw-p 00000000 00:00 0    [stack:986]
       ...
       7fffb2c0d000-7fffb2c2e000 rw-p 00000000 00:00 0   [stack]
       7fffb2d48000-7fffb2d49000 r-xp 00000000 00:00 0   [vdso]

              The address field is the address space in the process that the mapping occupies.
          The perms field is a set of permissions:

                   r = read
                   w = write
                   x = execute
                   s = shared
                   p = private (copy on write)

              The offset field is the offset into the file/whatever;
          dev is the device (major:minor); inode is the inode on that device.   0  indicates
              that no inode is associated with the memory region,
          as would be the case with BSS (uninitialized data).

              The  pathname field will usually be the file that is backing the mapping.
          For ELF files, you can easily coordinate with the offset field
              by looking at the Offset field in the ELF program headers (readelf -l).

              There are additional helpful pseudo-paths:

                   [stack]
                          The initial process's (also known as the main thread's) stack.

                   [stack:<tid>] (since Linux 3.4)
                          A thread's stack (where the <tid> is a thread ID).
              It corresponds to the /proc/[pid]/task/[tid]/ path.

                   [vdso] The virtual dynamically linked shared object.

                   [heap] The process's heap.

              If the pathname field is blank, this is an anonymous mapping as obtained via the mmap(2) function.
          There is no easy  way  to  coordinate
              this back to a process's source, short of running it through gdb(1), strace(1), or similar.

              Under Linux 2.0 there is no field giving pathname.
```
这意味着我们可以通过文件`/proc/[pid]/mem`找到一个正在运行的进程的堆。如果可以读取堆内容，我们就能找到想要修改的字符串。如果能够往堆中写数据，我们就可以进行字符串替换。


### pid ###

进程是可执行程序的一个实例，拥有一个全局唯一的进程ID（PID)。有些函数/系统调用使用PID操作进程或者和进程进行交互。  

可以通过命令`ps`来查看某个进程的pid（具体请参阅`man ps`)。

## 五、替换进程的字符串 ##
我们接下来要在一个进程的堆中搜索特定字符串，并用另一个字符串（长度不大于原字符串）替换它。 现在我们已经掌握了所需要的理论知识。

下面这个程序是我们将要hack的程序，正常情况下它循环输出字符串`Holberton`。

```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/**              
 * main - uses strdup to create a new string, loops forever-ever
 *                
 * Return: EXIT_FAILURE if malloc failed. Other never returns
 */
int main(void)
{
     char *s;
     unsigned long int i;

     s = strdup("Holberton");
     if (s == NULL)
     {
          fprintf(stderr, "Can't allocate mem with malloc\n");
          return (EXIT_FAILURE);
     }
     i = 0;
     while (s)
     {
          printf("[%lu] %s (%p)\n", i, s, (void *)s);
          sleep(1);
          i++;
     }
     return (EXIT_SUCCESS);
}
```

编译运行该程序，它将循环输出字符串`Holberton`直到进程被杀死。

```shell
julien@holberton:~/holberton/w/hackthevm0$ gcc -Wall -Wextra -pedantic -Werror loop.c -o loop
julien@holberton:~/holberton/w/hackthevm0$ ./loop 
[0] Holberton (0xfbd010)
[1] Holberton (0xfbd010)
[2] Holberton (0xfbd010)
[3] Holberton (0xfbd010)
[4] Holberton (0xfbd010)
[5] Holberton (0xfbd010)
[6] Holberton (0xfbd010)
[7] Holberton (0xfbd010)
...
```
感兴趣的话，你可以暂停阅读本文，尝试写个脚本/程序寻找进程堆中的字符串。


**.**

**.**

**.**  


### /proc探究 ###

首先运行刚才编译出来的可执行程序`loop`.

```shell
julien@holberton:~/holberton/w/hackthevm0$ ./loop 
[0] Holberton (0x10ff010)
[1] Holberton (0x10ff010)
[2] Holberton (0x10ff010)
[3] Holberton (0x10ff010)
...
```

然后找到这个进程的ID

```shell
julien@holberton:~/holberton/w/hackthevm0$ ps aux | grep ./loop | grep -v grep
julien     4618  0.0  0.0   4332   732 pts/14   S+   17:06   0:00 ./loop
```

上面的例子中， 进程ID是4618（不同机器上不同时刻运行例程，进程ID都可能不同）。所以，我们将要查看的文件`maps`和`mem`位于目录`/proc/4618`:

- `/proc/4618/maps`
- `/proc/4618/mem`


在目录`/proc/4618`下执行`ls -la`可以看到以下内容：

```shell
julien@ubuntu:/proc/4618$ ls -la
total 0
dr-xr-xr-x   9 julien julien 0 Mar 15 17:07 .
dr-xr-xr-x 257 root   root   0 Mar 15 10:20 ..
dr-xr-xr-x   2 julien julien 0 Mar 15 17:11 attr
-rw-r--r--   1 julien julien 0 Mar 15 17:11 autogroup
-r--------   1 julien julien 0 Mar 15 17:11 auxv
-r--r--r--   1 julien julien 0 Mar 15 17:11 cgroup
--w-------   1 julien julien 0 Mar 15 17:11 clear_refs
-r--r--r--   1 julien julien 0 Mar 15 17:07 cmdline
-rw-r--r--   1 julien julien 0 Mar 15 17:11 comm
-rw-r--r--   1 julien julien 0 Mar 15 17:11 coredump_filter
-r--r--r--   1 julien julien 0 Mar 15 17:11 cpuset
lrwxrwxrwx   1 julien julien 0 Mar 15 17:11 cwd -> /home/julien/holberton/w/funwthevm
-r--------   1 julien julien 0 Mar 15 17:11 environ
lrwxrwxrwx   1 julien julien 0 Mar 15 17:11 exe -> /home/julien/holberton/w/funwthevm/loop
dr-x------   2 julien julien 0 Mar 15 17:07 fd
dr-x------   2 julien julien 0 Mar 15 17:11 fdinfo
-rw-r--r--   1 julien julien 0 Mar 15 17:11 gid_map
-r--------   1 julien julien 0 Mar 15 17:11 io
-r--r--r--   1 julien julien 0 Mar 15 17:11 limits
-rw-r--r--   1 julien julien 0 Mar 15 17:11 loginuid
dr-x------   2 julien julien 0 Mar 15 17:11 map_files
-r--r--r--   1 julien julien 0 Mar 15 17:11 maps
-rw-------   1 julien julien 0 Mar 15 17:11 mem
-r--r--r--   1 julien julien 0 Mar 15 17:11 mountinfo
-r--r--r--   1 julien julien 0 Mar 15 17:11 mounts
-r--------   1 julien julien 0 Mar 15 17:11 mountstats
dr-xr-xr-x   5 julien julien 0 Mar 15 17:11 net
dr-x--x--x   2 julien julien 0 Mar 15 17:11 ns
-r--r--r--   1 julien julien 0 Mar 15 17:11 numa_maps
-rw-r--r--   1 julien julien 0 Mar 15 17:11 oom_adj
-r--r--r--   1 julien julien 0 Mar 15 17:11 oom_score
-rw-r--r--   1 julien julien 0 Mar 15 17:11 oom_score_adj
-r--------   1 julien julien 0 Mar 15 17:11 pagemap
-r--------   1 julien julien 0 Mar 15 17:11 personality
-rw-r--r--   1 julien julien 0 Mar 15 17:11 projid_map
lrwxrwxrwx   1 julien julien 0 Mar 15 17:11 root -> /
-rw-r--r--   1 julien julien 0 Mar 15 17:11 sched
-r--r--r--   1 julien julien 0 Mar 15 17:11 schedstat
-r--r--r--   1 julien julien 0 Mar 15 17:11 sessionid
-rw-r--r--   1 julien julien 0 Mar 15 17:11 setgroups
-r--r--r--   1 julien julien 0 Mar 15 17:11 smaps
-r--------   1 julien julien 0 Mar 15 17:11 stack
-r--r--r--   1 julien julien 0 Mar 15 17:07 stat
-r--r--r--   1 julien julien 0 Mar 15 17:11 statm
-r--r--r--   1 julien julien 0 Mar 15 17:07 status
-r--------   1 julien julien 0 Mar 15 17:11 syscall
dr-xr-xr-x   3 julien julien 0 Mar 15 17:11 task
-r--r--r--   1 julien julien 0 Mar 15 17:11 timers
-rw-r--r--   1 julien julien 0 Mar 15 17:11 uid_map
-r--r--r--   1 julien julien 0 Mar 15 17:11 wchan
```

#### /proc/pid/maps ####

如之前所见，文件`/proc/pid/maps`是个文本文件，我们可以直接读取，内容如下：

```shell
julien@ubuntu:/proc/4618$ cat maps
00400000-00401000 r-xp 00000000 08:01 1070052                            /home/julien/holberton/w/funwthevm/loop
00600000-00601000 r--p 00000000 08:01 1070052                            /home/julien/holberton/w/funwthevm/loop
00601000-00602000 rw-p 00001000 08:01 1070052                            /home/julien/holberton/w/funwthevm/loop
010ff000-01120000 rw-p 00000000 00:00 0                                  [heap]
7f144c052000-7f144c20c000 r-xp 00000000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f144c20c000-7f144c40c000 ---p 001ba000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f144c40c000-7f144c410000 r--p 001ba000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f144c410000-7f144c412000 rw-p 001be000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f144c412000-7f144c417000 rw-p 00000000 00:00 0 
7f144c417000-7f144c43a000 r-xp 00000000 08:01 136229                     /lib/x86_64-linux-gnu/ld-2.19.so
7f144c61e000-7f144c621000 rw-p 00000000 00:00 0 
7f144c636000-7f144c639000 rw-p 00000000 00:00 0 
7f144c639000-7f144c63a000 r--p 00022000 08:01 136229                     /lib/x86_64-linux-gnu/ld-2.19.so
7f144c63a000-7f144c63b000 rw-p 00023000 08:01 136229                     /lib/x86_64-linux-gnu/ld-2.19.so
7f144c63b000-7f144c63c000 rw-p 00000000 00:00 0 
7ffc94272000-7ffc94293000 rw-p 00000000 00:00 0                          [stack]
7ffc9435e000-7ffc94360000 r--p 00000000 00:00 0                          [vvar]
7ffc94360000-7ffc94362000 r-xp 00000000 00:00 0                          [vdso]
ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]
```

回想前面的内容，可以看到，栈（`[stack]`）位于内存的高地址，堆（`[heap]`）位于内存的低地址。


#### [heap] ####
从`maps`文件中， 我们可以找到搜索字符串需要的所有信息:

```shell
010ff000-01120000 rw-p 00000000 00:00 0                                  [heap]
```
这个进程的堆信息如下:

- 在虚拟内存中的起始地址是`0x010ff000`
- 结束地址是`01120000`
- 权限是可读写的（`rw`）

回顾下正在运行的`loop`的输出:

```shell
...
[1024] Holberton (0x10ff010)
...
```

`0x010ff000` < `0x10ff010` < `0x01120000`。这证明了我们的字符串是在堆上。更精确的说，新字符串是在堆偏移`0x10`的地方。如果我们打开文件`/proc/4618/mem`并且将文件指针移动到`0x10ff010`， 我们就能替换正在运行的程序`loop`中的字符串`Holberton`。

我们接下来会写个程序/脚本做这件事情。  
你也可以暂停阅读本文，用自己最熟悉的语言尝试写个脚本/程序来做这件事情。


**.**

**.**

**.**  


### 替换虚拟内存中的字符串###
下面是我们用Python3 实现字符串替换的脚本(read_write_heap.py):

```python
#!/usr/bin/env python3
'''             
Locates and replaces the first occurrence of a string in the heap
of a process    

Usage: ./read_write_heap.py PID search_string replace_by_string
Where:           
- PID is the pid of the target process
- search_string is the ASCII string you are looking to overwrite
- replace_by_string is the ASCII string you want to replace
  search_string with
'''

import sys

def print_usage_and_exit():
    print('Usage: {} pid search write'.format(sys.argv[0]))
    sys.exit(1)

# check usage  
if len(sys.argv) != 4:
    print_usage_and_exit()

# get the pid from args
pid = int(sys.argv[1])
if pid <= 0:
    print_usage_and_exit()
search_string = str(sys.argv[2])
if search_string  == "":
    print_usage_and_exit()
write_string = str(sys.argv[3])
if search_string  == "":
    print_usage_and_exit()

# open the maps and mem files of the process
maps_filename = "/proc/{}/maps".format(pid)
print("[*] maps: {}".format(maps_filename))
mem_filename = "/proc/{}/mem".format(pid)
print("[*] mem: {}".format(mem_filename))

# try opening the maps file
try:
    maps_file = open('/proc/{}/maps'.format(pid), 'r')
except IOError as e:
    print("[ERROR] Can not open file {}:".format(maps_filename))
    print("        I/O error({}): {}".format(e.errno, e.strerror))
    sys.exit(1)

for line in maps_file:
    sline = line.split(' ')
    # check if we found the heap
    if sline[-1][:-1] != "[heap]":
        continue
    print("[*] Found [heap]:")

    # parse line
    addr = sline[0]
    perm = sline[1]
    offset = sline[2]
    device = sline[3]
    inode = sline[4]
    pathname = sline[-1][:-1]
    print("\tpathname = {}".format(pathname))
    print("\taddresses = {}".format(addr))
    print("\tpermisions = {}".format(perm))
    print("\toffset = {}".format(offset))
    print("\tinode = {}".format(inode))

    # check if there is read and write permission
    if perm[0] != 'r' or perm[1] != 'w':
        print("[*] {} does not have read/write permission".format(pathname))
        maps_file.close()
        exit(0)

    # get start and end of the heap in the virtual memory
    addr = addr.split("-")
    if len(addr) != 2: # never trust anyone, not even your OS :)
        print("[*] Wrong addr format")
        maps_file.close()
        exit(1)
    addr_start = int(addr[0], 16)
    addr_end = int(addr[1], 16)
    print("\tAddr start [{:x}] | end [{:x}]".format(addr_start, addr_end))

    # open and read mem
    try:
        mem_file = open(mem_filename, 'rb+')
    except IOError as e:
        print("[ERROR] Can not open file {}:".format(mem_filename))
        print("        I/O error({}): {}".format(e.errno, e.strerror))
        maps_file.close()
        exit(1)

    # read heap  
    mem_file.seek(addr_start)
    heap = mem_file.read(addr_end - addr_start)

    # find string
    try:
        i = heap.index(bytes(search_string, "ASCII"))
    except Exception:
        print("Can't find '{}'".format(search_string))
        maps_file.close()
        mem_file.close()
        exit(0)
    print("[*] Found '{}' at {:x}".format(search_string, i))

    # write the new string
    print("[*] Writing '{}' at {:x}".format(write_string, addr_start + i))
    mem_file.seek(addr_start + i)
    mem_file.write(bytes(write_string, "ASCII"))

    # close files
    maps_file.close()
    mem_file.close()

    # there is only one heap in our example
    break
```

注意：需要以root权限执行上面的脚本， 否则无法读写文件`/proc/pid/mem`， 即使你是进程的所有者。

运行上面的脚本：

```shell
julien@holberton:~/holberton/w/hackthevm0$ sudo ./read_write_heap.py 4618 Holberton "Fun w vm!"
[*] maps: /proc/4618/maps
[*] mem: /proc/4618/mem
[*] Found [heap]:
    pathname = [heap]
    addresses = 010ff000-01120000
    permisions = rw-p
    offset = 00000000
    inode = 0
    Addr start [10ff000] | end [1120000]
[*] Found 'Holberton' at 10
[*] Writing 'Fun w vm!' at 10ff010
julien@holberton:~/holberton/w/hackthevm0$ 
```
可以看到上面脚本打印出来的地址和我们手动找到的是一致的:

- 进程的堆位于虚拟内存的`0x010ff000` ~ `0x01120000`
- 我们要找的字符串地址是`0x10ff010`, 相对于堆的起始地址偏移了`0x10`

回过头来看下我们的`loop`程序，它应该会打印字符串"fun w vm!"

```shell
...
[2676] Holberton (0x10ff010)
[2677] Holberton (0x10ff010)
[2678] Holberton (0x10ff010)
[2679] Holberton (0x10ff010)
[2680] Holberton (0x10ff010)
[2681] Holberton (0x10ff010)
[2682] Fun w vm! (0x10ff010)
[2683] Fun w vm! (0x10ff010)
[2684] Fun w vm! (0x10ff010)
[2685] Fun w vm! (0x10ff010)
...
```



![太不可思议了!](/assets/images/virtual_memory/blown-mind-explosion-gif.gif)



## 六、下节预告 ##

下一篇文章中我们要做的事情和本章类似， 不同的是我们将访问并修改一个Python3 脚本的内存。 这做起来比较吃力， 所以我们需要了解Pyhton3 内部的一些机制。不信你可以试试，上面的脚本read_write_heap.py并不能修改Python3进程中的ASCII字符串。

## 七、继续阅读 ##

- 第一篇:[虚拟内存探究 -- 第一篇:C strings & /proc](https://tech.coderhuo.tech/posts/Virtual_Memory_C_strings_proc/)
- 第二篇:[虚拟内存探究 -- 第二篇:Python 字节](https://tech.coderhuo.tech/posts/Virtual_Memory_python_bytes/)
- 第三篇:[虚拟内存探究 -- 第三篇:一步一步画虚拟内存图](https://tech.coderhuo.tech/posts/Virtual_Memory_drawing_VM_diagram/)
- 第四篇:[虚拟内存探究 -- 第四篇:malloc, heap & the program break](https://tech.coderhuo.tech/posts/Virtual_Memory_malloc_and_heap/)
- 第五篇:[虚拟内存探究 -- 第五篇:The Stack, registers and assembly code](https://tech.coderhuo.tech/posts/Virtual_Memory_malloc_and_heap_stack_and_register/)

## 八、原文链接 ##
[Hack The Virtual Memory: C strings & /proc](https://blog.holbertonschool.com/hack-the-virtual-memory-c-strings-proc/)
