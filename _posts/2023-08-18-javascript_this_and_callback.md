---
title: "JavaScript中的this指针与回调函数"
date: 2023-08-18
categories: [编程语言]
tags:  [JavaScript]  
---

在常规的面向对象语言中(比如C++)，this指针的指向是确定的。但在JavaScript中，this指向依赖于运行环境。


下面的例子，预期的输出是`nihao`：

```js
class A {
    setEventListener(func) {
        this.callback = func;
    }

    triggerEvent() {
        this.callback();
    }
 }

class B {
    constructor(a) {
        a.setEventListener(this.onEvent);
        this.localValue = "nihao";
    }

    onEvent() {
        console.log(this.localValue);
        // console.log(this);
    }
}

var a = new A();
var b = new B(a);

a.triggerEvent();
```

但实际输出是`undefined`。  
在onEvent中打印下`this`，可以看到，它指向的是对象`a`而不是`b`:


```console
A { callback: [Function: onEvent] }
```

这是因为在JavaScript中，this指向依赖于运行环境。上面的回调是被对象a执行的，所以`onEvent`的执行上下文是对象a。这有点像dart的Mixins。

可以在注册回调的时候，调用`bind`函数强制进行强制绑定，将下面的代码:

```js
a.setEventListener(this.onEvent);
```

改成：

```js
a.setEventListener(this.onEvent.bind(this));
```

这样就能得到预期的输出`nihao`。


关于JavaScript中this指针，可以参阅下面几篇文档：
- [https://github.com/Microsoft/TypeScript/wiki/%27this%27-in-TypeScript](https://github.com/Microsoft/TypeScript/wiki/%27this%27-in-TypeScript)
- [https://www.ruanyifeng.com/blog/2018/06/javascript-this.html](https://www.ruanyifeng.com/blog/2018/06/javascript-this.html)
- [https://developer.aliyun.com/article/790609](https://developer.aliyun.com/article/790609)
- [https://juejin.cn/post/6973974021138284558](https://juejin.cn/post/6973974021138284558)
- [https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/this](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/this)

