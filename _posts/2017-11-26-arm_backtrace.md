---
title:  "arm平台根据栈进行backtrace的方法"  
date:   2017-11-26
categories: [操作系统]
tags: [backtrace]
---


* content
{:toc}

本文主要介绍在arm平台回溯函数调用栈（backtrace）的方法。


## 一、 背景 ##

嵌入式设备开发过程中，难免会遇到各种死机问题。这类问题的定位一直是开发人员的噩梦。  
死机问题常见定位手段如下：

- 根据打印/日志信息梳理业务逻辑，排查代码；
- 设备死机的时候输出函数调用栈（backtrace），结合符号文件表/反汇编文件定位问题；
- 输出死机时的内存镜像（coredump），利用gdb还原“案发现场”。

三种定位手段中，第一种是最基本的，提供的信息也最少； 第二种能够给出函数调用关系，但是一般无法给出各个参数的值；第三种不仅能够给出函数调用关系，还能查看各个参数的值。但是后两种方法对编译工具链、系统都有一定的要求。

不过大部分嵌入式实时操作系统（RTOS）不支持生成coredump，下面主要介绍backtrace。

	
## 二、 backtrace ##

做backtrace最方便的就是使用gcc自带的backtrace功能，编译的时候加上-funwind-tables选项（该选项对性能无影响但是会使可执行文件略微变大），异常处理函数中调用相关函数即可输出函数调用栈，但是这依赖于你所用的编译工具链是否支持。 
 
另外一种方式就是集成第三方的工具（如unwind），但是这取决于该工具是否支持你的系统。很多开源软件对RTOS的支持并不好，特别是在一些商用的闭源RTOS上，移植开源软件更是比较困难。

下面介绍一种不依赖于第三方工具，不依赖编译工具链的backtrace方法。

### 1. 栈帧 ###
函数调用过程是栈伸缩的过程。调用函数的时候入参、寄存器和局部变量入栈，栈空间增长，函数返回的时候栈收缩。每个函数都有自己的栈空间，被称为栈帧，在被调用的时候创建，在返回的时候销毁。函数main调用func1的时候，栈帧如下图所示：

![](/assets/images/2017-11-26-arm_backtrace/stack_layout.jpg)


函数调用过程中涉及四个重要的寄存器：PC、LR、SP和FP。**注意，每个栈帧中的PC、LR、SP和FP都是寄存器的历史值，并非当前值。**PC寄存器和LR寄存器均指向代码段， 其中PC代表代码当前执行到哪里了，LR代表当前函数返回后，要回到哪里去继续执行。SP和FP用来维护栈空间，其中SP指向栈顶，FP指向上一个栈帧的栈顶。

上图中蓝灰色部分是main函数的栈帧，绿色部分是func1的栈帧。左边的标有SP、FP的箭头分别指向func1的栈顶和main的栈顶。右边的两条折线代表函数func1返回的时候（栈收缩），SP和FP将要指向的地方。


如此看来，栈是通过FP和SP寄存器串成一串的，每个单元就是一个栈帧（也就是一个函数调用过程）。又由于LR是指向调用函数的（即PC寄存器的历史值，通过addr2line工具或者把可执行文件反汇编，可以看到func1中的LR落在main函数中，并且指向调用func1的下一条语句）。那么，如果能得到每个栈帧中的LR值，就能得到整个的函数调用链。


我们可以根据FP和SP寄存器回溯函数调用过程，以上图为例：函数func1的栈中保存了main函数的栈信息（绿色部分的SP和FP），通过这两个值，我们可以知道main函数的栈起始地址（也就是FP寄存器的值）， 以及栈顶（也就是SP寄存器的值）。得到了main函数的栈帧，就很容易从里面提取LR寄存器的值了（FP向下偏移4个字节即为LR），也就知道了谁调用了main函数。以此类推，可以得到一个完整的函数调用链（一般回溯到 main函数或者线程入口函数就没必要继续了）。实际上，回溯过程中我们并不需要知道栈顶SP，只要FP就够了。

### 2. 编译优化 ###


仔细考虑下，栈帧中保存的PC值是没啥用的，有LR就够了。另外，并非每个函数都需要FP（具体哪些函数的栈帧中有FP哪些函数中没有，由编译器根据编译选项决定），那么也没必要为它重新开辟一个栈帧，继续在调用者的栈帧上运行即可。

gcc编译选项-fomit-frame-pointer就是优化FP寄存器的，这样可以把FP寄存器省下来在其他地方使用，可以提高运行效率，arm平台最新版本的编译器都是默认打开该选项的。另外，gcc编译的优化选项如 o1、o2等也会对影响栈帧布局。

这么看来 ，通过FP寄存器回溯的方式行不通了！！！

### 3. 通过追踪栈变化回溯函数 ###

由于函数调用完毕要返回，所以在调用函数的时候，LR寄存器的值必须入栈，也就是说，每个栈帧中肯定包含LR寄存器的历史值，并且LR位于当前栈帧的栈底。另外，SP指针我们肯定是可以拿到的。下面以一个例子看下如何回溯：

