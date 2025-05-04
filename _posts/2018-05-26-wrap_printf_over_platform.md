---
title:  "跨平台printf封装方法"  
date:   2018-05-26
categories: [软件设计]  
tags: [跨平台] 
---


* content
{:toc}
嵌入式开发中经常需要跨平台移植，但是不同平台的系统函数通常不一样，如果能封装一个平台适配层，将底层系统差异和上层业务代码隔离，移植起来将事半功倍。






##  需求  ##

系统层次结构如下所示：

![](/2018-05-26-wrap_printf_over_platform/system_layer.JPG?raw=true)

1. 平台适配层用来屏蔽各系统差异，自身编译成静态库，并对业务层提供统一的头文件。
2. 业务层直接调用平台适配层封装好的接口，不关心底层实现。
3. 业务层自身也是编译成静态库，并且编译过程中不依赖Platform相关的头文件。

对于下面常规的函数，封装起来很简单：
```c
/* platform_a.h */
int platform_a_func(int value);

/* platform_b.h */
int platform_b_func(int value);
```

平台适配层头文件定义如下:
```c
/* common.h */
int common_func(int value);
```

对于platform_a的适配如下：
```c
/* common_platform_a.c */
int common_func(int value)
{
    return platform_a_func(value);
}
```

对于platform_b的适配如下：
```c
/* common_platform_b.c */
int common_func(int value)
{
    return platform_b_func(value);
}
```

但是对于printf这种变参函数，假设两个平台提供的函数定义分别如下，应该如何适配呢？
```c
/* platform_a.h */
int platform_a_printf(const char *format, ...);

/* platform_b.h */
int platform_b_printf(const char *format, ...);
```

像下面这样肯定是行不通的，因为变参不能这样传递：
```c
/* common.h */
int common_printf(const char *format, ...);

/* common_platform_a.c */
int common_printf(const char *format, ...)
{
    return platform_a_printf(format, ...);
}
```
##  适配方法  ##

###  方法一：通过va_list传递变参  ###

```c
/* common.h */
int common_printf(const char *format, ...);

/* common_platform_a.c */
int common_printf(const char *format, ...)
{
    int len;
    va_list ap;
    va_start(ap, format);
    len = platform_a_vprintf(format, ap);
    va_end(ap);
    return len;
}
```

但是有些平台并不提供类似vprintf的函数，也就是说可能根本就不存在platform_a_vprintf。
当然我们可以通过vsnpirntf函数先把变参收集到一个缓冲中，然后再调用系统函数platform_a_printf：
```c
/* common.h */
int common_printf(const char *format, ...);

/* common_platform_a.c */
int common_printf(const char *format, ...)
{
    char msg[MAX_MSG_LEN];
    va_list ap;

    va_start(ap, format);
    vsnprintf(msg, MAX_MSG_LEN, format, ap); /* 截断情况下返回值大于MAX_MSG_LEN */
    va_end(ap);

    msg[MAX_MSG_LEN - 1] = 0;
  
    return platform_a_printf("%s", msg);
}
```
这样是解决了我们的问题，但是引入了一个缓冲，多了一次内存拷贝，多了一次函数调用，是有代价的。
那么，有没有其他方法呢？
###  方法二：宏重定义  ###

能否通过下面的方式在预编译期适配掉？
平台适配层头文件定义如下:
```c
/* common.h */

#if defined(PLATFORM_A)
#define common_printf  platform_a_printf
#elif defined(PLATFORM_B)
#define common_printf  platform_b_printf
#else
#error "Please choose your platform!!!"
#endif
 
```

但是这样存在以下问题：
1. 业务层在自身编译的时候，必须显示定义PLATFORM_A或者PLATFORM_B，违背了平台无关的初衷
2. 业务层在包含了common.h后，假设定义了PLATFORM_A，则会对platform_a.h产生依赖，否则编译期会产生platform_a_printf未显示定义的警告。这又把业务层和平台层搅合在一起了。（在`common.h`中添加`int common_printf(const char *format, ...);`可解决该问题）

另一种方式是头文件如下定义：
```c
/* common.h */
int common_printf(const char *format, ...);
```

在编译依赖common.h的代码时，CFLAG中添加选项 `-Dcommon_printf=platform_a_printf`。这种方式原理和上面的一样，都是在预编译期进行符号替换，不同之处是把平台相关的东西从代码中移到编译脚本中。


###  方法三：函数指针  ###

头文件如下定义：
```c
/* common.h */
extern int (*common_printf)(const char *format, ...);
```


对于platform_a的适配如下：
```c
/* common_platform_a.c */
int (*common_printf)(const char *format, ...) = platform_a_printf;
```

对于platform_b的适配如下：
```c
/* common_platform_b.c */
int (*common_printf)(const char *format, ...) = platform_b_printf;
```
这样头文件和编译脚本中都不需要特殊处理，只需要在平台适配层做区分即可。

##  性能分析  ##

方法二由于是编译期就搞定的，无额外消耗，性能最优。  
方法一性能最差，因为额外增加的操作太多。  
方法三略次于方法二，因为多了一次寻址过程。详见下面的分析。  
示例代码：
```c
#include <stdio.h>

int platform_printf(const char *format, ...)
{
    return printf("%s", format);
}

int (*common_printf)(const char *format, ...) = platform_printf;

int direct_call(void)
{
    platform_printf("hello\n");
    return 0;
}

int indirect_call(void)
{
    common_printf("hello\n");
    return 0;
}

int main()
{
    direct_call();
    indirect_call();
    return 0;
}
```
armv7-a架构下的反汇编如下图所示，可以看到，间接调用比直接调用多了3条指令。  
间接调用的时候首先把符号common_printf的地址0x1f64c加载到r3寄存器（右侧第4行和第5行），然后访问该地址的内容（右侧第6行），最后调用common_printf（右侧第9行）。
![](/2018-05-26-wrap_printf_over_platform/armv7-a.JPG?raw=true)


x86-64架构下的反汇编如下图所示，可以看到，间接调用比直接调用多了1条指令。
![](/2018-05-26-wrap_printf_over_platform/x86-64.JPG?raw=true)

## 其他 ##

1. 如果方法三的common.h中不使用extern关键字，每个引用common.h的文件中都会定义一个函数指针common_printf，这倒没什么。因为未初始化的全局变量默认属于弱符号，而common_platform_a.c中初始化了的common_printf则是强符号，链接的时候会选择强符号。  
2. 如果方法三的common.h中不使用extern关键字，但是编译的时候指定了-fno-common选项，则会报common_printf重复定义。这是由于-fno-common相当于把未初始化的全局变量也作为强符号。
3. 如果方法三的common.h中不使用extern关键字，并且忘了链接common_platform_a.o，链接器不会给出任何警告，但是运行的时候会死的很难看，因为common_printf = 0.
4. gcc中的alias属性可以定义函数别名，但是要求别名和原函数必须在同一个编译单元（可以认为是同一个.c文件）。由于platform_a_printf这种函数都是以库的形式提供的，所以无法使用该属性。


## 参考资料 ##

1. [http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.faqs/ka15833.html](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.faqs/ka15833.htmll)
2. [https://gcc.gnu.org/onlinedocs/gcc/Code-Gen-Options.html](https://gcc.gnu.org/onlinedocs/gcc/Code-Gen-Options.html)
3. [https://stackoverflow.com/questions/13089166/how-to-make-gcc-link-strong-symbol-in-static-library-to-overwrite-weak-symbol](https://stackoverflow.com/questions/13089166/how-to-make-gcc-link-strong-symbol-in-static-library-to-overwrite-weak-symbol)