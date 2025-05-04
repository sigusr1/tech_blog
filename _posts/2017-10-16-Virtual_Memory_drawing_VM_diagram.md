---
title:  "虚拟内存探究 -- 第三篇:一步一步画虚拟内存图"  
date:   2017-10-16
categories: [操作系统]
tags: [虚拟内存, 翻译]
---


* content
{:toc}
这是虚拟内存系列文章的第三篇。  
前面我们提到在进程的虚拟内存中可以找到哪些东西，以及在哪里去找。  
本文我们将通过打印程序中不同元素内存地址的方式，一步一步细化下面的虚拟内存图：

![虚拟内存示意图](/virtual_memory/virtual_memory.jpg?raw=true)






## 一、预备知识 ##

为了方便理解本文，你需要具备以下知识：  

- C语言基础
- 一些汇编知识（非必需）
- 了解Linux的文件系统和shell命令
- 了解文件`/proc/[pid]/maps`（可参阅`man proc`或[《虚拟内存探究 -- 第一篇:C strings & /proc》](https://tech.coderhuo.tech/posts/Virtual_Memory_C_strings_proc/)中的相关介绍）

## 二、实验环境 ##
所有的脚本和程序都在下面的环境中测试过：  

- Ubuntu 
	- Linux ubuntu 4.4.0-31-generic #50~14.04.1-Ubuntu SMP Wed Jul 13 01:07:32 UTC 2016 x86_64 x86_64 x86_64 GNU/Linux
	- **下面提到的都是基于本系统的，其他系统可能会有差异**
- gcc  
	- gcc (Ubuntu 4.8.4-2ubuntu1~14.04.3) 4.8.4
- objdump  
	- GNU objdump (GNU Binutils for Ubuntu) 2.24
- udcli
	- udis86 1.7.2
- bc
	- bc 1.06.95  

## 三、栈 ##

首先我们想确认的是栈在虚拟内存中的位置。  
我们知道，C语言中的局部变量位于栈上。如果我们打印一个局部变量的内存地址，就可以根据这个地址寻找栈在虚拟内存中的位置。我们使用下面的程序（`main-1.c`）寻找栈的位置：
```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * main - print locations of various elements
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    int a;

    printf("Address of a: %p\n", (void *)&a);
    return (EXIT_SUCCESS);
}
```
编译运行:
```shell
julien@holberton:~/holberton/w/hackthevm2$ gcc -Wall -Wextra -pedantic -Werror main-0.c -o 0
julien@holberton:~/holberton/w/hackthevm2$ ./0
Address of a: 0x7ffd14b8bd9c
julien@holberton:~/holberton/w/hackthevm2$ 
```
这是我们和其他元素的地址相比的第一个参照地址。

## 四、堆 ##

我们使用`malloc`为变量分配的内存位于堆上。  
可以在程序中添加一个使用`malloc`的语句，借此查看`malloc`返回的地址位于哪里。如下所示(`main-1.c`)：

```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * main - print locations of various elements
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    int a;
    void *p;

    printf("Address of a: %p\n", (void *)&a);
    p = malloc(98);
    if (p == NULL)
    {
        fprintf(stderr, "Can't malloc\n");
        return (EXIT_FAILURE);
    }
    printf("Allocated space in the heap: %p\n", p);
    return (EXIT_SUCCESS);
}
```
编译运行:

```shell
julien@holberton:~/holberton/w/hackthevm2$ gcc -Wall -Wextra -pedantic -Werror main-1.c -o 1
julien@holberton:~/holberton/w/hackthevm2$ ./1 
Address of a: 0x7ffd4204c554
Allocated space in the heap: 0x901010
julien@holberton:~/holberton/w/hackthevm2$ 
```

至此可以确定堆(`0x901010`)在栈(`0x7ffd4204c554`)的下面。据此可以画出如下的内存图:

![内存布局示意图](/virtual_memory/virtual_memory_stack_heap.png?raw=true)

## 五、可执行程序的位置 ##
可执行程序也会被加载到虚拟内存中。如果我们打印`main`函数的地址，就可以知道可执行程序在虚拟内存中相对于堆栈的位置。  
我们看看它是否真的在堆的下面(`main-2.c`)：

```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * main - print locations of various elements
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    int a;
    void *p;

    printf("Address of a: %p\n", (void *)&a);
    p = malloc(98);
    if (p == NULL)
    {
        fprintf(stderr, "Can't malloc\n");
        return (EXIT_FAILURE);
    }
    printf("Allocated space in the heap: %p\n", p);
    printf("Address of function main: %p\n", (void *)main);
    return (EXIT_SUCCESS);
}
```
编译并运行:

```shell
julien@holberton:~/holberton/w/hackthevm2$ gcc -Wall -Wextra -Werror main-2.c -o 2
julien@holberton:~/holberton/w/hackthevm2$ ./2 
Address of a: 0x7ffdced37d74
Allocated space in the heap: 0x2199010
Address of function main: 0x40060d
julien@holberton:~/holberton/w/hackthevm2$ 
```

不出所料，可执行程序(`0x40060d`)果然位于堆的下面（`0x2199010`)。但我们必须确认这是我们的程序所在地址，而不是指向其他地址的指针的地址。我们借助工具[objdump](https://en.wikipedia.org/wiki/Objdump)来查看函数`main`的内存地址：
```shell
julien@holberton:~/holberton/w/hackthevm2$ objdump -M intel -j .text -d 2 | grep '<main>:' -A 5
000000000040060d <main>:
  40060d:   55                      push   rbp
  40060e:   48 89 e5                mov    rbp,rsp
  400611:   48 83 ec 10             sub    rsp,0x10
  400615:   48 8d 45 f4             lea    rax,[rbp-0xc]
  400619:   48 89 c6                mov    rsi,rax
```

`000000000040060d <main>` -->该地址和我们打印出来的`0x40060d`一致。如果你还持有怀疑，可以打印出该地址开始的几个字节，看看和`objdump`输出的是否一致(`main-3.c`)：

```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * main - print locations of various elements
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(void)
{
    int a;
    void *p;
    unsigned int i;

    printf("Address of a: %p\n", (void *)&a);
    p = malloc(98);
    if (p == NULL)
    {
        fprintf(stderr, "Can't malloc\n");
        return (EXIT_FAILURE);
    }
    printf("Allocated space in the heap: %p\n", p);
    printf("Address of function main: %p\n", (void *)main);
    printf("First bytes of the main function:\n\t");
    for (i = 0; i < 15; i++)
    {
        printf("%02x ", ((unsigned char *)main)[i]);
    }
    printf("\n");
    return (EXIT_SUCCESS);
}
```


```shell
julien@holberton:~/holberton/w/hackthevm2$ gcc -Wall -Wextra -Werror main-3.c -o 3
julien@holberton:~/holberton/w/hackthevm2$ objdump -M intel -j .text -d 3 | grep '<main>:' -A 5
000000000040064d <main>:
  40064d:   55                      push   rbp
  40064e:   48 89 e5                mov    rbp,rsp
  400651:   48 83 ec 10             sub    rsp,0x10
  400655:   48 8d 45 f0             lea    rax,[rbp-0x10]
  400659:   48 89 c6                mov    rsi,rax
julien@holberton:~/holberton/w/hackthevm2$ ./3 
Address of a: 0x7ffeff0f13b0
Allocated space in the heap: 0x8b3010
Address of function main: 0x40064d
First bytes of the main function:
    55 48 89 e5 48 83 ec 10 48 8d 45 f0 48 89 c6 
julien@holberton:~/holberton/w/hackthevm2$ echo "55 48 89 e5 48 83 ec 10 48 8d 45 f0 48 89 c6" | udcli -64 -x -o 40064d
000000000040064d 55               push rbp                
000000000040064e 4889e5           mov rbp, rsp            
0000000000400651 4883ec10         sub rsp, 0x10           
0000000000400655 488d45f0         lea rax, [rbp-0x10]     
0000000000400659 4889c6           mov rsi, rax            
julien@holberton:~/holberton/w/hackthevm2$
```
*提示:可以在这里下载反汇编工具[Udis86 Disassembler Library](http://udis86.sourceforge.net/)*  

由此可见，我们打印出来的地址和内容都是一致的。因此可以确认这个地址就是我们的`main`函数。

更新后内存布局示意图如下:  
![](/virtual_memory/virtual_memory_stack_heap_executable.png?raw=true)



## 六、命令行参数和环境变量 ##

`main`函数可接收以下参数:

- 命令行参数
	- `main`函数的第一个参数(通常写作`argc`或`ac`)代表命令行参数的个数
	-  `main`函数的第二个参数(通常写作`argv`或`av`)是一个字符串指针数组，数组的每个成员都指向一个命令行参数（C字符串）
- 环境变量参数
	- `main`函数的第三个参数(通常写作`env`或`envp`)也是一个字符串指针数组，数组的每个成员都指向一个环境变量（C字符串）

我们看下这些元素位于虚拟内存的哪部分（`main-4.c`）:

```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * main - print locations of various elements
 *
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS
 */
int main(int ac, char **av, char **env)
{
        int a;
        void *p;
        int i;

        printf("Address of a: %p\n", (void *)&a);
        p = malloc(98);
        if (p == NULL)
        {
                fprintf(stderr, "Can't malloc\n");
                return (EXIT_FAILURE);
        }
        printf("Allocated space in the heap: %p\n", p);
        printf("Address of function main: %p\n", (void *)main);
        printf("First bytes of the main function:\n\t");
        for (i = 0; i < 15; i++)
        {
                printf("%02x ", ((unsigned char *)main)[i]);
        }
        printf("\n");
        printf("Address of the array of arguments: %p\n", (void *)av);
        printf("Addresses of the arguments:\n\t");
        for (i = 0; i < ac; i++)
        {
                printf("[%s]:%p ", av[i], av[i]);
        }
        printf("\n");
        printf("Address of the array of environment variables: %p\n", (void *)env);
    printf("Address of the first environment variable: %p\n", (void *)(env[0]));
        return (EXIT_SUCCESS);
}
```

编译并运行:
```shell
julien@holberton:~/holberton/w/hackthevm2$ gcc -Wall -Wextra -Werror main-4.c -o 4
julien@holberton:~/holberton/w/hackthevm2$ ./4 Hello Holberton School!
Address of a: 0x7ffe7d6d8da0
Allocated space in the heap: 0xc8c010
Address of function main: 0x40069d
First bytes of the main function:
    55 48 89 e5 48 83 ec 30 89 7d ec 48 89 75 e0 
Address of the array of arguments: 0x7ffe7d6d8e98
Addresses of the arguments:
    [./4]:0x7ffe7d6da373 [Hello]:0x7ffe7d6da377 [Holberton]:0x7ffe7d6da37d [School!]:0x7ffe7d6da387 
Address of the array of environment variables: 0x7ffe7d6d8ec0

/* 译者注：根据上面的程序，应该不会有下面的输出的，但作者接下来的讨论用到了这部分，所以保留这部分 */
Address of the first environment variables:
    [0x7ffe7d6da38f]:"XDG_VTNR=7"
    [0x7ffe7d6da39a]:"XDG_SESSION_ID=c2"
    [0x7ffe7d6da3ac]:"CLUTTER_IM_MODULE=xim"
julien@holberton:~/holberton/w/hackthevm2$ 
```

在这之前我们知道命令行参数和环境变量都位于栈上面，但是不知道二者的相对位置。现在可以确定二者的相对位置：

`stack` (`0x7ffe7d6d8da0`) < `argv` (`0x7ffe7d6d8e98`) < `env` (`0x7ffe7d6d8ec0`) < `arguments` (从 `0x7ffe7d6da373` 到 `0x7ffe7d6da387` + `8` (`8` = 字符串`school`大小 + `1` 字节的字符串结束符`\0`)) < `environment variables` (起始地址是`0x7ffe7d6da38f`)。

事实上，我们可以看出所有的命令行参数在内存中都是相邻的，并且和环境变量也是相邻的（从`0x7ffe7d6da387` + `8` = `0x7ffe7d6da38f`可以看出）。

### 6.1 数组变量argv和env是相邻的吗？ ###

数组变量`argv`有5个元素（4个是从命令行输入的，另一个是空元素`NULL` -- `argv`总是以NULL结束，以此标记数组的结尾）。`argv`的每个元素都是指向`char`类型的指针，由于我们是64位系统，所以一个指针是8个字节（可通过C语言中的操作符`sizeof()`获取指针大小）。因此数组`argv`占用 `5 * 8 = 40`字节，也就是十六进制的`0x28`。将`0x28`加到`argv`的起始地址`0x7ffe7d6d8e98`, 得到`0x7ffe7d6d8ec0`(**也就是`env`的地址**)！  

**因此，数组变量argv和env在内存中是相邻的！**

### 6.2 第一个命令行参数紧挨着数组变量env吗？ ###

为了回答这个问题，我们需要确认数组`env`的大小。数组`env`也是以`NULL`指针结束，基于此，我们可以遍历数组`env`以确定数组大小，代码如下所示（`main-5.c`）：

```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**                                                                                                      
 * main - print locations of various elements                                                            
 *                                                                                                       
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS                                      
 */
int main(int ac, char **av, char **env)
{
     int a;
     void *p;
     int i;
     int size;

     printf("Address of a: %p\n", (void *)&a);
     p = malloc(98);
     if (p == NULL)
     {
          fprintf(stderr, "Can't malloc\n");
          return (EXIT_FAILURE);
     }
     printf("Allocated space in the heap: %p\n", p);
     printf("Address of function main: %p\n", (void *)main);
     printf("First bytes of the main function:\n\t");
     for (i = 0; i < 15; i++)
     {
          printf("%02x ", ((unsigned char *)main)[i]);
     }
     printf("\n");
     printf("Address of the array of arguments: %p\n", (void *)av);
     printf("Addresses of the arguments:\n\t");
     for (i = 0; i < ac; i++)
     {
          printf("[%s]:%p ", av[i], av[i]);
     }
     printf("\n");
     printf("Address of the array of environment variables: %p\n", (void *)env);
     printf("Address of the first environment variables:\n");
     for (i = 0; i < 3; i++)
     {
          printf("\t[%p]:\"%s\"\n", env[i], env[i]);
     }
     /* size of the env array */
     i = 0;
     while (env[i] != NULL)
     {
          i++;
     }
     i++; /* the NULL pointer */
     size = i * sizeof(char *);
     printf("Size of the array env: %d elements -> %d bytes (0x%x)\n", i, size, size);
     return (EXIT_SUCCESS);
}
```
编译运行：

```shell
julien@holberton:~/holberton/w/hackthevm2$ ./5 Hello Betty Holberton!
Address of a: 0x7ffc77598acc
Allocated space in the heap: 0x2216010
Address of function main: 0x40069d
First bytes of the main function:
    55 48 89 e5 48 83 ec 40 89 7d dc 48 89 75 d0 
Address of the array of arguments: 0x7ffc77598bc8
Addresses of the arguments:
    [./5]:0x7ffc7759a374 [Hello]:0x7ffc7759a378 [Betty]:0x7ffc7759a37e [Holberton!]:0x7ffc7759a384 
Address of the array of environment variables: 0x7ffc77598bf0
Address of the first environment variables:
    [0x7ffc7759a38f]:"XDG_VTNR=7"
    [0x7ffc7759a39a]:"XDG_SESSION_ID=c2"
    [0x7ffc7759a3ac]:"CLUTTER_IM_MODULE=xim"
Size of the array env: 62 elements -> 496 bytes (0x1f0)


julien@holberton:~/holberton/w/hackthevm2$ bc
bc 1.06.95
Copyright 1991-1994, 1997, 1998, 2000, 2004, 2006 Free Software Foundation, Inc.
This is free software with ABSOLUTELY NO WARRANTY.
For details type `warranty'. 
obase=16
ibase=16
1F0+7FFC77598BF0
7FFC77598DE0
quit
julien@holberton:~/holberton/w/hackthevm2$ 
```
`0x1F0 + 0x7FFC77598BF0 = 0x7FFC77598DE0`, 该值仍然小于第一个命令行参数的存储地址`0x7ffc7759a374`。**所以答案是否定的！**  

至此，我们可以画出如下所示的内存布局图(注意哪些是相邻的，哪些是不相邻的)：  

*译者注：下图中的`argv array`指的是变量`argv`的地址，'env array'指的是变量`env`的地址。*

![](/virtual_memory/virtual_memory_args_env.png?raw=true)



## 七、栈真的是向下生长吗？ ##
可以通过函数调用来确认这个问题。**如果栈真的向下生长，调用函数中的变量地址应该大于被调用函数中的变量地址**（`main-6.c`）：

```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**                                                                                                      
 * f - print locations of various elements                                                               
 *                                                                                                       
 * Returns: nothing                                                                                      
 */
void f(void)
{
     int a;
     int b;
     int c;

     a = 98;
     b = 1024;
     c = a * b;
     printf("[f] a = %d, b = %d, c = a * b = %d\n", a, b, c);
     printf("[f] Adresses of a: %p, b = %p, c = %p\n", (void *)&a, (void *)&b, (void *)&c);
}

/**                                                                                                      
 * main - print locations of various elements                                                            
 *                                                                                                       
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS                                      
 */
int main(int ac, char **av, char **env)
{
     int a;
     void *p;
     int i;
     int size;

     printf("Address of a: %p\n", (void *)&a);
     p = malloc(98);
     if (p == NULL)
     {
          fprintf(stderr, "Can't malloc\n");
          return (EXIT_FAILURE);
     }
     printf("Allocated space in the heap: %p\n", p);
     printf("Address of function main: %p\n", (void *)main);
     printf("First bytes of the main function:\n\t");
     for (i = 0; i < 15; i++)
     {
          printf("%02x ", ((unsigned char *)main)[i]);
     }
     printf("\n");
     printf("Address of the array of arguments: %p\n", (void *)av);
     printf("Addresses of the arguments:\n\t");
     for (i = 0; i < ac; i++)
     {
          printf("[%s]:%p ", av[i], av[i]);
     }
     printf("\n");
     printf("Address of the array of environment variables: %p\n", (void *)env);
     printf("Address of the first environment variables:\n");
     for (i = 0; i < 3; i++)
     {
          printf("\t[%p]:\"%s\"\n", env[i], env[i]);
     }
     /* size of the env array */
     i = 0;
     while (env[i] != NULL)
     {
          i++;
     }
     i++; /* the NULL pointer */
     size = i * sizeof(char *);
     printf("Size of the array env: %d elements -> %d bytes (0x%x)\n", i, size, size);
     f();
     return (EXIT_SUCCESS);
}
```
编译运行:

```shell
julien@holberton:~/holberton/w/hackthevm2$ gcc -Wall -Wextra -Werror main-6.c -o 6
julien@holberton:~/holberton/w/hackthevm2$ ./6
Address of a: 0x7ffdae53ea4c
Allocated space in the heap: 0xf32010
Address of function main: 0x4006f9
First bytes of the main function:
    55 48 89 e5 48 83 ec 40 89 7d dc 48 89 75 d0 
Address of the array of arguments: 0x7ffdae53eb48
Addresses of the arguments:
    [./6]:0x7ffdae54038b 
Address of the array of environment variables: 0x7ffdae53eb58
Address of the first environment variables:
    [0x7ffdae54038f]:"XDG_VTNR=7"
    [0x7ffdae54039a]:"XDG_SESSION_ID=c2"
    [0x7ffdae5403ac]:"CLUTTER_IM_MODULE=xim"
Size of the array env: 62 elements -> 496 bytes (0x1f0)
[f] a = 98, b = 1024, c = a * b = 100352
[f] Adresses of a: 0x7ffdae53ea04, b = 0x7ffdae53ea08, c = 0x7ffdae53ea0c
julien@holberton:~/holberton/w/hackthevm2$ 
```
`main`函数中变量`a`的地址`0x7ffdae53ea4c`大于被调用函数`f`中变量`a`的地址`0x7ffdae53ea04 `。所以，**栈确实是向下生长的！**


至此，我们可以画出如下所示的内存布局图(注意栈的生长方向)：

![](/virtual_memory/virtual_memory_stack.png?raw=true)

## 八、/proc ##

我们通过`/proc/[pid]/maps`（可参阅`man proc`或[《虚拟内存探究 -- 第一篇:C strings & /proc》](https://tech.coderhuo.tech/posts/Virtual_Memory_C_strings_proc/)中的相关介绍）再次确认之前得到的结论是否正确。


我们在(`main-6.c`)的基础上添加个`getchar()`函数以便暂停程序的执行，有时间查看`/proc`(`main-7.c`)：

```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**                                                                                                      
 * f - print locations of various elements                                                               
 *                                                                                                       
 * Returns: nothing                                                                                      
 */
void f(void)
{
     int a;
     int b;
     int c;

     a = 98;
     b = 1024;
     c = a * b;
     printf("[f] a = %d, b = %d, c = a * b = %d\n", a, b, c);
     printf("[f] Adresses of a: %p, b = %p, c = %p\n", (void *)&a, (void *)&b, (void *)&c);
}

/**                                                                                                      
 * main - print locations of various elements                                                            
 *                                                                                                       
 * Return: EXIT_FAILURE if something failed. Otherwise EXIT_SUCCESS                                      
 */
int main(int ac, char **av, char **env)
{
     int a;
     void *p;
     int i;
     int size;

     printf("Address of a: %p\n", (void *)&a);
     p = malloc(98);
     if (p == NULL)
     {
          fprintf(stderr, "Can't malloc\n");
          return (EXIT_FAILURE);
     }
     printf("Allocated space in the heap: %p\n", p);
     printf("Address of function main: %p\n", (void *)main);
     printf("First bytes of the main function:\n\t");
     for (i = 0; i < 15; i++)
     {
          printf("%02x ", ((unsigned char *)main)[i]);
     }
     printf("\n");
     printf("Address of the array of arguments: %p\n", (void *)av);
     printf("Addresses of the arguments:\n\t");
     for (i = 0; i < ac; i++)
     {
          printf("[%s]:%p ", av[i], av[i]);
     }
     printf("\n");
     printf("Address of the array of environment variables: %p\n", (void *)env);
     printf("Address of the first environment variables:\n");
     for (i = 0; i < 3; i++)
     {
          printf("\t[%p]:\"%s\"\n", env[i], env[i]);
     }
     /* size of the env array */
     i = 0;
     while (env[i] != NULL)
     {
          i++;
     }
     i++; /* the NULL pointer */
     size = i * sizeof(char *);
     printf("Size of the array env: %d elements -> %d bytes (0x%x)\n", i, size, size);
     f();
     getchar();
     return (EXIT_SUCCESS);
}
```
编译执行:

```shell
julien@holberton:~/holberton/w/hackthevm2$ gcc -Wall -Wextra -Werror main-7.c -o 7
julien@holberton:~/holberton/w/hackthevm2$ ./7 Rona is a Legend SRE
Address of a: 0x7fff16c8146c
Allocated space in the heap: 0x2050010
Address of function main: 0x400739
First bytes of the main function:
    55 48 89 e5 48 83 ec 40 89 7d dc 48 89 75 d0 
Address of the array of arguments: 0x7fff16c81568
Addresses of the arguments:
    [./7]:0x7fff16c82376 [Rona]:0x7fff16c8237a [is]:0x7fff16c8237f [a]:0x7fff16c82382 [Legend]:0x7fff16c82384 [SRE]:0x7fff16c8238b 
Address of the array of environment variables: 0x7fff16c815a0
Address of the first environment variables:
    [0x7fff16c8238f]:"XDG_VTNR=7"
    [0x7fff16c8239a]:"XDG_SESSION_ID=c2"
    [0x7fff16c823ac]:"CLUTTER_IM_MODULE=xim"
Size of the array env: 62 elements -> 496 bytes (0x1f0)
[f] a = 98, b = 1024, c = a * b = 100352
[f] Adresses of a: 0x7fff16c81424, b = 0x7fff16c81428, c = 0x7fff16c8142c
```

查看`/proc/[pid]/maps`:

```shell
julien@holberton:~$ ps aux | grep "./7" | grep -v grep
julien     5788  0.0  0.0   4336   628 pts/8    S+   18:04   0:00 ./7 Rona is a Legend SRE
julien@holberton:~$ cat /proc/5788/maps
00400000-00401000 r-xp 00000000 08:01 171828                             /home/julien/holberton/w/hackthevm2/7
00600000-00601000 r--p 00000000 08:01 171828                             /home/julien/holberton/w/hackthevm2/7
00601000-00602000 rw-p 00001000 08:01 171828                             /home/julien/holberton/w/hackthevm2/7
02050000-02071000 rw-p 00000000 00:00 0                                  [heap]
7f68caa1c000-7f68cabd6000 r-xp 00000000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f68cabd6000-7f68cadd6000 ---p 001ba000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f68cadd6000-7f68cadda000 r--p 001ba000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f68cadda000-7f68caddc000 rw-p 001be000 08:01 136253                     /lib/x86_64-linux-gnu/libc-2.19.so
7f68caddc000-7f68cade1000 rw-p 00000000 00:00 0 
7f68cade1000-7f68cae04000 r-xp 00000000 08:01 136229                     /lib/x86_64-linux-gnu/ld-2.19.so
7f68cafe8000-7f68cafeb000 rw-p 00000000 00:00 0 
7f68cafff000-7f68cb003000 rw-p 00000000 00:00 0 
7f68cb003000-7f68cb004000 r--p 00022000 08:01 136229                     /lib/x86_64-linux-gnu/ld-2.19.so
7f68cb004000-7f68cb005000 rw-p 00023000 08:01 136229                     /lib/x86_64-linux-gnu/ld-2.19.so
7f68cb005000-7f68cb006000 rw-p 00000000 00:00 0 
7fff16c62000-7fff16c83000 rw-p 00000000 00:00 0                          [stack]
7fff16d07000-7fff16d09000 r--p 00000000 00:00 0                          [vvar]
7fff16d09000-7fff16d0b000 r-xp 00000000 00:00 0                          [vdso]
ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]
julien@holberton:~$ 
```
通过`maps`文件，我们可以确认以下几点：
- 栈地址从`0x7fff16c62000`开始，结束于`0x7fff16c83000`。我们的局部变量都位于这一区域(`0x7fff16c8146c`, `0x7fff16c81424`, `0x7fff16c81428`, `0x7fff16c8142c`)。
- 堆地址从`02050000`开始，结束于`02071000`。我们动态分配的内存也位于这一区域（`0x2050010`）。
- 代码段（`main`函数）位于地址`0x400739`，因此位于下面的区段：

	```shell
	00400000-00401000 r-xp 00000000 08:01 171828 /home/julien/holberton/w/hackthevm2/7
	```
它来自可执行文件`/home/julien/holberton/w/hackthevm2/7`，并且具有可执行权限。
- 命令行参数（变量argv）和环境变量参数(变量env)(从地址`0x7fff16c81568` 到 `0x7fff16c8238f` + `0x1f0`)落在栈的范围内。也就是说，**它们在栈内部而不是在栈外面。**

这也带来了更多的问题：
- 为什么可执行文件在内存中被分成三部分，并且每部分具有不同的权限？下面两个区域中是什么？
	- `00600000-00601000 r--p 00000000 08:01 171828 /home/julien/holberton/w/hackthevm2/7`
	- `00601000-00602000 rw-p 00001000 08:01 171828 /home/julien/holberton/w/hackthevm2/7`
- 其他的区段又是干嘛的？
- 我们动态分配的内存为何不是从堆的起始位置`0x2050000`开始，而是偏移16个字节从`0x2050010`开始？

当然还有另一个事实没有确认：堆真的是向上生长的吗？
  
我们将在下一篇文章中解答这些问题。在结束本章前，让我们看下目前得到的虚拟内存示意图：

![](/virtual_memory/virtual_memory_args_stack.png?raw=true)


## 九、下节预告 ##
通过简单的打印信息，我们学到了有关虚拟内存的很多知识。但是在完成虚拟内存布局图之前，我们还有一些问题需要解决。下一篇文章我们将解决剩余问题。

## 十、继续阅读 ##

- 第一篇:[虚拟内存探究 -- 第一篇:C strings & /proc](https://tech.coderhuo.tech/posts/Virtual_Memory_C_strings_proc/)
- 第二篇:[虚拟内存探究 -- 第二篇:Python 字节](https://tech.coderhuo.tech/posts/Virtual_Memory_python_bytes/)
- 第三篇:[虚拟内存探究 -- 第三篇:一步一步画虚拟内存图](https://tech.coderhuo.tech/posts/Virtual_Memory_drawing_VM_diagram/)
- 第四篇:[虚拟内存探究 -- 第四篇:malloc, heap & the program break](https://tech.coderhuo.tech/posts/Virtual_Memory_malloc_and_heap/)
- 第五篇:[虚拟内存探究 -- 第五篇:The Stack, registers and assembly code](https://tech.coderhuo.tech/posts/Virtual_Memory_malloc_and_heap_stack_and_register/)

## 十一、原文链接 ##
[Hack The Virtual Memory: Drawing the VM diagram](https://blog.holbertonschool.com/hack-the-virtual-memory-drawing-the-vm-diagram/)
