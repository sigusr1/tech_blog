---
title: "ARM下char类型符号问题"
date:   2017-03-19  
categories: [操作系统]  
tags: [计算机基础, arm]  
---

最近在项目中遇到问题，在x86平台下调试好的程序，移植到arm上，程序行为完全变了。

示例如下：

```c
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{	
	char value = 129;
	
	if (value > 128)
	{
			printf("Bigger than 128\n");
	}
	else
	{
			printf("Smaller than 128\n");
	}
	
	return 0;
}
```

在x86平台输出Smaller than 128。  
在arm平台输出Bigger than 128。  

原来，C标准表示char类型可以带符号也可以不带符号，由具体的编译器、处理器或由它们两者共同决定到底char是带符号合适还是不带符号合适。  


大部分体系结构上，char默认是带符号的，它可以自-128到127之间取值，也有一些例外，比如ARM体系结构上，char默认就是不带符号的，它的取值范围是0～255。  


可以在编译的时候通过-funsigned-char 和-fsigned-char 指定char是无符号还是有符号的。
