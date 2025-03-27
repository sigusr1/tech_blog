---
title: "gcc编译临时文件存放路径"
date: 2017-07-02  
categories: [计算机基础]  
tags: [gcc, 临时文件] 
---

代码编译的时候，编译服务器莫名其妙的报以下错误:

```console
fatal error: error writing to /tmp/ccGjoKTF.s:No space left on device
```
  
奇怪了，编译脚本中并没有往tmp目录写文件呀！  

仔细看了下错误信息，这个ccGjoKTF.s应该是编译过程的中间文件，文件名是随机值。

然而makefile中并未要求保留汇编代码。  

写了个demo，用strace（strace gcc test）跟踪了下，发现gcc不仅把汇编代码（*.s*）写到了tmp目录，也把二进制文件（*.o*）写到了tmp目录，并且编译完成自动删除临时文件。  

如果在编译的时候使用-S或者-C选项，则会把对应的中间文件保存在当前目录，而不是tmp目录。  

如果在编译的时候使用`save-temps`选项，也会把中间产物保存在当前目录，并且编译完成不删除临时文件。

[查资料](https://stackoverflow.com/questions/4874735/tmp-folder-and-gcc)发现原来gcc默认把编译过程中的中间文件写到tmp目录。

如果不想让gcc把中间文件写到tmp目录，可以设置环境变量TMPDIR。  
比如可以在makefile中设置到当前目录:

```console
export TMPDIR=$(pwd)
```

至于tmp目录空间不足，`ls -l` 一看，竟然是有些项目的makefile写的有问题，编译完成后残留了很多文件。  


**参考文档：**  
[https://stackoverflow.com/questions/4874735/tmp-folder-and-gcc](https://stackoverflow.com/questions/4874735/tmp-folder-and-gcc)  
[https://gcc.gnu.org/onlinedocs/gcc-5.4.0/gcc.pdf](https://gcc.gnu.org/onlinedocs/gcc-5.4.0/gcc.pdf)
