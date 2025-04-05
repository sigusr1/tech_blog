---
title:  "虚拟内存探究 -- 第二篇:Python 字节"  
date:   2017-10-15
categories: [操作系统]
tags: [虚拟内存, 翻译]
---


* content
{:toc}
这是虚拟内存系列文章的第二篇。  
这次我们要做的事情和[《虚拟内存探究 -- 第一篇:C strings & /proc》](https://tech.coderhuo.tech/posts/Virtual_Memory_C_strings_proc/)类似，不同的是我们将访问Python 3 脚本的虚拟内存。这会比较费劲， 因为我们需要了解Pyhton3 内部的一些机制。







## 一、预备知识 ##
本文基于上一篇文章[《虚拟内存探究 -- 第一篇:C strings & /proc》](https://tech.coderhuo.tech/posts/Virtual_Memory_C_strings_proc/)中所讲的知识， 所以，在继续阅读本文前，请确保阅读并理解上一篇文章。  

为了方便理解本文，你需要具备以下知识：  

- C语言基础
- 些许Python知识
- 了解Linux的文件系统和shell命令
- `/proc`文件系统的基本知识（可参阅[《虚拟内存探究 -- 第一篇:C strings & /proc》](https://tech.coderhuo.tech/posts/Virtual_Memory_C_strings_proc/)中的相关介绍）

## 二、实验环境 ##
所有的脚本和程序都在下面的环境中测试过：  

- Ubuntu 14.04 LTS  
	- Linux ubuntu 4.4.0-31-generic #50~14.04.1-Ubuntu SMP Wed Jul 13 01:07:32 UTC 2016 x86_64 x86_64 x86_64 GNU/Linux
- gcc  
	- gcc (Ubuntu 4.8.4-2ubuntu1~14.04.3) 4.8.4
- Python 3  
	- Python 3.4.3 (default, Nov 17 2016, 01:08:31)  
	- [GCC 4.8.4] on linux

## 三、剖析一个简单的Python脚本 ##

下面是我们将要使用的Python脚本（`main.py`）。我们将尝试修改运行该脚本的进程虚拟内存中的“字符串” `Holberton`。

```python
#!/usr/bin/env python3
'''
Prints a b"string" (bytes object), reads a char from stdin
and prints the same (or not :)) string again
'''

import sys

s = b"Holberton"
print(s)
sys.stdin.read(1)
print(s)
```

### 1. Python中的字节对象(bytes object) ###

#### 1.1 字节和字符串(bytes vs str) ####
*译者注:bytes在这里翻译成字节， 并非指单个字符。*

如上面代码所示，我们使用一个字节对象（字符串`Holberton`前面的`b`说明这是个字节对象）来存储字符串`Holberton`。字节对象会把字符串中的字符以字节的形式（相对于每个字符占多个字节的字符串编码方式而言，也就是宽字符编码，具体可参阅`unicodeobject.h`）存下来。这样可以保证字符串在虚拟内存中是连续的ASCII码。

从技术上来讲， 上面代码中的`s`并不是一个Python字符串。如下所示， 它是一个字节对象(不过没关系, 这不影响我们的后续讨论)：

```shell
julien@holberton:~/holberton/w/hackthevm1$ python3
Python 3.4.3 (default, Nov 17 2016, 01:08:31) 
[GCC 4.8.4] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>> s = "Betty"
>>> type(s)
<class 'str'>
>>> s = b"Betty"
>>> type(s)
<class 'bytes'>
>>> quit()
```


#### 1.2 一切都是对象 ####

Pyhton中的整数、字符串、字节、函数等等， 都是对象。所以， 语句`s = b"Holberton"`将创建一个字节对象，并将字符串存在内存中某处。字符串`Holberton`很可能在堆上，因为Python必须为字节对象`s`以及`s`指向的字符串分配内存（字符串可能直接存在对象`s`中，也可能`s`只维护了一个指向字符串的索引，目前我们并不确定具体的实现）。


### 2. 执行read_write_heap.py脚本 ###
*提示：`read_write_heap.py`是[《虚拟内存探究 -- 第一篇:C strings & /proc》](https://tech.coderhuo.tech/posts/Virtual_Memory_C_strings_proc/)中的脚本，用来查找并替换内存中的字符串。*


我们首先执行前面的脚本`main.py`：
```shell
julien@holberton:~/holberton/w/hackthevm1$ ./main.py 
b'Holberton'
```

这时`main.py`阻塞在语句`sys.stdin.read(1)`上，一直在等待用户输入。

接下来我们用管理员权限执行脚本`read_write_heap.py`：

```shell
julien@holberton:~/holberton/w/hackthevm1$ ps aux | grep main.py | grep -v grep
julien     3929  0.0  0.7  31412  7848 pts/0    S+   15:10   0:00 python3 ./main.py
julien@holberton:~/holberton/w/hackthevm1$ sudo ./read_write_heap.py 3929 Holberton "~ Betty ~"
[*] maps: /proc/3929/maps
[*] mem: /proc/3929/mem
[*] Found [heap]:
    pathname = [heap]
    addresses = 022dc000-023c6000
    permisions = rw-p
    offset = 00000000
    inode = 0
    Addr start [22dc000] | end [23c6000]
[*] Found 'Holberton' at 8e192
[*] Writing '~ Betty ~' at 236a192
julien@holberton:~/holberton/w/hackthevm1$ 
```

不出所料，我们在堆上找到了字符串`Holberton`并且将之替换成'~ Betty ~'。  
现在我们按下回车键让脚本`main.py`继续执行，它应该会输出`b'~ Betty ~'`：

```shell
b'Holberton'
julien@holberton:~/holberton/w/hackthevm1$
```

**什么？？？**  
![**这不可能！！！**](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/giphy-4.gif?raw=true)

我们找到字符串`Holberton`并且替换了它，但是这不是我们要找的字符串？继续深入探究之前，我们需要再确认一件事情。我们的脚本`read_write_heap.py`在目标字符串首次出现之后就退出了，如果堆中有多个字符串`Holberton`呢？为了避免遗漏，我们将脚本`read_write_heap.py`执行多次。

还是先启动脚本`main.py`：
```shell
julien@holberton:~/holberton/w/hackthevm1$ ./main.py 
b'Holberton'
```
然后多次执行脚本`read_write_heap.py`：

```shell
julien@holberton:~/holberton/w/hackthevm1$ ps aux | grep main.py | grep -v grep
julien     4051  0.1  0.7  31412  7832 pts/0    S+   15:53   0:00 python3 ./main.py
julien@holberton:~/holberton/w/hackthevm1$ sudo ./read_write_heap.py 4051 Holberton "~ Betty ~"
[*] maps: /proc/4051/maps
[*] mem: /proc/4051/mem
[*] Found [heap]:
    pathname = [heap]
    addresses = 00bf4000-00cde000
    permisions = rw-p
    offset = 00000000
    inode = 0
    Addr start [bf4000] | end [cde000]
[*] Found 'Holberton' at 8e162
[*] Writing '~ Betty ~' at c82162
julien@holberton:~/holberton/w/hackthevm1$ sudo ./read_write_heap.py 4051 Holberton "~ Betty ~"
[*] maps: /proc/4051/maps
[*] mem: /proc/4051/mem
[*] Found [heap]:
    pathname = [heap]
    addresses = 00bf4000-00cde000
    permisions = rw-p
    offset = 00000000
    inode = 0
    Addr start [bf4000] | end [cde000]
Can't find 'Holberton'
julien@holberton:~/holberton/w/hackthevm1$ 
```

字符串'Holberton'在堆上只出现了一次。那么脚本`main.py`所使用的字符串'Holberton'到底在哪里呢？Python的字节对象又是在内存的哪部分呢？有没有可能在栈上？我们可以把脚本`read_write_heap.py`中的`[heap]`改成`[stack]`试试看。

*提示:文件`/proc/[pid]/maps`中标记为`[stack]`的部分就是栈， 具体可参阅上一篇文件[《虚拟内存探究 -- 第一篇:C strings & /proc》](https://tech.coderhuo.tech/posts/Virtual_Memory_C_strings_proc/)。*

改写栈的脚本`read_write_stack.py`如下， 它所做的和之前的脚本`read_write_heap.py`一样，唯一的不同是它访问进程的栈：

```python
#!/usr/bin/env python3
'''
Locates and replaces the first occurrence of a string in the stack
of a process

Usage: ./read_write_stack.py PID search_string replace_by_string
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
    # check if we found the stack
    if sline[-1][:-1] != "[stack]":
        continue
    print("[*] Found [stack]:")

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

    # get start and end of the stack in the virtual memory
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

    # read stack
    mem_file.seek(addr_start)
    stack = mem_file.read(addr_end - addr_start)

    # find string
    try:
        i = stack.index(bytes(search_string, "ASCII"))
    except Exception:
        print("Can't find '{}'".format(search_string))
        maps_file.close()
        mem_file.close()
        exit(0)
    print("[*] Found '{}' at {:x}".format(search_string, i))

    # write the new stringprint("[*] Writing '{}' at {:x}".format(write_string, addr_start + i))
    mem_file.seek(addr_start + i)
    mem_file.write(bytes(write_string, "ASCII"))

    # close filesmaps_file.close()
    mem_file.close()

    # there is only one stack in our example
    break
```

我们依次执行脚本`main.py`和`read_write_stack.py`尝试在栈上寻找字符串:

```shell
julien@holberton:~/holberton/w/hackthevm1$ ./main.py
b'Holberton'
```

```shell
julien@holberton:~/holberton/w/hackthevm1$ ps aux | grep main.py | grep -v grep
julien     4124  0.2  0.7  31412  7848 pts/0    S+   16:10   0:00 python3 ./main.py
julien@holberton:~/holberton/w/hackthevm1$ sudo ./read_write_stack.py 4124 Holberton "~ Betty ~"
[sudo] password for julien: 
[*] maps: /proc/4124/maps
[*] mem: /proc/4124/mem
[*] Found [stack]:
    pathname = [stack]
    addresses = 7fff2997e000-7fff2999f000
    permisions = rw-p
    offset = 00000000
    inode = 0
    Addr start [7fff2997e000] | end [7fff2999f000]
Can't find 'Holberton'
julien@holberton:~/holberton/w/hackthevm1$ 
```
由此可见， 我们的字符串既不在栈上也不在堆上。它究竟在哪里呢？  
我们只有从Python3的内部实现中去寻找答案。

## 四、从Python实现中寻找目标字符串 ##

*提示：Python3有很多实现版本，本文使用的是最原始的、最常用的CPython（用C语言实现的）。后续有关Python3的讨论都是基于CPython。*

### 1. id ###

有个简单的方法可以知道python的对象（注意：是对象不是字符串）位于虚拟内存的哪部分。CPython的内置函数`id()`实现比较特别，它返回对象的内存地址。  

下面的脚本`main_id.py`在`main.py`的基础上添加了打印对象id的语句，也就可以获得对象内存地址:

```python
#!/usr/bin/env python3
'''
Prints:
- the address of the bytes object
- a b"string" (bytes object)
reads a char from stdin
and prints the same (or not :)) string again
'''

import sys

s = b"Holberton"
print(hex(id(s)))
print(s)
sys.stdin.read(1)
print(s)
```

```shell
julien@holberton:~/holberton/w/hackthevm1$ ./main_id.py
0x7f343f010210
b'Holberton'
```

字节对象s的内存地址是`0x7f343f010210`。通过`/proc`可以查看对象到底位于哪里。

```shell
julien@holberton:/usr/include/python3.4$ ps aux | grep main_id.py | grep -v grep
julien     4344  0.0  0.7  31412  7856 pts/0    S+   16:53   0:00 python3 ./main_id.py
julien@holberton:/usr/include/python3.4$ cat /proc/4344/maps
00400000-006fa000 r-xp 00000000 08:01 655561                             /usr/bin/python3.4
008f9000-008fa000 r--p 002f9000 08:01 655561                             /usr/bin/python3.4
008fa000-00986000 rw-p 002fa000 08:01 655561                             /usr/bin/python3.4
00986000-009a2000 rw-p 00000000 00:00 0 
021ba000-022a4000 rw-p 00000000 00:00 0                                  [heap]
7f343d797000-7f343de79000 r--p 00000000 08:01 663747                     /usr/lib/locale/locale-archive
7f343de79000-7f343df7e000 r-xp 00000000 08:01 136303                     /lib/x86_64-linux-gnu/libm-2.19.so
7f343df7e000-7f343e17d000 ---p 00105000 08:01 136303                     /lib/x86_64-linux-gnu/libm-2.19.so
7f343e17d000-7f343e17e000 r--p 00104000 08:01 136303                     /lib/x86_64-linux-gnu/libm-2.19.so
7f343e17e000-7f343e17f000 rw-p 00105000 08:01 136303                     /lib/x86_64-linux-gnu/libm-2.19.so
7f343e17f000-7f343e197000 r-xp 00000000 08:01 136416                     /lib/x86_64-linux-gnu/libz.so.1.2.8
7f343e197000-7f343e396000 ---p 00018000 08:01 136416                     /lib/x86_64-linux-gnu/libz.so.1.2.8
7f343e396000-7f343e397000 r--p 00017000 08:01 136416                     /lib/x86_64-linux-gnu/libz.so.1.2.8
7f343e397000-7f343e398000 rw-p 00018000 08:01 136416                     /lib/x86_64-linux-gnu/libz.so.1.2.8
7f343e398000-7f343e3bf000 r-xp 00000000 08:01 136275                     /lib/x86_64-linux-gnu/libexpat.so.1.6.0
7f343e3bf000-7f343e5bf000 ---p 00027000 08:01 136275                     /lib/x86_64-linux-gnu/libexpat.so.1.6.0
7f343e5bf000-7f343e5c1000 r--p 00027000 08:01 136275                     /lib/x86_64-linux-gnu/libexpat.so.1.6.0
7f343e5c1000-7f343e5c2000 rw-p 00029000 08:01 136275                     /lib/x86_64-linux-gnu/libexpat.so.1.6.0
7f343e5c2000-7f343e5c4000 r-xp 00000000 08:01 136408                     /lib/x86_64-linux-gnu/libutil-2.19.so
7f343e5c4000-7f343e7c3000 ---p 00002000 08:01 136408                     /lib/x86_64-linux-gnu/libutil-2.19.so
7f343e7c3000-7f343e7c4000 r--p 00001000 08:01 136408                     /lib/x86_64-linux-gnu/libutil-2.19.so
7f343e7c4000-7f343e7c5000 rw-p 00002000 08:01 136408                     /lib/x86_64-linux-gnu/libutil-2.19.so
7f343e7c5000-7f343e7c8000 r-xp 00000000 08:01 136270                     /lib/x86_64-linux-gnu/libdl-2.19.so
7f343e7c8000-7f343e9c7000 ---p 00003000 08:01 136270                     /lib/x86_64-linux-gnu/libdl-2.19.so
7f343e9c7000-7f343e9c8000 r--p 00002000 08:01 136270                     /lib/x86_64-linux-gnu/libdl-2.19.so
7f343e9c8000-7f343e9c9000 rw-p 00003000 08:01 136270                     /lib/x86_64-linux-gnu/libdl-2.19.so
7f343e9c9000-7f343eb83000 r-xp 00000000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f343eb83000-7f343ed83000 ---p 001ba000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f343ed83000-7f343ed87000 r--p 001ba000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f343ed87000-7f343ed89000 rw-p 001be000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f343ed89000-7f343ed8e000 rw-p 00000000 00:00 0 
7f343ed8e000-7f343eda7000 r-xp 00000000 08:01 136373                     /lib/x86_64-linux-gnu/libpthread-2.19.so
7f343eda7000-7f343efa6000 ---p 00019000 08:01 136373                     /lib/x86_64-linux-gnu/libpthread-2.19.so
7f343efa6000-7f343efa7000 r--p 00018000 08:01 136373                     /lib/x86_64-linux-gnu/libpthread-2.19.so
7f343efa7000-7f343efa8000 rw-p 00019000 08:01 136373                     /lib/x86_64-linux-gnu/libpthread-2.19.so
7f343efa8000-7f343efac000 rw-p 00000000 00:00 0 
7f343efac000-7f343efcf000 r-xp 00000000 08:01 136229                     /lib/x86_64-linux-gnu/ld-2.19.so
7f343f000000-7f343f1b6000 rw-p 00000000 00:00 0 
7f343f1c5000-7f343f1cc000 r--s 00000000 08:01 918462                     /usr/lib/x86_64-linux-gnu/gconv/gconv-modules.cache
7f343f1cc000-7f343f1ce000 rw-p 00000000 00:00 0 
7f343f1ce000-7f343f1cf000 r--p 00022000 08:01 136229                     /lib/x86_64-linux-gnu/ld-2.19.so
7f343f1cf000-7f343f1d0000 rw-p 00023000 08:01 136229                     /lib/x86_64-linux-gnu/ld-2.19.so
7f343f1d0000-7f343f1d1000 rw-p 00000000 00:00 0 
7ffccf1fd000-7ffccf21e000 rw-p 00000000 00:00 0                          [stack]
7ffccf23c000-7ffccf23e000 r--p 00000000 00:00 0                          [vvar]
7ffccf23e000-7ffccf240000 r-xp 00000000 00:00 0                          [vdso]
ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]
julien@holberton:/usr/include/python3.4$ 
```

字节对象s位于以下内存区域:
```shell
7f343f000000-7f343f1b6000 rw-p 00000000 00:00 0
```
这既不是堆也不是栈，这也说明了为什么我们替换字符串一直没有成功。但这并不意味着字符串本身也存放在这一内存区域。比如，字节对象`s`可以维护一个指向字符串的指针，而不是把字符串复制一份。当然，我们可以粗暴的搜索这一内存区域来查看字符串是不是在这里。先别急， 我们先多了解下Pyhton中的字节对象。
### 2. bytesobject.h ###

我们使用的是C语言实现的Pyhton(CPyhton），所以让我们先看下字节对象的头文件。

*提示:如果你没有Python3的头文件，在`Ubuntu`上可以通过命令`sudo apt-get install python3-dev`下载。如果你的环境和我一样，Pyhton3的头文件应该位于目录`/usr/include/python3.4/`。*

由`bytesobject.h`可见：
```c
typedef struct {
    PyObject_VAR_HEAD
    Py_hash_t ob_shash;
    char ob_sval[1];

    /* Invariants:
     *     ob_sval contains space for 'ob_size+1' elements.
     *     ob_sval[ob_size] == 0.
     *     ob_shash is the hash of the string or -1 if not computed yet.
     */
} PyBytesObject;
```

这意味着什么？

- Pyhton3中的字节对象对应的内部类型是`PyBytesObject`
- `ob_sval`中存放着整个字符串
- 字符串是以`0x0`（`NULL`）结尾的
- `ob_size `中存放着字符串的长度（`ob_size `在`objects.h`中的宏`PyObject_VAR_HEAD`定义中，后面我们会涉及）

所以，在我们的例子中，如果我们打印字节对象`s`, 将看到以下信息:
- `ob_sval`: 值为`Holberton` --> 十六进制表示的ASCII码如下: `48` `6f` `6c` `62` `65` `72` `74` `6f` `6e` `00`
- `ob_size`: 值为`9`

**也即是说，字符串位于字节对象内部，所以和对象在同一内存区域。**

假如不知道CPython中内置函数`id()`返回的是对象的内存地址，我们如何查找字符串所在区域呢？这种情况下， 我们可以解析内存中的对象。

## 五、从内存中寻找目标字符串 ##

如果想查看内存中的`PyBytesObject`变量， 我们需要写一个C函数，并且用Python调用这个C函数。Python可以通过多种方式调用C函数。我们仅使用最简单的动态库的方式。

### C函数原型 ###
我们要创建的C函数将被Python调用，它的入参是Python对象。该函数将剖析Python对象并找到字符串的地址，以及该对象的其他信息。  

函数原型如下， 其中`p`是指向Python对象的指针:
```c
void print_python_bytes(PyObject *p);
```
### 1. object.h ###
不知你是否注意到，`print_python_bytes`的入参类型不是`PyBytesObject`而是`PyObject`。为社么？让我们尝试从头文件`object.h`中寻找答案：

```c
/* Object and type object interface */

/*
Objects are structures allocated on the heap.  Special rules apply to
the use of objects to ensure they are properly garbage-collected.
Objects are never allocated statically or on the stack; they must be
...
*/
```

- “Python对象不能静态分配或者在栈上分配” --> 这说明了为什么字符串不在栈上。
- “Python对象是在堆上分配的” --> ***等等... 什么？？？我们在堆上寻找字符串但是并没找到啊...***想不明白！我们将在另一篇文章中探讨这个问题。

我们还能从头文件`object.h`中找到什么呢？
```c
/*
...
Objects do not float around in memory; once allocated an object keeps
the same size and address.  Objects that must hold variable-size data
...
*/
```

- “对象在内存中是固定的：对象一经分配就保持固定大小，并且地址也不会再变化” --> 这意味着，如果我们找到对应的字符串（字节对象中的字符串）并修改它，它将永远被改变。
- “一经分配” --> 分配？但是不在堆中？想不明白！我们将在另一篇文章中探讨这个问题。

```c
/*
...
Objects are always accessed through pointers of the type 'PyObject *'.
The type 'PyObject' is a structure that only contains the reference count
and the type pointer.  The actual memory allocated for an object
contains other data that can only be accessed after casting the pointer
to a pointer to a longer structure type.  This longer type must start
with the reference count and type fields; the macro PyObject_HEAD should be
used for this (to accommodate for future changes).  The implementation
of a particular object type can cast the object pointer to the proper
type and back.
...
*/
```
*译者注：下面的意思大概可以类比成，`PyObject` 和 `PyBytesObject`是父类和子类的关系，通过父类只能访问父类的成员变量；如果想访问子类的成员变量，必须把类型转换成子类类型。*

- “对象总是由类型为`PyObject *`的指针访问” --> 这就是函数`print_python_bytes`的入参类型是`PyObject`而不是`PyBytesObject`的原因。
- “每个对象所占用的包含其他信息的实际内存，只能通过转化后的具体的类型指针访问” --> 因此，为了访问类型`PyBytesObject`的所有成员，我们必须把入参`PyObject *`转换成`PyBytesObject *`。这种转换是可行的，因为`PyBytesObject`的起始处是`PyVarObject`, 而`PyVarObject`的起始处又是个`PyObject`：

```c
/* PyObject_VAR_HEAD defines the initial segment of all variable-size
 * container objects.  These end with a declaration of an array with 1
 * element, but enough space is malloc'ed so that the array actually
 * has room for ob_size elements.  Note that ob_size is an element count,
 * not necessarily a byte count.
 */
#define PyObject_VAR_HEAD      PyVarObject ob_base;
#define Py_INVALID_SIZE (Py_ssize_t)-1

/* Nothing is actually declared to be a PyObject, but every pointer to
 * a Python object can be cast to a PyObject*.  This is inheritance built
 * by hand.  Similarly every pointer to a variable-size Python object can,
 * in addition, be cast to PyVarObject*.
 */
typedef struct _object {
    _PyObject_HEAD_EXTRA
    Py_ssize_t ob_refcnt;
    struct _typeobject *ob_type;
} PyObject;

typedef struct {
    PyObject ob_base;
    Py_ssize_t ob_size; /* Number of items in variable part */
} PyVarObject;
```
这里我们看到了在`bytesobject.h`中提到的`ob_size `。

### 2. C函数实现 ###

基于上面了解到的Python知识，我们可以写出打印Python对象的C函数（`bytes.c`）:

```c
#include "Python.h"

/**
 * print_python_bytes - prints info about a Python 3 bytes object
 * @p: a pointer to a Python 3 bytes object
 * 
 * Return: Nothing
 */
void print_python_bytes(PyObject *p)
{
     /* The pointer with the correct type.*/
     PyBytesObject *s;
     unsigned int i;

     printf("[.] bytes object info\n");
     /* casting the PyObject pointer to a PyBytesObject pointer */
     s = (PyBytesObject *)p;
     /* never trust anyone, check that this is actually
        a PyBytesObject object. */
     if (s && PyBytes_Check(s))
     {
          /* a pointer holds the memory address of the first byte
         of the data it points to */
          printf("  address of the object: %p\n", (void *)s);
          /* op_size is in the ob_base structure, of type PyVarObject. */
          printf("  size: %ld\n", s->ob_base.ob_size);
          /* ob_sval is the array of bytes, ending with the value 0:
         ob_sval[ob_size] == 0 */
          printf("  trying string: %s\n", s->ob_sval);
          printf("  address of the data: %p\n", (void *)(s->ob_sval));
          printf("  bytes:");
          /* printing each byte at a time, in case this is not
         a "string". bytes doesn't have to be strings.
         ob_sval contains space for 'ob_size+1' elements.
         ob_sval[ob_size] == 0. */
          for (i = 0; i < s->ob_base.ob_size + 1; i++)
          {
               printf(" %02x", s->ob_sval[i] & 0xff);
          }
          printf("\n");
     }
     /* if this is not a PyBytesObject print an error message */
     else
     {
          fprintf(stderr, "  [ERROR] Invalid Bytes Object\n");
     }
}
```

### 3. Python调用C函数 ###

#### 3.1 创建动态库 ####
如之前所述，我们将在Python脚本中用动态库的方式调用函数。我们可以用下面的命令编译C动态库：
```shell
gcc -Wall -Wextra -pedantic -Werror -std=c99 -shared -Wl,-soname,libPython.so -o libPython.so -fPIC -I/usr/include/python3.4 bytes.c
```
*提示：不要忘记包含Python3头文件目录:`-I/usr/include/python3.4`*

上面的命令将会创建动态库`libPython.so`。

#### 3.2 调用动态库 ####

为了调用动态库`libPython.so`中的函数，我们需要在Python脚本中增加下面的语句:
```python
import ctypes

lib = ctypes.CDLL('./libPython.so')
lib.print_python_bytes.argtypes = [ctypes.py_object]
```
并用下面的方式调用函数:
```python
lib.print_python_bytes(s)
```
#### 3.3 打印Python对象的脚本 ####

下面是用来打印Python字节对象的完整的Python脚本（`main_bytes.py`）:

```python
#!/usr/bin/env python3
'''
Prints:
- the address of the bytes object
- a b"string" (bytes object)
- information about the bytes object
And then:
- reads a char from stdin
- prints the same (or not :)) information again
'''

import sys
import ctypes

lib = ctypes.CDLL('./libPython.so')
lib.print_python_bytes.argtypes = [ctypes.py_object]

s = b"Holberton"
print(hex(id(s)))
print(s)
lib.print_python_bytes(s)

sys.stdin.read(1)

print(hex(id(s)))
print(s)
lib.print_python_bytes(s)
```
让我们执行这个脚本:

```shell
julien@holberton:~/holberton/w/hackthevm1$ ./main_bytes.py 
0x7f04d721b210
b'Holberton'
[.] bytes object info
  address of the object: 0x7f04d721b210
  size: 9
  trying string: Holberton
  address of the data: 0x7f04d721b230
  bytes: 48 6f 6c 62 65 72 74 6f 6e 00
```

不出所料：
- `id()`返回的是对象自身地址（`0x7f04d721b210`）
- 字节对象`s`内部数据的大小(`ob_size`) 是9
- 字节对象`s`的数据是字符串`Holberton`， 十六进制表示的ASCII码如下: `48` `6f` `6c` `62` `65` `72` `74` `6f` `6e` `00`，并且如`bytesobject.h`所言，是以`0x00`结尾的字符串。

好了，我们已经找到字符串的准确地址`0x7f04d721b230`。  

![](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/tumblr_nomr17FFSt1tym3lfo1_400.gif?raw=true)


## 六、替换Python进程的字符串 ##

现在我们已经了解了事情的来龙去脉，可以“暴力”搜索内存区域了。原来替换字符串的Python脚本只搜索堆段和栈段，现在我们让它搜索所有具有读写权限的内存区段。下面是具体的代码(`rw_all.py`):

```python
#!/usr/bin/env python3
'''
Locates and replaces (if we have permission) all occurrences of
an ASCII string in the entire virtual memory of a process.

Usage: ./rw_all.py PID search_string replace_by_string
Where:
- PID is the pid of the target process
- search_string is the ASCII string you are looking to overwrite
- replace_by_string is the ASCII string you want to replace
search_string with
'''

import sys

def print_usage_and_exit():
    print('Usage: {} pid search write'.format(sys.argv[0]))
    exit(1)

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

# try opening the file
try:
    maps_file = open('/proc/{}/maps'.format(pid), 'r')
except IOError as e:
    print("[ERROR] Can not open file {}:".format(maps_filename))
    print("        I/O error({}): {}".format(e.errno, e.strerror))
    exit(1)

for line in maps_file:
    # print the name of the memory region
    sline = line.split(' ')
    name = sline[-1][:-1];
    print("[*] Searching in {}:".format(name))

    # parse line
    addr = sline[0]
    perm = sline[1]
    offset = sline[2]
    device = sline[3]
    inode = sline[4]
    pathname = sline[-1][:-1]

    # check if there are read and write permissions
    if perm[0] != 'r' or perm[1] != 'w':
        print("\t[\x1B[31m!\x1B[m] {} does not have read/write permissions ({})".format(pathname, perm))
        continue

    print("\tpathname = {}".format(pathname))
    print("\taddresses = {}".format(addr))
    print("\tpermisions = {}".format(perm))
    print("\toffset = {}".format(offset))
    print("\tinode = {}".format(inode))

    # get start and end of the memoy region
    addr = addr.split("-")
    if len(addr) != 2: # never trust anyone
        print("[*] Wrong addr format")
        maps_file.close()
        exit(1)
    addr_start = int(addr[0], 16)
    addr_end = int(addr[1], 16)
    print("\tAddr start [{:x}] | end [{:x}]".format(addr_start, addr_end))

    # open and read the memory region
    try:
        mem_file = open(mem_filename, 'rb+')
    except IOError as e:
        print("[ERROR] Can not open file {}:".format(mem_filename))
        print("        I/O error({}): {}".format(e.errno, e.strerror))
        maps_file.close()

    # read the memory region
    mem_file.seek(addr_start)
    region = mem_file.read(addr_end - addr_start)

    # find string
    nb_found = 0;
    try:
        i = region.index(bytes(search_string, "ASCII"))
        while (i):
            print("\t[\x1B[32m:)\x1B[m] Found '{}' at {:x}".format(search_string, i))
            nb_found = nb_found + 1
            # write the new string
        print("\t[:)] Writing '{}' at {:x}".format(write_string, addr_start + i))
            mem_file.seek(addr_start + i)
            mem_file.write(bytes(write_string, "ASCII"))
            mem_file.flush()

            # update our buffer
        region.write(bytes(write_string, "ASCII"), i)

            i = region.index(bytes(search_string, "ASCII"))
    except Exception:
        if nb_found == 0:
            print("\t[\x1B[31m:(\x1B[m] Can't find '{}'".format(search_string))
    mem_file.close()

# close files
maps_file.close()
```

让我们运行这个脚本:

```shell
julien@holberton:~/holberton/w/hackthevm1$ ./main_bytes.py 
0x7f37f1e01210
b'Holberton'
[.] bytes object info
  address of the object: 0x7f37f1e01210
  size: 9
  trying string: Holberton
  address of the data: 0x7f37f1e01230
  bytes: 48 6f 6c 62 65 72 74 6f 6e 00
```

```shell
julien@holberton:~/holberton/w/hackthevm1$ ps aux | grep main_bytes.py | grep -v grep
julien     4713  0.0  0.8  37720  8208 pts/0    S+   18:48   0:00 python3 ./main_bytes.py
julien@holberton:~/holberton/w/hackthevm1$ sudo ./rw_all.py 4713 Holberton "~ Betty ~"
[*] maps: /proc/4713/maps
[*] mem: /proc/4713/mem
[*] Searching in /usr/bin/python3.4:
    [!] /usr/bin/python3.4 does not have read/write permissions (r-xp)
...
[*] Searching in [heap]:
    pathname = [heap]
    addresses = 00e26000-00f11000
    permisions = rw-p
    offset = 00000000
    inode = 0
    Addr start [e26000] | end [f11000]
    [:)] Found 'Holberton' at 8e422
    [:)] Writing '~ Betty ~' at eb4422
...
[*] Searching in :
    pathname = 
    addresses = 7f37f1df1000-7f37f1fa7000
    permisions = rw-p
    offset = 00000000
    inode = 0
    Addr start [7f37f1df1000] | end [7f37f1fa7000]
    [:)] Found 'Holberton' at 10230
    [:)] Writing '~ Betty ~' at 7f37f1e01230
...
[*] Searching in [stack]:
    pathname = [stack]
    addresses = 7ffdc3d0c000-7ffdc3d2d000
    permisions = rw-p
    offset = 00000000
    inode = 0
    Addr start [7ffdc3d0c000] | end [7ffdc3d2d000]
    [:(] Can't find 'Holberton'
...
julien@holberton:~/holberton/w/hackthevm1$ 
```

现在我们按下回车键继续运行脚本`main_bytes.py`:
```shell
julien@holberton:~/holberton/w/hackthevm1$ ./main_bytes.py 
0x7f37f1e01210
b'Holberton'
[.] bytes object info
  address of the object: 0x7f37f1e01210
  size: 9
  trying string: Holberton
  address of the data: 0x7f37f1e01230
  bytes: 48 6f 6c 62 65 72 74 6f 6e 00

0x7f37f1e01210
b'~ Betty ~'
[.] bytes object info
  address of the object: 0x7f37f1e01210
  size: 9
  trying string: ~ Betty ~
  address of the data: 0x7f37f1e01230
  bytes: 7e 20 42 65 74 74 79 20 7e 00
julien@holberton:~/holberton/w/hackthevm1$ 
```

很好， 成功把字符串`Holberton`替换成`~ Betty ~`。  

![](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/giphy-3.gif?raw=true)

## 七、下节预告 ##
本文我们成功修改了正在运行的Python3脚本中的字符串，但是仍有几个问题有待解答：
- 堆中的字符串`Holberton`是干什么的？
- Python3如何在堆以外分配内存？
- 如果Python3没有使用堆，头文件`object.h`中所说的“对象是堆上的结构”又该如何解释？

下一篇文章我们将一一解答上面的问题。

## 八、继续阅读 ##

- 第一篇:[虚拟内存探究 -- 第一篇:C strings & /proc](https://tech.coderhuo.tech/posts/Virtual_Memory_C_strings_proc/)
- 第二篇:[虚拟内存探究 -- 第二篇:Python 字节](https://tech.coderhuo.tech/posts/Virtual_Memory_python_bytes/)
- 第三篇:[虚拟内存探究 -- 第三篇:一步一步画虚拟内存图](https://tech.coderhuo.tech/posts/Virtual_Memory_drawing_VM_diagram/)
- 第四篇:[虚拟内存探究 -- 第四篇:malloc, heap & the program break](https://tech.coderhuo.tech/posts/Virtual_Memory_malloc_and_heap/)
- 第五篇:[虚拟内存探究 -- 第五篇:The Stack, registers and assembly code](https://tech.coderhuo.tech/posts/Virtual_Memory_malloc_and_heap_stack_and_register/)

## 九、原文链接 ##
[Hack The Virtual Memory: Python bytes](https://blog.holbertonschool.com/hack-the-virtual-memory-python-bytes/)