下面的代码中func1调用func3, 入参param是NULL, func3中访问非法内存导致程序异常。


```c
struct obj
{
    int a;
    int b;
};

int func3(struct obj *param, int a, int b, char *dst_str, char *src_str)
{
    int len;
    int i;

    param->a = param->b;	/* 非法访问 */

    for (i = 0; i < b; i++)
    {
        a--;
    }
    
    strcpy(dst_str, src_str);
    len = strlen(src_str);
    
    return (a + b + len);
}

void func1(char *src_str)
{
    int result = 0;
    char str[8];

    if (src_str == NULL)
    {
        return;
    }

    if (!strcmp(src_str, "backtrace"))
	{
        result = func3(NULL, 4, 3, str, "111");
	}
	else if (!strcmp(src_str, "stack_protector"))
	{
	    /* 32 个1， 远大于str 容量*/
        result = func2(4, 3, str, "11111111111111111111111111111111");	
	}
    
    return;
}
```

程序异常的栈信息如下：

![](/assets/images/2017-11-26-arm_backtrace/stack_unwind.JPG)


可以看到问题出现时的PC寄存器取值为0xa02f8524, SP寄存器的值为0xa84f26d8。通过反汇编代码可以看到，PC指针落在函数func3内部。另外看到func3中入栈了4个寄存器，每个寄存器4个字节，lr的值（也就是栈底）应该位于0xa84f26d8（当前的栈顶）向上偏移12个字节的地方，也就是0xa84f26e4的地方，从栈信息中可以看到这里的数值是0xa02f85d8，指向函数func1。

![](/assets/images/2017-11-26-arm_backtrace/stack_func3.jpg)


从下图func1的汇编代码中可以看到，栈增长了16个字节，并且有两个寄存器入栈。本栈帧中lr的位置应该在0xa84f26e4（func3栈帧中lr的位置）的基础上向上偏移24个字节，也就是0xa84f26fc，从栈信息中可以看到这里的数值是0xa02f8a70, 指向函数func2，同样的方法，可以得到再往上一级调用是0xa029d404, 指向func。这样我们就得到了下面的调用链：

**func –> func2 –> func1 –> func3**


![](/assets/images/2017-11-26-arm_backtrace/stack_func1.jpg)


### 4. 程序实现 ###


在异常处理函数中，根据以上思路，添加自定义的backtrace函数，可以实现函数调用栈回溯。

实现过程中需要根据pc指针遍历代码区，识别每个函数中的栈相关操作指令，计算lr位置，依次循环。另外需要判断回溯的终止点。

这种方法最大的优点就是对可执行文件大小、程序性能都不会产生影响，无副作用。
局限性在于整个机制是基于栈的，如果栈被破坏了，就完了

	
## 三、 栈保护 ##


如果代码中出现类似下面的情况，栈会被破坏，上面提到的回溯机制也将会失效。这种问题，死机的地方一般不是出问题的地方，打印出来的pc指针也是乱七八糟的，对定位问题很不利。

```c
void func(void)
{
	char str[4];
	memset(str, 0, 100);
	return;
}
```

为了能在出问题的时候就把异常抛出，可以引入gcc的编译选项-fstack-protector/-fstack-protector-all/-fstack-protector-strong，它们会在栈帧之间插入特定的内容作为安全边界，函数返回的时候对该边界做校验，如果发现被修改就抛出异常。我们可以在异常处理函数中把PC指针打出来，从而知道死在那里了。同时还可以根据SP指针把栈内容打出来一部分，观察被踩的区域，结合代码人工排查。

遗憾的是某些RTOS使用的编译工具链不支持栈保护编译选项，好在下面的资料给出了一种解决方法：

- [英文版：http://antoinealb.net/programming/2016/06/01/stack-smashing-protector-on-microcontrollers.html](http://antoinealb.net/programming/2016/06/01/stack-smashing-protector-on-microcontrollers.html)
- [中文版：https://tech.coderhuo.tech/posts/gcc_stack_protect_on_rtos](https://tech.coderhuo.tech/posts/gcc_stack_protect_on_rtos)

栈保护有一定的局限性，并非所有的栈溢出问题都能被检测到，另外开启后执行的指令增多了，对性能或多或少会有影响。内部调试版本可以使用该方法定位相关问题。


## 四、 参考资料 ##

1. [https://stackoverflow.com/questions/15752188/arm-link-register-and-frame-pointer](https://stackoverflow.com/questions/15752188/arm-link-register-and-frame-pointer)
2. [https://yosefk.com/blog/getting-the-call-stack-without-a-frame-pointer.html](https://yosefk.com/blog/getting-the-call-stack-without-a-frame-pointer.html)
3. [http://antoinealb.net/programming/2016/06/01/stack-smashing-protector-on-microcontrollers.html](http://antoinealb.net/programming/2016/06/01/stack-smashing-protector-on-microcontrollers.html)