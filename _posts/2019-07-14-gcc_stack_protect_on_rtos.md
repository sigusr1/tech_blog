---
title:  "如何在实时操作系统(RTOS)中使用GCC栈溢出保护(SSP)功能"  
date:   2019-07-14  
categories: [操作系统]
tags: [RTOS, 栈保护, 翻译]
---

* content
{:toc}

本文是对[Using GCC's Stack Smashing Protector on microcontrollers](http://antoinealb.net/programming/2016/06/01/stack-smashing-protector-on-microcontrollers.html)的意译，中间插入了较多自己的理解，主要介绍如何在嵌入式实时操作系统（RTOS）中使用GCC的栈溢出保护功能(Stack Smashing Protection，简称SSP)，特别是编译器本身不支持的情况下。  

C语言中，需要开发人员自己管理内存，不可避免的会引入一系列内存相关的BUG，比如：内存重复释放、野指针、栈溢出等。这些问题通常都比较难定位，因为出问题的地方一般都不是案发现场（比如A处发生内存越界写操作，可能在B处程序才异常）。  



## 一、什么是栈溢出 ##

引用维基百科的说法：**缓冲区溢出**是指往内存中写数据时，越过了对应的内存边界，写到了相邻的内存中。  
如果发生溢出的缓冲区位于栈空间，这就是**栈溢出**，也就是说栈溢出是缓冲区溢出的一种情况。  
黑客可以利用栈溢出修改函数的返回地址，从而改变程序的执行逻辑。如果你的产品具有联网功能，就特别需要注意这一点，以免被攻击。

以下面的代码为例：  

```c
void my_buggy_function(const char *user_provided_message)
{
        char Buffer[16];
        strcpy(Buffer, user_provided_message);
}
```

如果用户提供的信息长度超过16字节，将会导致Buffer发生缓冲区溢出，多出来的数据将会被写到Buffer紧邻的内存区域。如果栈帧中函数的返回地址被修改，将会导致不可预见的异常。  

## 二、GCC栈溢出保护的工作原理 ##
GCC栈溢出保护(SSP)是在函数中插入一个额外的变量(stack canary)，该变量位于函数返回地址所在内存的后面，函数进入的时候该变量被赋为特定的值，函数返回前判断该变量的值有没有改变。如果变化了，说明出现了栈溢出，这时候返回地址可能已经被修改了。  

下图结合第一部分的代码片段展示SSP的工作原理：图1是正常的调用不会产生任何异常；图2写入了20个字节，导致Buffer发生缓冲区溢出，并把返回地址覆盖了，这会导致程序产生非预期的行为，但是程序并不知道发生了栈溢出；图3开启了SSP，函数返回的时候发现canary被修改，检测到栈溢出。  
![栈溢出示意图](/assets/images/2019-07-14-gcc_stack_protect_on_rtos/buffer_overflow.JPG)

当然，SSP并不能检测所有的栈溢出，但有胜于无。不过，SSP会增加运行期消耗，表现为使用的栈内存增加，CPU执行的指令增多。可以考虑在debug版本中开启该功能，release版本中关闭该功能。


## 三、开启GCC栈溢出保护 ##

在编译选项中增加-fstack-protector-all、-fstack-protector-strong、-fstack-protector中的任何一个即可开启GCC的栈溢出保护，三个选项的差异可以参考[https://mudongliang.github.io/2016/05/24/stack-protector.html](https://mudongliang.github.io/2016/05/24/stack-protector.html).

但是，并非所有的编译器能提供完整的支持，比如arm-none-eabi就会报下面的错误：

```console
arm-none-eabi/bin/ld: cannot find -lssp_nonshared
arm-none-eabi/bin/ld: cannot find -lssp
```

看起来是少了一些库。那么如何解决呢？  

可以先通过下面的命令生成空的静态库，然后在gcc的链接选项（一般定义为LDFLAGS）中通过-L添加指向libssp.a和libssp_nonshared.a所在的目录。

```console
arm-none-eabi-ar rcs libssp.a
arm-none-eabi-ar rcs libssp_nonshared.a
```

这时候重新编译，GCC会提示缺少符号`__stack_chk_guard `和 `__stack_chk_fail`。
SSP需要这两个符号才能正常工作：
- __stack_chk_guard 是栈保护区域(stack canary)的初始值
- __stack_chk_fail 为栈被破坏后的回调函数，该函数应该永远不会返回（可以考虑在这个函数中把系统halt住）。

下面是定义`__stack_chk_guard `和 `__stack_chk_fail`的一个最简示例，可以根据具体需要修改。不过要注意，`__stack_chk_guard `的长度必须和系统字长一致（32位系统上`__stack_chk_guard `的大小是4字节，64位系统是8字节）。

```c
uintptr_t __stack_chk_guard = 0xdeadbeef;

void __stack_chk_fail(void)
{
    printf("Stack smashing detected");
}
```

这时候再重新编译，应该就没问题了。  
*注：上面的例子中把`__stack_chk_guard `设为了一个固定值，这在反汇编中很容易看到其取值为0xdeadbeef。如果想让你的程序很难被破解，可以利用硬件随机数发生器，每次启动的时候都将`__stack_chk_guard `设为随机值。*  

可以通过下面的代码测试SSP是否已生效：

```c
void foo(void)
{
    char buffer[2];
    strcpy(buffer, "hello, I am smashing your stack!");
}
```

如果SSP已生效，函数__stack_chk_fail会被调用，否则SSP未生效，这时可以尝试禁用编译器的优化选项。




## 四、参考资料 ##


1. [https://mudongliang.github.io/2016/05/24/stack-protector.html](https://mudongliang.github.io/2016/05/24/stack-protector.html)
2. [https://www.ibm.com/developerworks/cn/linux/l-cn-gccstack/index.html](https://www.ibm.com/developerworks/cn/linux/l-cn-gccstack/index.html)
3. [http://antoinealb.net/programming/2016/06/01/stack-smashing-protector-on-microcontrollers.html](http://antoinealb.net/programming/2016/06/01/stack-smashing-protector-on-microcontrollers.html)
4. [http://www.cbi.umn.edu/securitywiki/CBI_ComputerSecurity/MechanismCanary.html](http://www.cbi.umn.edu/securitywiki/CBI_ComputerSecurity/MechanismCanary.html)
​                