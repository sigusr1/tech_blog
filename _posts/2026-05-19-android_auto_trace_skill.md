---
title: "自我蒸馏：抓取Android trace并分析cpu占用的Skill"
date: 2026-05-19
categories: [AI]
tags:  [Skill]
---

分享一个Skill，它能自动抓取Android系统的Trace，并分析指定进程的CPU占用情况。

Skill链接：[android-auto-trace](https://github.com/sigusr1/myskills/tree/main/android-auto-trace)

下面是一个简单的使用示例：

```
帮我分析下进程com.test.agentdemo的cpu占用情况
```

- 首先通过已连接的Android设备调用Perfetto抓取Trace，触发命令中可指定抓取时长及自定义Trace Tag。
- 抓取完成后自动拉取至PC端，调用Trace Processor进行分析。若未安装Trace Processor，将尝试自动安装（可能会遇到网络问题，需自行解决梯子/代理问题）。
- 分析内容包括：
  - 指定进程总体CPU消耗
  - 指定进程内各线程的CPU消耗
  - 指定进程内CPU占用排名前10的Slice

下面是模型获取原始数据后的分析结果示例，不同模型的展示形式可能有所差异。

这是进程整体的CPU消耗情况，主要包含该进程总CPU消耗时间及占比：
![进程整体的cpu消耗情况](/assets/images/2026-05-19-android_auto_trace_skill/summary.jpg)

这是按线程分解的CPU消耗情况，主要展示每个线程的CPU消耗时长及占比（分母为该进程总CPU消耗时长）：
![按线程分解后的cpu消耗情况](/assets/images/2026-05-19-android_auto_trace_skill/per_thread.jpg)

这是按Slice分解的CPU消耗情况，统计指定进程内CPU占用排名前10的Slice，按同名Slice的CPU消耗总和进行排序：
![按slice分解后的cpu消耗情况](/assets/images/2026-05-19-android_auto_trace_skill/slice.jpg)


你也可以在[https://ui.perfetto.dev/](https://ui.perfetto.dev/)中打开Trace文件验证上述数据。以下是我的验证结果，数据能够对应上（部分细微差异是因为Perfetto工具中选取的时间范围不同）：

![Perfetto的分析](/assets/images/2026-05-19-android_auto_trace_skill/cpu_use_by_perfetto.jpg)


最后是模型基于上述信息生成的总结分析，告诉你CPU热点在哪里并提供优化建议。这部分是模型的自由发挥，也最考验模型能力。
![大模型的分析](/assets/images/2026-05-19-android_auto_trace_skill/analyse.jpg)

这个Skill仅在macOS上验证通过，理论上linux系统应该也可以用。

希望对你有所帮助😊


