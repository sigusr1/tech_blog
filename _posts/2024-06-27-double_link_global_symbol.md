---
title: "静态库重复链接导致的crash问题"
date: 2024-06-27
categories: [操作系统]
tags:  [elf]  
---

近期项目中遇到一个静态变量被double free导致的crash问题。    
**奇怪的是这个静态变量被构造了两次，但内存地址却是一样的。**  
最终查下来，是因为动态库libA.so、libB.so都链接了libCommon.a（包含全局变量kProblemSymbol），可执行程序同时依赖libA.so和libB.so，导致静态变量kProblemSymbol被double free。


接下来通过一个示例程序还原下这个问题。  

## 1. 示例程序

程序结构如下所示(完整例子参考：[demo](http://data.coderhuo.tech/2024-06-27-double_link_global_symbol/code/))
- 静态库libCommon.a包含静态变量kProblemSymbol
- 动态库libA.so、libB.so都链接了libCommon.a
- 可执行程序C同时依赖动态库libA.so、libB.so

> 有经验的同学估计一眼就看出来问题在哪了：这种情况下libCommon应该用动态库，而不是静态库。
{: .prompt-info }

![程序框图](/2024-06-27-double_link_global_symbol/framework.jpg)

### 1.1 libCommon.a

类Common中定义了静态变量kProblemSymbol供外部模块使用：

```c++
// Common.h

#pragma once

#include "DemoClass.h"

class Common {
public:
    static const DemoClass kProblemSymbol;
};
```

```c++
// Common.cpp

#include "Common.h"

const DemoClass Common::kProblemSymbol(888);
```


kProblemSymbol的类型是DemoClass，定义如下：

```c++
// DemoClass.h

#pragma once

#include <stdio.h>

class DemoClass {
public:
    DemoClass(int value);
    ~DemoClass();

    int value() const;

private:
    int* mValue;
};
```

```c++
// DemoClass.cpp

#include "DemoClass.h"

DemoClass::DemoClass(int value) {
    printf("%s:%d this:%p\n", __func__, __LINE__, this);
    mValue = new int(value);
}

int DemoClass::value() const {
    return *mValue;
}

DemoClass::~DemoClass() {
    printf("%s:%d this:%p\n", __func__, __LINE__, this);
    delete mValue;
}
```

### 1.2 libA.so

libA.so提供了函数printInLibA供外部使用，printInLibA中打印kProblemSymbol的值：

```c++
// A.h

#pragma once

void printInLibA();
```

```c++
// A.cpp

#include "A.h"

#include "Common.h"

void printInLibA() {
    printf("%s:%d kProblemSymbol:%d\n", __func__, __LINE__, Common::kProblemSymbol.value());
}
```

### 1.3 libB.so

libB.so和libA.so基本上一样，提供了函数printInLibB供外部使用，printInLibB中打印kProblemSymbol的值：

```c++
// B.h

#pragma once

void printInLibB();
```

```c++
// B.cpp

#include "B.h"

#include "Common.h"

void printInLibB() {
    printf("%s:%d kProblemSymbol:%d\n", __func__, __LINE__, Common::kProblemSymbol.value());
}
```

### 1.4 可执行程序

可执行程序依次调用libA.so和libB.so提供的printInLibA和printInLibB函数：

```c++
// main.cpp

#include "A.h"
#include "B.h"

int main() {
    printInLibA();
    printInLibB();
    return 0;
}
```

编译运行，这个程序是必挂的：

```console
DemoClass:5 this:0x7f42bfc18060
DemoClass:5 this:0x7f42bfc18060
printInLibA:7 kProblemSymbol:888
printInLibB:7 kProblemSymbol:888
~DemoClass:14 this:0x7f42bfc18060
~DemoClass:14 this:0x7f42bfc18060
free(): double free detected in tcache 2
Aborted (core dumped)
```

> 从运行日志可以看出，kProblemSymbol确实被构造/析构两次，并且两次构造内存地址是一样的。
{: .prompt-info }

接下来我们分析下为什么会这样呢。

## 2. 原因分析

接下来的内容涉及elf相关知识，如果不熟悉，可以参考：[《浅析elf中的.bss和.data
》](http://tech.coderhuo.tech/posts/elf_bss_data_segment/)。

### 2.1 编译期分析

首先执行 `readelf -s libA.so -W 100` 查看符号信息（截取部分）：

```
   Num:    Value          Size Type    Bind   Vis      Ndx Name
    14: 0000000000004060     8 OBJECT  GLOBAL DEFAULT   26 _ZN6Common14kProblemSymbolE
```

接着执行 `readelf -t libA.so` 查看section信息（截取部分）：
```
Section Headers:
  [Nr] Name
       Type              Address          Offset            Link
       Size              EntSize          Info              Align
       Flags
  [26] .bss
       NOBITS           0000000000004058  0000000000003058  0
       0000000000000010 0000000000000000  0                 8
       [0000000000000003]: WRITE, ALLOC
```


执行 `c++filt _ZN6Common14kProblemSymbolE` 可以看到这正是符号`Common::kProblemSymbol`：
 - 它是全局可见的符号（`GLOBAL`、`DEFAULT`属性）
 - 它位于elf的.bss section（Ndx 26对应.bss）

 > kProblemSymbol虽然初始化了却在.bss，而不是.data或者.rodata，是因为它不是POD类型的。对于非POD对象，C++标准允许编译器只在.bss分配存储空间，具体值在运行期初始化。
{: .prompt-info }


对libB.so执行同样的操作，可以看到，**libB.so中也包含符号kProblemSymbol，并且属性和libA.so中的kProblemSymbol是一模一样的**。可执行程序最终会用谁的kProblemSymbol呢？

### 2.2 运行期分析

背景知识：
  - 动态库中的全局/静态变量，在so加载的时候初始化，so卸载的时候销毁
  - 各动态库本身是没有全局视角的，它们只知道自己包含哪些符号，不知道是否和别的动态库冲突
  - 动态链接器有全局视角，它在运行期负责动态库中的符号绑定

回到我们的例子：
 - 初始化阶段，加载libA.so的时候，libA.so中的全局/静态变量被初始化，加载libB.so的时候，libB.so中的全局/静态变量被初始化
 - 退出阶段，libA.so、libB.so分别释放各自的全局/静态变量
 - 所以我们会看到kProblemSymbol有两次构造、两次析构，这是正常行为，因为libA.so和libB.so都需要构造、释放自己的kProblemSymbol对象；不正常的是，为啥这两个对象内存地址相同

执行命令`LD_DEBUG=all,files,symbols,bindings ./main`运行程序，跟踪下符号绑定过程（截取部分）：

```
535576:	relocation processing: libB.so (lazy)
535576:	symbol=_ZN6Common14kProblemSymbolE;  lookup in file=./main [0]
535576:	symbol=_ZN6Common14kProblemSymbolE;  lookup in file=libA.so [0]
535576:	binding file libB.so [0] to libA.so [0]: normal symbol `_ZN6Common14kProblemSymbolE'

535576:	relocation processing: libA.so (lazy)
535576:	symbol=_ZN6Common14kProblemSymbolE;  lookup in file=./main [0]
535576:	symbol=_ZN6Common14kProblemSymbolE;  lookup in file=libA.so [0]
535576:	binding file libA.so [0] to libA.so [0]: normal symbol `_ZN6Common14kProblemSymbolE'
```

可以看到：
 - **libA.so和libB.so中的kProblemSymbol都绑定到了同一个对象，即libA中的kProblemSymbol，所以二者内存地址相同**
 - 这是因为kProblemSymbol本身是全局可见的，可执行程序加载libA.so后，进程的全局符号表中就记录了kProblemSymbol，接下来加载libB.so时，复用了已存在的符号
 - 之所以绑定到了libA而不是libB，是因为可执行程序先加载的libA，这是由可执行程序Makefile中LDFLAGS中二者的顺序决定的（感兴趣的同学可以修改下顺序看看是不是都绑定到libB了）

 回到我们的问题：
  - 从动态链接器的角度看，libA和libB中的kProblemSymbol就是同一个对象
  - 但是libA和libB都以为kProblemSymbol是自己的，所以分别对其进行了构造、析构
  - 对同一个对象进行多次构造、析构本身就是个危险动作，比如示例程序中，就出现了double free导致的crash

如果libA和libB中的kProblemSymbol被分配了不同的内存，也不会出问题的。示例程序之所以出问题，是因为**内存分配**和**执行构造函数**被分成了两步，导致libB中的kProblemSymbol没分配新内存。我认为这是编译工具链（gcc）和运行时动态链接器（ld-linux.so）没配合好导致的（据说windows平台不存在这类问题，未考证）。

该问题解决方案也很简单，libCommon就不应该是静态库。网上也可以看到一些其他解决方法，我认为都是些治标不治本的方法。