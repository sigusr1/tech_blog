---
title: "AI帮我优化了一个动态库"
date: 2026-08-04
categories: [AI]
tags:  [AI]
---

最近在排查一个崩溃问题时，发现两个现象：

1. 一个跨平台动态库，有的平台 tombstone 能解析出函数名，有的却解析不出来；
2. 补上符号化能力后，又发现 so 体积大得离谱——比 Android AOSP 编出来的大了近 3 倍。

最终查明，问题1是因为基于CMake的构建脚本没有注入符号，问题2是编译优化选项差异导致的。  
优化后，tombstone 不仅能解析出函数名，so 反而从 9.6MB 瘦身到 4.6MB。

## 1. 背景

主角是一个车机系统上的 C++ 中间件库 `libxx.so`，
静态链接了一票三方库并把它们的 API 隐藏起来，对外只暴露 `libxx` 自己的接口，形成自闭环。

同一份源码，对应三种编译环境、三种部署方式：

| 部署方式 | 编译环境 | 工具链 |
|---|---|---|
| 自研车机 OS 内置（下称**自研系统版**） | 交叉编译脚本 + CMake | GNU (gcc) |
| Android APK 集成（下称 **AAR 版**） | Gradle/NDK 打 AAR | clang (NDK) |
| Android 系统内置（下称 **AOSP 版**） | AOSP 源码树内编译 | clang (Soong) |

## 2. 问题一：崩溃没有函数名

线上崩溃时，三个版本的表现截然不同：**AOSP 版的 tombstone 每一帧都有函数名，
另外两个版本却只有裸偏移**：

```
#5  0x0000007fa5af17f0 in ?? () from /usr/lib64/libxx.so
#6  0x0000007fa5af1810 in ?? () from /usr/lib64/libxx.so
#7  0x0000007fa5d18cd8 in ?? () from /usr/lib64/libxx.so
```

同一份代码、同样被 strip，为什么 AOSP 版可以解析出函数名？

### 2.1 AOSP 自带 MiniDebugInfo

MiniDebugInfo 的工作原理可以用三步概括：

1. 从 strip 前的 so 中，找出所有**不在 `.dynsym` 里的内部函数符号**；
2. 生成一个只含这些符号的 mini ELF，xz 压缩后以 `.gnu_debugdata`
   section 的形式塞回 so；
3. 正常 strip so, strip 不会动 `.gnu_debugdata`。

AOSP 的 Soong 构建系统在 strip 阶段默认就走这个流程：
`build/soong/scripts/strip.sh --keep-mini-debug-info`。

崩溃时，`libunwindstack`（`debuggerd` 调用它来 unwind 栈）原生支持解压
这个 section 并做第二轮符号查找，于是 tombstone 全帧可读。**这是 AOSP 构建产物的标配**，而自研系统版和
AAR 版是自己写的构建脚本，没有这一步，所以无法解析出函数名。

### 2.2 补齐另外两个版本

**生产端**：写一个脚本，hook 到另外两个版本的构建流程中，在 strip 之前
自动完成 MiniDebugInfo 注入（做法参照 AOSP 的 `strip.sh`）。流程跟 2.1
描述的三步一致：先用 `nm` 提取未导出的内部函数符号，再用 `objcopy`
生成只含这些符号的 mini ELF，xz 压缩后注入 `.gnu_debugdata` section。

**消费端**：Android 的 debuggerd 原生支持 `.gnu_debugdata`，但自研 OS 的
crash handler 用的是 libunwind，需要开启 `-DHAVE_LZMA`（启用 lzma 解压）
才能解析。启用后 libunwind 的查找流程：

1. 先在主 ELF 的 `.dynsym`/`.symtab` 里 `lookup_symbol`；
2. 再调 `extract_minidebuginfo` 解压 `.gnu_debugdata` 得到 mini ELF；
3. 如果解压成功，再对解压出来的 mini ELF 做一次 `lookup_symbol`。

至此三个版本的 tombstone 都有函数名了。但新的问题来了。

## 3. 问题二：so 大小问题

注入 MiniDebugInfo 后，自研系统版的 so 从 9.6MB 涨到了 **12.7MB**。涨一点
可以理解（塞了符号表进去），但 3MB+ 的增量不对劲。更扎心的是横向对比：
**AOSP 版的同源码产物，带着 MiniDebugInfo 也才 3.7MB**。

差距不是一点半点，值得把两个 so 彻底解剖一遍。

### 3.1 先量化：readelf 按 section 拆解

```bash
readelf -SW libxx.so   # 关注每个 section 的 Size 列
```

自研系统版产物（12.7MB）的构成：

| section | 大小 | 说明 |
|---|---|---|
| `.text` | 5.10 MB | 代码 |
| `.gnu_debugdata` | 3.06 MB | 刚注入的 MiniDebugInfo（压缩后！）|
| `.eh_frame` | 1.51 MB | 栈展开表 |
| `.rodata` | 0.98 MB | 只读数据 |
| 其他 | ~2MB | 重定位表、GOT 等 |

