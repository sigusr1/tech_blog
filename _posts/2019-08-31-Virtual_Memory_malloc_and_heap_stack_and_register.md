---
title:  "虚拟内存探究 -- 第五篇:The Stack, registers and assembly code"  
date:   2019-08-31  
categories: [操作系统]
tags: [虚拟内存, 翻译]
---

* content
{:toc}  

这是虚拟内存系列文章的第五篇，也是最后一篇，目标是以不同的方式在实践中学习一些计算机基础知识。




前几篇可以通过下面的链接查看：  

- 第一篇:[虚拟内存探究 -- 第一篇:C strings & /proc](http://blog.coderhuo.tech/2017/10/12/Virtual_Memory_C_strings_proc/)
- 第二篇:[虚拟内存探究 -- 第二篇:Python 字节](http://blog.coderhuo.tech/2017/10/15/Virtual_Memory_python_bytes/)
- 第三篇:[虚拟内存探究 -- 第三篇:一步一步画虚拟内存图](http://blog.coderhuo.tech/2017/10/16/Virtual_Memory_drawing_VM_diagram/)
- 第四篇:[虚拟内存探究 -- 第四篇:malloc, heap & the program break](http://blog.coderhuo.tech/2017/10/18/Virtual_Memory_malloc_and_heap/)
 
## 一、栈 ##

从[《虚拟内存探究 -- 第三篇:一步一步画虚拟内存图》](https://tech.coderhuo.tech/posts/Virtual_Memory_drawing_VM_diagram/)这一章我们了解到，栈是从高地址向低地址生长的。但它究竟是如何工作的呢？它是如何翻译成汇编代码的？寄存器是如何使用的？  

本章我们将深入学习栈是如何工作的，以及局部变量是如何自动申请、释放的。  

一旦掌握了这些知识，我们就可以搞点事情，比如劫持程序的执行流程。  

准备好了吗？让我们开始吧！  

*注意：我们即将学习的是用户态的栈，而不是内核态的栈。*



## 二、预备知识 ##
学习本章内容，需要掌握C语言的基础知识，特别是指针相关知识。


## 三、实验环境 ##
所有的脚本和程序都在下面的环境中测试过：  

- Ubuntu  
	- Linux ubuntu 4.4.0-31-generic #50~14.04.1-Ubuntu SMP Wed Jul 13 01:07:32 UTC 2016 x86_64 x86_64 x86_64 GNU/Linux
- gcc  
	- gcc (Ubuntu 4.8.4-2ubuntu1~14.04.3) 4.8.4
- objdump  
	- GNU objdump (GNU Binutils for Ubuntu) 2.2

**本文涉及的内容在上面的环境中都是OK的，但在其他环境中可能会存在差异。**


## 四、局部变量 ##
### 1、自动分配内存 ###

我们先看一个简单的程序，它只有一个main函数，main函数中只有一个变量：  

```c
#include <stdio.h>

int main(void)
{
    int a;

    a = 972;
    printf("a = %d\n", a);
    return (0);
}
```

我们先编译这个程序，然后用`objdump`反汇编生成的可执行程序:  

    holberton$ gcc 0-main.c
    holberton$ objdump -d -j .text -M intel a.out

`main`函数对应的反汇编代码如下：  

	  
	000000000040052d <main>:
	  40052d:       55                      push   rbp
	  40052e:       48 89 e5                mov    rbp,rsp
	  400531:       48 83 ec 10             sub    rsp,0x10
	  400535:       c7 45 fc cc 03 00 00    mov    DWORD PTR [rbp-0x4],0x3cc
	  40053c:       8b 45 fc                mov    eax,DWORD PTR [rbp-0x4]
	  40053f:       89 c6                   mov    esi,eax
	  400541:       bf e4 05 40 00          mov    edi,0x4005e4
	  400546:       b8 00 00 00 00          mov    eax,0x0
	  40054b:       e8 c0 fe ff ff          call   400410 <printf@plt>
	  400550:       b8 00 00 00 00          mov    eax,0x0
	  400555:       c9                      leave  
	  400556:       c3                      ret    
	  400557:       66 0f 1f 84 00 00 00    nop    WORD PTR [rax+rax*1+0x0]
	  40055e:       00 00   
	




我们重点关注`main`函数的前3行汇编代码：  

	000000000040052d <main>:
	  40052d:       55                      push   rbp
	  40052e:       48 89 e5                mov    rbp,rsp
	  400531:       48 83 ec 10             sub    rsp,0x10


前两行操作了寄存器`rbp`和`rsp`。这两个都是具有特殊用途的寄存器，`rbp`是指向当前栈帧底部的寄存器，`rsp`是指向当前栈帧顶部的寄存器。  

我们逐步展示下这里发生了什么。  

刚进入`main`函数还未执行任何指令的时候，栈状态如下图所示：  

![main函数栈初始状态](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/stack/main_stack_init_status.png?raw=true)


指令`push rbp`把寄存器`rbp`的值存在了栈上，并使栈向下生长（即栈顶下移，同时寄存器`rsp`指向新的栈顶），如下图所示：  


![rbp入栈后的栈状态](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/stack/main_stack_after_push.png?raw=true)


指令`mov rbp, rsp`将寄存器`rsp`的值拷贝到寄存器`rbp`，现在二者都指向新栈帧的栈顶，如下图所示：  


![rbp、rsp均指向栈顶](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/stack/main_stack_after_mov.png?raw=true)


指令`sub rsp, 0x10`在栈上为局部变量开辟了存储空间，即从`rbp`所指地址到`rsp`所指地址的内存区域（16个字节），足以存储`int`型变量`a`。这块内存空间被称为**栈帧**。任何一个定义了局部变量的函数，都会使用栈帧来存储这些局部变量。


![开辟内存空间后的栈状态](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/stack/main_stack_after_create_space.png?raw=true)


### 2、使用局部变量 ###

`main`函数对应汇编代码的第四行如下所示:  

	  400535:       c7 45 fc cc 03 00 00    mov    DWORD PTR [rbp-0x4],0x3cc

`0x3cc`是十进制数`972`的十六进制表示。这句汇编代码对应的C代码如下:  

```c
a = 972;
```

指令`mov DWORD PTR [rbp-0x4],0x3cc`把`rbp - 4`所指内存区域赋值为`972`。`[rbp - 4]`此时表示的就是局部变量`a`。计算机并不知道代码中所用变量的名字，它指向栈上不同的内存区域来表示不同的变量。  

下图是赋值操作后栈和寄存器的状态：  

![赋值后栈和寄存器的状态](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/stack/main_stack_after_assign.png?raw=true)


### 3、自动销毁内存 ###

`main`函数结束的时候调用了`leave`指令：  

	400555:       c9                      leave  

`leave`指令实现了栈收缩的功能，该指令首先把`rbp`的值赋给`rsp`（*译者注：栈顶指向上一个栈帧的栈顶*），然后执行出栈操作，把栈顶存储的值赋给`rbp`（*译者注:当前栈顶存储的是`rbp`的历史值，出栈操作将`rbp`恢复为调用`main`函数前的状态，也就是指向上一个栈帧的底部*）。所以，本指令达到了两个目的：  

- 局部变量占用的内存空间被释放
- 栈、寄存器`rbp`和`rsp`均恢复成调用`main`函数前的状态


![leave后栈和寄存器的状态1](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/stack/lacal_var_after_func1_leave.png?raw=true)

![leave后栈和寄存器的状态2](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/stack/main_stack_after_leave2.png?raw=true)



## 五、对栈的进一步探究 ##
### 1、局部变量为什么要初始化 ###

当栈上的局部变量被释放的时候，他们并未被完全清理。他们所占内存区域，填充的仍然是原来的数值，而这一区域在接下来可能被其他函数用到。所以，程序中的变量一定要初始化，否则，他们的值取决于所用栈内存区域的当前状态，是不确定的。  

下面的例子可以解释为什么局部变量在使用前必须初始化，及其背后的原理：  

```c
#include <stdio.h>

void func1(void)
{
    int a;
    int b;
    int c;

    a = 98;
    b = 972;
    c = a + b;
    printf("a = %d, b = %d, c = %d\n", a, b, c);
}

void func2(void)
{
    int a;
    int b;
    int c;

    printf("a = %d, b = %d, c = %d\n", a, b, c);
}

int main(void)
{
    func1();
    func2();
    return (0);
}
```


如上所示，`func2`中并未给变量`a`、`b`、`c`赋值，我们看下编译运行这段代码会出现什么情况：  

	holberton$ gcc 1-main.c && ./a.out 
	a = 98, b = 972, c = 1070
	a = 98, b = 972, c = 1070
	holberton$ 

不可思议，`func2`中的变量`a`、`b`、`c`打印出来的数值和`func1`一模一样！这是由于栈的工作原理决定的。这两个函数局部变量的个数一样，类型一样，顺序也一样，这导致它们的栈帧也一样。`func1`退出的时候，仅仅是栈收缩，局部变量`a`、`b`、`c`所在的内存区域并未被清理，其值仍然为`func1`退出时的值。因此，当`func2`被调用的时候，它的栈帧和原来`func1`的栈帧完全重合，`func2`中局部变量`a`、`b`、`c`的值就是`func1`退出时局部变量`a`、`b`、`c`的值。

我们可以从汇编代码证明这一推论。首先反汇编上面的可执行程序：  

	holberton$ objdump -d -j .text -M intel a.out

对应的汇编代码如下：

	000000000040052d <func1>:
	  40052d:       55                      push   rbp
	  40052e:       48 89 e5                mov    rbp,rsp
	  400531:       48 83 ec 10             sub    rsp,0x10
	  400535:       c7 45 f4 62 00 00 00    mov    DWORD PTR [rbp-0xc],0x62
	  40053c:       c7 45 f8 cc 03 00 00    mov    DWORD PTR [rbp-0x8],0x3cc
	  400543:       8b 45 f8                mov    eax,DWORD PTR [rbp-0x8]
	  400546:       8b 55 f4                mov    edx,DWORD PTR [rbp-0xc]
	  400549:       01 d0                   add    eax,edx
	  40054b:       89 45 fc                mov    DWORD PTR [rbp-0x4],eax
	  40054e:       8b 4d fc                mov    ecx,DWORD PTR [rbp-0x4]
	  400551:       8b 55 f8                mov    edx,DWORD PTR [rbp-0x8]
	  400554:       8b 45 f4                mov    eax,DWORD PTR [rbp-0xc]
	  400557:       89 c6                   mov    esi,eax
	  400559:       bf 34 06 40 00          mov    edi,0x400634
	  40055e:       b8 00 00 00 00          mov    eax,0x0
	  400563:       e8 a8 fe ff ff          call   400410 <printf@plt>
	  400568:       c9                      leave  
	  400569:       c3                      ret    
	
	000000000040056a <func2>:
	  40056a:       55                      push   rbp
	  40056b:       48 89 e5                mov    rbp,rsp
	  40056e:       48 83 ec 10             sub    rsp,0x10
	  400572:       8b 4d fc                mov    ecx,DWORD PTR [rbp-0x4]
	  400575:       8b 55 f8                mov    edx,DWORD PTR [rbp-0x8]
	  400578:       8b 45 f4                mov    eax,DWORD PTR [rbp-0xc]
	  40057b:       89 c6                   mov    esi,eax
	  40057d:       bf 34 06 40 00          mov    edi,0x400634
	  400582:       b8 00 00 00 00          mov    eax,0x0
	  400587:       e8 84 fe ff ff          call   400410 <printf@plt>
	  40058c:       c9                      leave  
	  40058d:       c3                      ret  
	
	000000000040058e <main>:
	  40058e:       55                      push   rbp
	  40058f:       48 89 e5                mov    rbp,rsp
	  400592:       e8 96 ff ff ff          call   40052d <func1>
	  400597:       e8 ce ff ff ff          call   40056a <func2>
	  40059c:       b8 00 00 00 00          mov    eax,0x0
	  4005a1:       5d                      pop    rbp
	  4005a2:       c3                      ret    
	  4005a3:       66 2e 0f 1f 84 00 00    nop    WORD PTR cs:[rax+rax*1+0x0]
	  4005aa:       00 00 00 
	  4005ad:       0f 1f 00                nop    DWORD PTR [rax]



从上面的汇编代码可以看出，栈帧形成的方式是一致的。在我们的例子中，由于`func1`和`func2`两个函数的局部变量一样，所以形成的栈帧大小也是一样的。这从下面开辟栈帧相关的汇编代码可以看出来：  

	push   rbp
	mov    rbp,rsp
	sub    rsp,0x10

 
在`func1`和`func2`这两个函数中，变量`a`、`b`、`c`的解析方式是一样的：  

- `a`位于`rbp - 0xc`所指的内存区域
- `b`位于`rbp - 0x8`所指的内存区域
- `c`位于`rbp - 0x4`所指的内存区域

注意，这几个变量在栈上的排列顺序和代码中定义的顺序并不一致。实际上，编译器会根据一定的规则排列局部变量，我们不应该对局部变量在栈上的顺序做任何假设。  

函数`func1`返回前的栈如下图所示：  

![函数func1返回前的栈示意图](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/stack/lacal_var_befor_func1_leave.png?raw=true)

函数`func1`返回的时候，会调用指令`leave`。前面解释过，该指令会导致栈帧收缩，如下图所示：  


![函数func1返回后的栈示意图](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/stack/lacal_var_after_func1_leave.png?raw=true)

当我们调用函数`func2`时，它的栈帧如下图所示，局部变量的值就是当前栈上残留的值。这就是`func2`中变量`a`、`b`、`c`的值和`func1`一致的原因了。

![函数func2的栈帧](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/stack/lacal_var_when_func2_enter.png?raw=true)



### 2、函数返回机制：ret指令 ###


你可能已经注意到了，所有的示例函数都是以指令`ret`结尾。该指令从栈中取出返回地址，并且跳转到那里（在函数被调用前，`call`指令就已经把返回地址入栈）。这就是函数调用的工作原理，它可以确保函数能够正确返回并执行下一条指令。  

这么说来，栈上存储的不仅有局部变量，还有函数返回地址。  

回到上面的例子，`main`函数调用`func1`的汇编代码如下：  

	400592:       e8 96 ff ff ff          call   40052d <func1>

该汇编指令先把下一条指令所在的内存地址存储到栈上，然后再跳转到`func1`。这样，在执行`func1`的指令前，栈顶包含了函数`func1`调用完成后的返回地址，寄存器`rsp`指向该区域，如下图所示：  

![call前的栈帧](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/stack/call_statck.png?raw=true)

当`func1`的栈帧形成后，完整的栈帧如下所示：  

![func1的栈帧](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/stack/func1_stack.png?raw=true)


## 六、通过寄存器探索栈内容 ##

结合上面介绍的知识，我们可以直接利用寄存器`rbp`访问局部变量（不是通过变量名），以及栈上保存的寄存器`rbp`的历史值、函数返回地址。  

下面的代码可以访问寄存器：  

	register long rsp asm ("rsp");
	register long rbp asm ("rbp");

完整代码如下所示：  

```c
#include <stdio.h>

void func1(void)
{
    int a;
    int b;
    int c;
    register long rsp asm ("rsp");
    register long rbp asm ("rbp");

    a = 98;
    b = 972;
    c = a + b;
    printf("a = %d, b = %d, c = %d\n", a, b, c);
    printf("func1, rpb = %lx\n", rbp);
    printf("func1, rsp = %lx\n", rsp);
    printf("func1, a = %d\n", *(int *)(((char *)rbp) - 0xc) );
    printf("func1, b = %d\n", *(int *)(((char *)rbp) - 0x8) );
    printf("func1, c = %d\n", *(int *)(((char *)rbp) - 0x4) );
    printf("func1, previous rbp value = %lx\n", *(unsigned long int *)rbp );
    printf("func1, return address value = %lx\n", *(unsigned long int *)((char *)rbp + 8) );
}

void func2(void)
{
    int a;
    int b;
    int c;
    register long rsp asm ("rsp");
    register long rbp asm ("rbp");

    printf("func2, a = %d, b = %d, c = %d\n", a, b, c);
    printf("func2, rpb = %lx\n", rbp);
    printf("func2, rsp = %lx\n", rsp);
}

int main(void)
{
    register long rsp asm ("rsp");
    register long rbp asm ("rbp");

    printf("main, rpb = %lx\n", rbp);
    printf("main, rsp = %lx\n", rsp);
    func1();
    func2();
    return (0);
}
```

从上面的章节我们了解到，`func1`的栈帧如下所示：  

![func1的栈帧](https://github.com/sigusr1/blog_assets/blob/master/virtual_memory/stack/func1_stack.png?raw=true)

### 1、访问局部变量 ###

根据前面的知识，我们知道局部变量和寄存器`rbp`的对应关系如下：  

- `a`位于`rbp - 0xc`所指的内存区域
- `b`位于`rbp - 0x8`所指的内存区域
- `c`位于`rbp - 0x4`所指的内存区域


下面介绍如何通过寄存器`rbp`获取变量`a`的值：  

- 将局部变量`rbp`转化为类型`char *`：`(char *)rbp` 
- 计算变量`a`在内存中的位置：`(char *)rbp) - 0xc`
- 将上一步的数值转换为`int`类型的指针，因为变量`a`是`int`类型的：`(int *)(((char *)rbp) - 0xc)`
- 从上一步的内存地址中解析数值，该值即为变量`a`的数值: `*(int *)(((char *)rbp) - 0xc)`

### 2、访问寄存器rbp的历史值 ###

从`func1`的栈帧示意图可以看出，寄存器`rbp`直接指向寄存器`rbp`历史值的存储区域。所以，我们只需要将变量`rsp`转换为`unsigned long int`类型的指针，并解析该指针指向内存区域的值即可：`*(unsigned long int *)rbp`。

### 3、访问函数返回地址 ###


从`func1`的栈帧示意图可以看出，函數返回地址存储在寄存器`rbp`历史值的前面。寄存器`rbp`的历史值占用8字节的存储空间，所以我们需要将变量`rbp`的当前值加8，得到返回地址所在内存的地址。具体如下：  


- 将局部变量`rbp`转化为类型`char *`：`(char *)rbp` 
- 将变量`rbp`的值加8：`((char *)rbp + 8)`
- 将上一步的数值转换为`unsigned long int`类型的指针：`(unsigned long int *)(((char *)rbp) + 8)`
- 从上一步的内存地址中解析数值，该值即函数返回地址: `*(unsigned long int *)((char *)rbp + 8)`

### 4、程序执行结果 ###

上面程序的运行结果如下：  
	
	holberton$ gcc 2-main.c && ./a.out 
	main, rpb = 7ffc78e71b70
	main, rsp = 7ffc78e71b70
	a = 98, b = 972, c = 1070
	func1, rpb = 7ffc78e71b60
	func1, rsp = 7ffc78e71b50
	func1, a = 98
	func1, b = 972
	func1, c = 1070
	func1, previous rbp value = 7ffc78e71b70
	func1, return address value = 400697
	func2, a = 98, b = 972, c = 1070
	func2, rpb = 7ffc78e71b60
	func2, rsp = 7ffc78e71b50
	holberton$


从上面的结果可以得到以下结论：  

- 在函数`func1`中我们通过寄存器`rbp`正确访问了所有的局部变量
- 在函数`func1`中我们获取了`main`函数栈帧的`rbp`值（寄存器`rbp`的历史值）
- 函数`func1`和`func2`拥有相同的`rbp`和`rsp`值(它们的栈帧是相同的）
- 从`func1`的汇编代码`sub rsp,0x10`可以看出，寄存器`rbp`和`rsp`的差值是`0x10`
- 在`main`函数中`rbp`和`rsp`的值相同，因为`main`无局部变量

从程序运行结果看，`func1`执行完毕的返回地址是`0x400697`。我们通过汇编代码确认下该值是否正确。如果该值是对的，这个地址应该是`main`函数中紧挨着`func1`的下一条指令。  

首先反汇编该可执行程序：  

	holberton$ objdump -d -j .text -M intel a.out | less

`main`函数对应的汇编代码如下所示：  

	0000000000400664 <main>:
	  400664:       55                      push   rbp
	  400665:       48 89 e5                mov    rbp,rsp
	  400668:       48 89 e8                mov    rax,rbp
	  40066b:       48 89 c6                mov    rsi,rax
	  40066e:       bf 3b 08 40 00          mov    edi,0x40083b
	  400673:       b8 00 00 00 00          mov    eax,0x0
	  400678:       e8 93 fd ff ff          call   400410 <printf@plt>
	  40067d:       48 89 e0                mov    rax,rsp
	  400680:       48 89 c6                mov    rsi,rax
	  400683:       bf 4c 08 40 00          mov    edi,0x40084c
	  400688:       b8 00 00 00 00          mov    eax,0x0
	  40068d:       e8 7e fd ff ff          call   400410 <printf@plt>
	  400692:       e8 96 fe ff ff          call   40052d <func1>
	  400697:       e8 7a ff ff ff          call   400616 <func2>
	  40069c:       b8 00 00 00 00          mov    eax,0x0
	  4006a1:       5d                      pop    rbp
	  4006a2:       c3                      ret    
	  4006a3:       66 2e 0f 1f 84 00 00    nop    WORD PTR cs:[rax+rax*1+0x0]
	  4006aa:       00 00 00 
	  4006ad:       0f 1f 00                nop    DWORD PTR [rax]
	

可以看出，我们的猜测是正确的，`0x400697`是紧挨着`func1`的下一条指令。


## 七、栈入侵 ##

既然我们已经知道函数返回地址在栈上的位置，如果我们修改了这个值会怎么样呢？  

我们是否可以改变程序的执行流程，让`func1`执行完毕返回到其他地方？  

在上面代码的基础上，我们增加一个新函数`bye`：  

```c
#include <stdio.h>
#include <stdlib.h>

void bye(void)
{
    printf("[x] I am in the function bye!\n");
    exit(98);
}

void func1(void)
{
    int a;
    int b;
    int c;
    register long rsp asm ("rsp");
    register long rbp asm ("rbp");

    a = 98;
    b = 972;
    c = a + b;
    printf("a = %d, b = %d, c = %d\n", a, b, c);
    printf("func1, rpb = %lx\n", rbp);
    printf("func1, rsp = %lx\n", rsp);
    printf("func1, a = %d\n", *(int *)(((char *)rbp) - 0xc) );
    printf("func1, b = %d\n", *(int *)(((char *)rbp) - 0x8) );
    printf("func1, c = %d\n", *(int *)(((char *)rbp) - 0x4) );
    printf("func1, previous rbp value = %lx\n", *(unsigned long int *)rbp );
    printf("func1, return address value = %lx\n", *(unsigned long int *)((char *)rbp + 8) );
}

void func2(void)
{
    int a;
    int b;
    int c;
    register long rsp asm ("rsp");
    register long rbp asm ("rbp");

    printf("func2, a = %d, b = %d, c = %d\n", a, b, c);
    printf("func2, rpb = %lx\n", rbp);
    printf("func2, rsp = %lx\n", rsp);
}

int main(void)
{
    register long rsp asm ("rsp");
    register long rbp asm ("rbp");

    printf("main, rpb = %lx\n", rbp);
    printf("main, rsp = %lx\n", rsp);
    func1();
    func2();
    return (0);
}
```

我们看下`bye`函数的起始地址，首先反汇编：  

	holberton$ gcc 3-main.c && objdump -d -j .text -M intel a.out | less


函数`bye`的汇编代码如下：  

	00000000004005bd <bye>:
	  4005bd:       55                      push   rbp
	  4005be:       48 89 e5                mov    rbp,rsp
	  4005c1:       bf d8 07 40 00          mov    edi,0x4007d8
	  4005c6:       e8 b5 fe ff ff          call   400480 <puts@plt>
	  4005cb:       bf 62 00 00 00          mov    edi,0x62
	  4005d0:       e8 eb fe ff ff          call   4004c0 <exit@plt>


下面的代码中，我们将在函数`func1`中把返回地址替换成函数`bye`的起始地址`0x4005bd`：  

```c
#include <stdio.h>
#include <stdlib.h>

void bye(void)
{
    printf("[x] I am in the function bye!\n");
    exit(98);
}

void func1(void)
{
    int a;
    int b;
    int c;
    register long rsp asm ("rsp");
    register long rbp asm ("rbp");

    a = 98;
    b = 972;
    c = a + b;
    printf("a = %d, b = %d, c = %d\n", a, b, c);
    printf("func1, rpb = %lx\n", rbp);
    printf("func1, rsp = %lx\n", rsp);
    printf("func1, a = %d\n", *(int *)(((char *)rbp) - 0xc) );
    printf("func1, b = %d\n", *(int *)(((char *)rbp) - 0x8) );
    printf("func1, c = %d\n", *(int *)(((char *)rbp) - 0x4) );
    printf("func1, previous rbp value = %lx\n", *(unsigned long int *)rbp );
    printf("func1, return address value = %lx\n", *(unsigned long int *)((char *)rbp + 8) );
    /* hack the stack! */
    *(unsigned long int *)((char *)rbp + 8) = 0x4005bd;
}

void func2(void)
{
    int a;
    int b;
    int c;
    register long rsp asm ("rsp");
    register long rbp asm ("rbp");

    printf("func2, a = %d, b = %d, c = %d\n", a, b, c);
    printf("func2, rpb = %lx\n", rbp);
    printf("func2, rsp = %lx\n", rsp);
}

int main(void)
{
    register long rsp asm ("rsp");
    register long rbp asm ("rbp");

    printf("main, rpb = %lx\n", rbp);
    printf("main, rsp = %lx\n", rsp);
    func1();
    func2();
    return (0);
}
```


编译运行，结果如下：  

	holberton$ gcc 4-main.c && ./a.out
	main, rpb = 7fff62ef1b60
	main, rsp = 7fff62ef1b60
	a = 98, b = 972, c = 1070
	func1, rpb = 7fff62ef1b50
	func1, rsp = 7fff62ef1b40
	func1, a = 98
	func1, b = 972
	func1, c = 1070
	func1, previous rbp value = 7fff62ef1b60
	func1, return address value = 40074d
	[x] I am in the function bye!
	holberton$ echo $?
	98
	holberton$ 


天哪！！！函数`bye`竟然被调用了，尽管我们在代码中并未调用该函数。



## 八、继续阅读 ##

- 第一篇:[虚拟内存探究 -- 第一篇:C strings & /proc](http://blog.coderhuo.tech/2017/10/12/Virtual_Memory_C_strings_proc/)
- 第二篇:[虚拟内存探究 -- 第二篇:Python 字节](http://blog.coderhuo.tech/2017/10/15/Virtual_Memory_python_bytes/)
- 第三篇:[虚拟内存探究 -- 第三篇:一步一步画虚拟内存图](http://blog.coderhuo.tech/2017/10/16/Virtual_Memory_drawing_VM_diagram/)
- 第四篇:[虚拟内存探究 -- 第四篇:malloc, heap & the program break](http://blog.coderhuo.tech/2017/10/18/Virtual_Memory_malloc_and_heap/)
- 第五篇：[虚拟内存探究 -- 第五篇:The Stack, registers and assembly code](http://blog.coderhuo.tech/2019/08/31/Virtual_Memory_malloc_and_heap_stack_and_register/)

## 九、原文链接 ##
[Hack the virtual memory, chapter 4: the stack, registers and assembly code](https://github.com/holbertonschool/Hack-The-Virtual-Memory/tree/master/04.%20The%20Stack%2C%20registers%20and%20assembly%20code)

