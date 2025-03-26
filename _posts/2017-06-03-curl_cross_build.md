---
title: "curl交叉编译方法"
date: 2017-06-04 23:40:18 +0800
categories: [开源软件]
tags: [curl]
---

本文主要介绍arm平台curl交叉编译方法。

基于curl + openssl + zlib 的方式组建arm上的https客户端，其中curl作为http客户端，openssl提供https支持，zlib负责处理gzip压缩的http报文。


编译目录结构如下:
- curl-7.29.0、openssl-OpenSSL\_1\_0\_2g和zlib-1.2.8是待编译的源码
- libs用来存放编译出来的库以及头文件

![目录结构图](https://github.com/sigusr1/blog_assets/blob/master/2017-06-03-curl_cross_build/curl_dir_tree.jpg?raw=true)

编译所需源码请从官网下载。

下面的例子中https_framework的全路径是/home/test/https\_framework。

## 一、编译zlib：(版本zlib-1.2.8)
1. 执行下面的命令生成makefile:
	```console
	./configure --prefix=/home/test/https_framework/libs/zlib
	```
2. 由于zlib在生成makefile的时候不支持修改编译器选项，只好在makefile中修改：
	```makefile
	AR=ar   
	RANLIB=ranlib
	```

	改成:

	```makefile
	AR=arm-linux-gnueabihf-ar
	RANLIB=arm-linux-gnueabihf-ranlib
	```  
	然后全文搜索一下gcc, 全部替换成arm-linux-gnueabihf-gcc  
3. 执行make 命令编译  
4. 执行make install命令安装  
5. 生成的头文件、库都在/home/test/https_framework/libs/zlib目录下


## 二、编译openssl：（版本openssl-OpenSSL_1_0_2g）
1. 执行下面的命令生成makefile:
	```console
	./Configure linux-elf-arm linux:'arm-linux-gnueabihf-gcc' --prefix=/home/test/https_framework/libs/openssl
	```

	x86用下面的命令
	```console
	./Configure linux-x86_64 --prefix=/home/test/https_framework/libs/openssl
	```

	如果提示编译前先make depend，可忽略。
2. 在makefile中做如下修改：

	```makefile
	RANLIB= /usr/bin/ranlib  -->  RANLIB= arm-linux-gnueabihf-ranlib  
    NM= nm                   -->  NM= arm-linux-gnueabihf-nm
	```
3. 执行make 命令编译  
4. 执行make install命令安装  
5. 生成的头文件、库都在/home/test/https_framework/libs/openssl 目录下

## 三、编译curl：（版本curl-7.29.0）
1. 执行下面的命令生成makefile:
	```console
	./configure --with-ssl=/home/test/https_framework/libs/openssl --with-zlib=/home/test/https_framework/libs/zlib --host=arm-linux-gnueabihf --target=arm-linux-gnueabihf --prefix=/home/test/https_framework/libs/curl/ --enable-shared=0
	```

	其中  
	--enable-shared=0 说明只编译静态库  
	--with-ssl= 指定openssl的安装路径  
	--with-zlib= 指定zlib的安装路径
2. configure执行完毕后输出配置信息，注意查看openssl和zlib是否使能：


	curl version: 7.29.0  
	**SSL support: enabled (OpenSSL)**  
	SSH support: no (--with-libssh2)  
	**zlib support: enabled**

3. 执行make 命令编译  
4. 执行make install命令安装  
5. 生成的头文件、库都在/home/test/https_framework/libs/curl/ 目录下  
6. 编译x86平台下的库会默认开启openssl和zlib选项，可以通过\-\-without-ssl和\-\-without-zlib 分别禁止掉  