两个疑点：
- `.gnu_debugdata` 一个"符号表"占了 24%；
- `.text` 是 AOSP 版（2.2MB）的两倍多。

两个疑点分别对应下面的两次瘦身。

### 3.2 瘦身一：`.gnu_debugdata` 从 3.06MB 到 134KB

把 `.gnu_debugdata` 按 section 偏移 dd 出来、xz 解压，看看 mini ELF 里
到底装了什么：

```
uncompressed mini ELF: 17.3 MB (!!)
.strtab   6.18 MB   ← 符号名字符串，需要
.text     5.10 MB   ← 完整代码拷贝？？
.eh_frame 1.51 MB   ← 又一份拷贝？？
.symtab   1.26 MB   ← 符号表，需要
.rodata   0.98 MB   ← 还是拷贝？？
```

真相大白：第一版注入脚本抄 AOSP 配方时漏了 `--only-keep-debug` 这步，直接
对原库做了：

```bash
objcopy --strip-all --keep-symbols=keep_list in.so mini_debuginfo
```

`--strip-all` 会保留 **ALLOC section 的内容**（.text/.rodata/.eh_frame
都是 ALLOC 的），于是整个库的代码和数据被原样打包进了"符号表"里，再 xz 压缩
一遍塞回库中——等于把自身又复制了一份。

修复方法是补上 `--only-keep-debug`：它把 ALLOC section 的内容清空为
NOBITS（解析的时候只需 section 头里的地址做映射，不需要实际内容），之后再
strip + keep-symbols。

效果：mini ELF 里只剩 symtab + strtab（约 7.8MB），xz 压完 **134KB**。
`.gnu_debugdata` 直接砍掉 96%，和 AOSP 版的 `.gnu_debugdata` 基本对齐。

### 3.3 瘦身二：编译优化

`.text` 为什么是 AOSP 版的两倍多？用 AOSP 版产物做对照组，逐 section 对比：

| section | AOSP 版（clang） | 自研系统版（gcc） | 差距 |
|---|---|---|---|
| `.text` | 2.20 MB | 5.10 MB | **2.3x** |
| `.eh_frame`(+hdr) | 0.36 MB | 1.82 MB | **5.1x** |
| `.rodata` | 0.42 MB | 0.98 MB | 2.3x |

同一份代码，每个 section 都膨胀数倍——这不是"gcc 和 clang 的编译器差异"
能解释的量级，一定是构建配置出了问题。查看自研系统版的交叉编译脚本，编译参数长
这样：

```
-I... -g -fvisibility=hidden -fstack-protector --sysroot=...
```

**没有任何 `-O` 选项。** GCC 的默认优化级别是 `-O0`——也就是说这个so一直跑的是无优化代码：每个变量都读写
内存、零内联、指令量膨胀 2~3 倍，体积和性能双输。

> 这种事比想象中常见：CMake 项目如果既不设 `CMAKE_BUILD_TYPE`、又不在
> `CMAKE_CXX_FLAGS` 里显式给 `-O`，就会静默地以 -O0 编译。而 AOSP/NDK
> 的构建系统默认就是 `-O2` + gc-sections，这正是 AOSP 版产物小得多的主因。

修复三件套（向 AOSP 的默认行为看齐）：

```bash
CFLAGS  += -O2 -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections
```

- `-O2`：标准生产级优化，`.text` 直接减半；
- `-ffunction-sections -fdata-sections`：每个函数/数据独立成段，把链接器
  的裁剪粒度从"编译单元"细化到"函数"；
- `--gc-sections`：链接器从导出符号出发做可达性分析，没人引用的段直接扔。
  对静态链接了大量第三方库、实际只用一小部分 API 的场景（正是我们）效果极好。

三个参数是组合拳：前两个编译参数本身不减体积（甚至微增），价值全在为
`--gc-sections` 铺路。

### 3.4 最终战果

两轮修复后，达到以下效果：

1. **体积**：12.76 MB → 4.63 MB，缩减 64%
2. **符号化**：崩溃 tombstone 全部栈帧带函数名
3. **性能**：优化级别从 -O0 → -O2，指令量和内存访问大幅减少

各阶段明细：

| 阶段 | 体积 | 手段 |
|---|---|---|
| 初始（含臃肿 MiniDebugInfo） | 12.76 MB | — |
| 修复 mini ELF 生成 | 9.99 MB | `--only-keep-debug` |
| 开优化 + 链接裁剪 | **4.63 MB（-64%）** | `-O2` + sections + `--gc-sections` |

继续往下还有 LTO（预计再省 15~25%）和 `-Os` 可选，但收益递减，而且会引入其他成本，
我们选择停在这里——4.63MB 与 AOSP 版 3.7MB 的剩余差距，推测主要来自
工具链差异（GCC vs Clang + LTO + lld ICF）。

## 4. 题外话

在这个问题排查过程中，我给AI输入了以下信息：
1. 三种部署方式对应的完整工程仓库，并指明libxx.so源码目录（源码及上下游仓库一定要给全，否则效果大打折扣）
2. 三个工程的编译方式
3. 我的问题

之后就是不断循环：AI 分析问题、给方案，我来判断、验证。
