---
title: "Bug分享：栈上的临时变量赋值给class的引用型成员"
date: 2023-09-09
categories: [Bug分享]
tags:  [Bug分享]
---


最近遇到一个开源库中的[bug](https://gitee.com/jeremyczhen/fdbus/issues/ICXEWG)，简化后的示例代码如下：
- 类成员变量`mValue`是个**引用**
- 函数`doTask`中创建一个`Demo`对象，其中`mValue`指向栈上的临时变量`a`
- 创建的`Demo`对象交给另一个线程使用，另一个线程操作`Demo`对象的时候，因为函数`doTask`已返回，栈上的临时变量`a`已失效，导致踩内存

```c++
#include <string.h>

#include <chrono>
#include <thread>

class Demo {
public:
    Demo(int& value) : mValue(value) {
    }

    int& mValue;
};

void doTask() {
    int a = 1;
    printf("Address range of a:%p\n", &a);

    auto ptr = std::make_shared<Demo>(a);
    std::thread workThread([ptr]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // 栈内存被踩：doTask函数已结束，ptr->mValue指向的变量a已失效
        ptr->mValue = 2;
    });

    workThread.detach();
}

void myValueChange() {
    char b[1024];
    memset(b, 0, sizeof(b));
    printf("Address range of b:[%p, %p)\n", b, b + sizeof(b));

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 数组b的内存被踩
    for (uint32_t i = 0; i < sizeof(b); i++) {
        if (b[i] != 0) {
            printf("value of b[%d] is change to %d\n", i, b[i]);
        }
    }
}

int main() {
    doTask();
    myValueChange();
    return 0;
}
```

直接编译运行上面的代码，可以看到函数`myValueChange`中数组`b`的内存被修改了，躺着中枪：

```
Address range of a:0x7ffdc0cc37c0
Address range of b:[0x7ffdc0cc33e0, 0x7ffdc0cc37e0)
value of b[992] is change to 2
```

实际工程中，这种偶现的踩内存问题比较难排查，因为罪魁祸首是A线程，但是最终程序可能crash在B线程，犯罪现场已被破坏。上面的demo，在我的`Ubuntu 20.04.4 LTS`上能逃过`-fstack-protector`、`Asan`的法眼，Android平台开启`HWASan`倒是可以检测出来。