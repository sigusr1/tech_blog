---
title: "Android智能指针sp/wp原理"
date: 2025-08-31
categories: [Android]
tags:  [智能指针]
---

谈到智能指针，大家首先想到的肯定是`std::shared_ptr`，其实Android binder中也实现了一套[智能指针sp/wp](https://cs.android.com/android/_/android/platform/system/core/+/refs/tags/android-14.0.0_r74:libutils/binder/include/utils/RefBase.h?hl=zh-cn)，原理类似，也是基于引用计数。Android之所以自己搞一套，估计是因为当时的C++标准还不完善。

我司的基础库中也有一套自研的智能指针，和Android中的实现类似。之所以写这篇文章，是因为最近发现这里面隐藏多年的一个bug：特定场景下weak_ptr虽然能提升为shared_ptr，但指向的却是已释放的对象。借此机会学习了下Android中的实现。


## 1. 杂谈：你的基础库可靠吗？

一般公司都会封装一些基础库给各业务模块使用，一方面可以提高开发效率，另一方面跨平台移植也方便。这些基础库经过一段时间的打磨完善后，成为各业务模块的基石。大家也都形成了一定的共识，它们是可靠的。

平时代码中出了bug，一般也不会怀疑到这些基础库，就像我们不会去质疑malloc/free这些基础函数的可靠性一样。事实也确实如此，平时遇到的问题基本上都是上层代码导致的，这更加坚定了大家的信念“基础库是可靠的”。
即使偶尔有人怀疑，一句“这个库运行多少年了，一行代码没改过”就可以把对方怼过去。  

但哪有没bug的代码，就像系统函数的man手册中NOTES部分，有些注意事项在一定程度上是不是也可以理解为`bug`。个人经验：
- 对于基础库的维护者，别盲目自信，特别是基础库的后爸后妈们，代码不是自己写的，你可能没那么了解它。
- 对于基础库的使用者，可以大胆假设，但一定要小心求证，比如总不能看到crash堆栈在libc中，就说libc有问题吧。

## 2. Android智能指针sp/wp基本用法
基本用法如下:
- 首先继承RefBase，它负责引用计数的管理
- sp/wp是强弱指针模版，等价于`std::shared_ptr`/`std::weak_ptr`
- promote函数尝试将弱指针提升为强指针，使用前需判断是否提升成功

```c++
   class Foo : virtual public RefBase { ... };

   // always construct an sp object with sp::make
   sp<Foo> myFoo = sp<Foo>::make(/*args*/);

   // if you need a weak pointer, it must be constructed from a strong pointer
   wp<Foo> weakFoo = myFoo; // NOT myFoo.get()

   // convert weak pointer to strong pointer
   sp<Foo> theirFoo = weakFoo.promote();
   if (theirFoo) { /*do something*/ }
```

相比于c++的智能指针，Android中的智能指针更像瑞士军刀，提供了更大的灵活性，比如RefBase的`incStrong`、`decStrong`都是对外暴露的，特定场景下可以脱离sp/wp操作对象生命周期，还可以通过`extendObjectLifetime`设置`OBJECT_LIFETIME_WEAK`扩展对象的生命周期（正常情况下强引用计数为0释放对象，该接口可以设置**强弱引用计数均为0时才释放对象**）。  

Android中这套智能指针机制最初从binder中诞生，后来作为通用组件给其他模块使用，有它自己的使用场景，这些特殊用法本文暂不涉及。


## 3. 实现原理

如下图所示：
- 强指针sp持有对象Foo的指针，还持有控制块指针，执行`incStrong`/`decStrong`对强引用计数+1/-1，当强引用计数为0的时候，释放Foo对象。
- 弱指针wp持有对象Foo的指针，也持有控制块指针，执行`incWeak`/`decWeak`对弱引用计数+1/-1，当弱引用计数为0时，释放控制块`weakref_impl`。
- 弱指针可以通过`promote`函数尝试转化为强指针。

![智能指针原理](http://data.coderhuo.tech/2025-08-31-android_shared_ptr/android_sp_wp.png)


控制块`weakref_impl`是在构造基类RefBase时创建的，也就是创建Foo对象的时候就会创建控制块，控制块和对象Foo不在同一个内存块。`weakref_impl`中最关键的两个变量是强引用计数`mStrong`、弱引用计数`mWeak`:
```c++
// 基类RefBase的构造函数中分配了控制块weakref_impl
RefBase::RefBase()
    : mRefs(new weakref_impl(this))
{
}

// 控制块weakref_impl中包含强引用计数mStrong、弱引用计数mWeak
class RefBase::weakref_impl : public RefBase::weakref_type
{
public:
    std::atomic<int32_t>    mStrong;
    std::atomic<int32_t>    mWeak;
    RefBase* const          mBase;
    std::atomic<int32_t>    mFlags;

    explicit weakref_impl(RefBase* base)
        : mStrong(INITIAL_STRONG_VALUE)
        , mWeak(0)
        , mBase(base)
        , mFlags(OBJECT_LIFETIME_STRONG)
    {
    }
};
```

下面代码展示了sp构造的时候执行`incStrong`把强引用计数+1，析构的时候执行`decStrong`把强引用计数-1(弱指针wp类似)：
```c++
//强引用计数+1
template<typename T>
sp<T>::sp(T* other)
        : m_ptr(other) {
    if (other) {
        other->incStrong(this);
    }
}

//强引用计数-1
template<typename T>
sp<T>::~sp() {
    if (m_ptr)
        m_ptr->decStrong(this);
}
```

**智能指针的线程安全是如何保证的？**  
考虑下面的代码，foo1、foo2都指向同一个Foo对象，并且是双线程并发，是否会造成引用计数错乱？不会的，因为控制块`weakref_impl`中，无论是`mStrong`还是`mWeak`，类型都是`std::atomic`：

```c++
   sp<Foo> myFoo = sp<Foo>::make(/*args*/);

   std::thread t1([]() {sp<Foo> foo1 = myFoo;});
   std::thread t2([]() {sp<Foo> foo2 = myFoo;});

```

**多线程情况下，有没有可能多个线程同时释放Foo对象？**  
肯定不会的，关键就在于下面的`fetch_sub`函数，首先它是原子操作，其次它的返回值是执行`fetch_sub`前的旧值，这可以保证只有一个线程的返回值c是1，也就保证了只有一个线程可以执行`delete this`，相当于实现了线程间的互斥。

```c++
void RefBase::decStrong(const void* id) const
{
    weakref_impl* const refs = mRefs;
    const int32_t c = refs->mStrong.fetch_sub(1, std::memory_order_release);
    // 强引用计数为0时是否this，也就是上面例子中的Foo对象
    if (c == 1) {
        std::atomic_thread_fence(std::memory_order_acquire);
        refs->mBase->onLastStrongRef(id);
        int32_t flags = refs->mFlags.load(std::memory_order_relaxed);
        if ((flags&OBJECT_LIFETIME_MASK) == OBJECT_LIFETIME_STRONG) {
            delete this;
            // The destructor does not delete refs in this case.
        }
    }

    // 通过弱引用计数-1决定是否是否控制块，为0则释放

    // Note that even with only strong reference operations, the thread
    // deallocating this may not be the same as the thread deallocating refs.
    // That's OK: all accesses to this happen before its deletion here,
    // and all accesses to refs happen before its deletion in the final decWeak.
    // The destructor can safely access mRefs because either it's deleting
    // mRefs itself, or it's running entirely before the final mWeak decrement.
    //
    // Since we're doing atomic loads of `flags`, the static analyzer assumes
    // they can change between `delete this;` and `refs->decWeak(id);`. This is
    // not the case. The analyzer may become more okay with this patten when
    // https://bugs.llvm.org/show_bug.cgi?id=34365 gets resolved. NOLINTNEXTLINE
    refs->decWeak(id);
}
```

**弱指针是如何提升为强指针的？**  

`promote`函数中尝试对强引用计数+1，如果+1成功则转换成功，否则转换失败：
```c++
template<typename T>
sp<T> wp<T>::promote() const
{
    sp<T> result;
    if (m_ptr && m_refs->attemptIncStrong(&result)) {
        result.set_pointer(m_ptr);
    }
    return result;
}
```

`attemptIncStrong`函数尝试对强引用计数+1，这里的关键是原子操作函数`compare_exchange_weak`函数，结合下面代码片段，它的语义是：如果mStrong的值和curCount相等，则将mStrong设置为curCount+1（即+1操作），否则将curCount设置为mStrong的当前值，这保证了线程间的同步。

```c++
bool RefBase::weakref_type::attemptIncStrong(const void* id)
{
   // 无论成功与否，弱引用计数先+1
    incWeak(id);

    weakref_impl* const impl = static_cast<weakref_impl*>(this);
    int32_t curCount = impl->mStrong.load(std::memory_order_relaxed);

    // 尝试对强引用计数+1
    while (curCount > 0 && curCount != INITIAL_STRONG_VALUE) {
        // we're in the easy/common case of promoting a weak-reference
        // from an existing strong reference.
        if (impl->mStrong.compare_exchange_weak(curCount, curCount+1,
                std::memory_order_relaxed)) {
            break;
        }
    }

    // 省略了特殊情况下的处理逻辑(强指针已经全部释放，或者压根没产生过强指针)
   if (curCount <= 0 || curCount == INITIAL_STRONG_VALUE) {
   }

    return true;
}
```