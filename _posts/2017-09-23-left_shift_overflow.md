---
title: "移位溢出"  
date: 2017-09-23  
categories: [计算机基础]  
tags: [移位溢出]  
---

本文简要介绍左移导致的溢出问题。 



实际项目中需要计算SD卡中某个目录的大小，并判断该目录所占空间是否超过SD卡总容量的一半。  
测试过程中经常发现误报，该目录所占空间远小于SD卡容量一半的时候，就上报占用空间过半的事件。  
排查发现原来是计算的时候移位导致了溢出。问题代码如下：

```c
unsigned int total_space_in_mb;
unsigned long long used_space_in_byte;

/* total_space_in_mb 左移20位从MB转换为byte, 左移19位相当于总容量的一半 */
used_space_in_byte > (unsigned long long)(total_space_in_mb << (20 - 1));
```

我们以下面的例子来分析下原因:

```c
#include<stdio.h>
int main()
{
    unsigned int total_space_in_mb = 30208;
    unsigned long long result;

    result = (unsigned long long)(total_space_in_mb << 19);
    printf("wrong result:%llu\n", result); /* wrong result:2952790016 */
    
    result = ((unsigned long long)total_space_in_mb << 19);
    printf("right result:%llu\n", result); /* right result:15837691904 */
	return 0;
}

``` 

将十进制表示转换为对应的二进制：  
```console
‭30208对应的二进制：                          0111011000000000
‭2952790016对应的二进制：     10110000000000000000000000000000（30208左移19位，溢出2位）
‭15837691904对应的二进制：001110110000000000000000000000000000（30208左移19位，无溢出）
```

错误的代码中虽然进行了强制类型转换，但是转换发生在移位后，所以无法避免溢出。  
正确的代码先进行了类型提升，然后再移位，可以避免溢出。  
这点从汇编代码中可以看出。  
执行gcc -S test.c可以生产名为test.s的汇编代码文件，内容如下：

```console
	.file	"test.c"
	.section	.rodata
.LC0:
	.string	"wrong result:%llu\n"
.LC1:
	.string	"right result:%llu\n"
	.text
	.globl	main
	.type	main, @function
main:
.LFB0:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$16, %rsp
	movl	$30208, -12(%rbp)
	movl	-12(%rbp), %eax
	sall	$19, %eax
	movl	%eax, %eax
	movq	%rax, -8(%rbp)
	movq	-8(%rbp), %rax
	movq	%rax, %rsi
	movl	$.LC0, %edi
	movl	$0, %eax
	call	printf
	movl	-12(%rbp), %eax
	salq	$19, %rax
	movq	%rax, -8(%rbp)
	movq	-8(%rbp), %rax
	movq	%rax, %rsi
	movl	$.LC1, %edi
	movl	$0, %eax
	call	printf
	movl	$0, %eax
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE0:
	.size	main, .-main
	.ident	"GCC: (Ubuntu 5.4.0-6ubuntu1~16.04.4) 5.4.0 20160609"
	.section	.note.GNU-stack,"",@progbits

```

可以看出， 错误代码调用了32位算数左移指令sall, 正确代码调用了64位算术左移指令salq。  
其中eax是寄存器rax的低32位。



  
  

**参考文档：**  
[https://software.intel.com/en-us/articles/introduction-to-x64-assembly](https://software.intel.com/en-us/articles/introduction-to-x64-assembly)  
[https://my.oschina.net/guonaihong/blog/511576](https://my.oschina.net/guonaihong/blog/511576)