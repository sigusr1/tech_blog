---
title: "可怕的extern关键字"  
date:   2017-04-03  
categories: [编程语言]  
tags: 【extern, 计算机基础] 
---

实际项目中看到有人用extern关键字来声明外部函数，这是一个很不好的行为。


## 一、不利之处

如果函数原型改变的话，每个extern声明的地方都要改一遍。  
如果有地方没改到呢？  
我们通过一个例子来看下悲剧是怎么发生的。 

## 二、例子

头文件api.h中声明了一个函数func：

```c
#ifndef __API_H__
#define __API_H__

void func(int a);

#endif

```

文件api.c中实现了func函数:

```c
#include <stdio.h>
void func(int a)
{
    printf("hello world.[%d]\n", a);
}
```

文件bad_test.c中调用了func函数，但是**func被重新声明成无参数的了**
```c
#include <stdio.h>
extern void func();
int main(int argc,char *argv[])
{
    func();
    return 0;
}
```

编译运行结果如下:

```console
gcc -Wall bad_test.c api.c
./a.out 
hello world.[1]
```

## 三、分析

1. 编译的时候即时加了**-Wall**选项也没有编译警告。  
   这是因为编译是以源文件为单位的，在bad_test.c中func的声明是无参的，调用也是按无参调用的，所以编译器不会告警。  
   **如果把extern声明去掉，编译器好歹还能给个“函数未显式定义”的警告。**
2. 链接的时候也没报错？  
  这是因为，在C语言中，编译出来的函数符号表是不带参数的，如下所示, func在符号表中就是字符串func。  
  这也是为啥C语言不能做编译时多态的原因。  
  所以，别指望在链接的时候报错。

    ```console
    gcc -c api.c 
    nm api.o 
    0000000000000000 T func
                    U printf
    ```
3. 程序竟然还能运行？？？  
  程序输出了1， 但是这个1是哪里来的呢？  
  我们先看看下面这一系列输入输出：  
	```console
	$./a.out 
	hello world.[1]
	$./a.out a 
	hello world.[2]
	$./a.out a b
	hello world.[3] 
	$./a.out a b c
	hello world.[4]
	$./a.out a b c d
	hello world.[5]
	```
  看明白了吗？ **竟然把argc的值打了出来!!!**  
  程序运行的时候，调用的肯定还是带参数的func函数，但是这个参数从哪里来呢？  
  考虑到默认从右到左的压栈顺序，处于栈顶的argc被取出来塞给func函数作为入参了，所以func打印出来的是argc的值。  
  **都这样了，接下来离各种莫名其妙的异常还远吗？  
  这种问题定位起来会搞死人的。**

## 四、正确做法

  建议通过头文件引用的方式来使用外部函数，如果bad_test.c写成下面这样，编译就无法通过，可以有效阻止错误蔓延。 
```c
#include <stdio.h>
#include "api.h"

int main(int argc,char *argv[])
{
    func();
    return 0;
}
```

编译报错:
```console
gcc bad_test.c api.c -c
bad_test.c: In function ‘main’:
bad_test.c:6:5: error: too few arguments to function ‘func’
     func();
     ^
In file included from bad_test.c:2:0:
api.h:3:6: note: declared here
 void func(int a);
      ^
```
