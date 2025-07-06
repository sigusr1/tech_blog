---
title: "HaloOS第一观感"
date: 2024-06-27
categories: [操作系统]
tags:  [HaloOS]  
---


## 1. 代码风格

感觉HaloOS可能暂时没有统一的编程规范，下面举几个例子（本文的排版也很值得吐槽）：

- `vbslitespace/examples/local_test/app_test.c`中变量未对齐：

![变量未对齐](/2025-06-27-first_impressions_of_haloos/not_align_main_func.png)


- `vbslitespace/mvbs/posix_aux/src/loop.c`操作符前后是否加空格风格不一致：  

![操作符前后是否加空格风格不一致](/2025-06-27-first_impressions_of_haloos/space_before_and_after_oprator.png)


- `vbslitespace/mvbs/src/adapter/posix/src/adapter_socket.c`中tab和space混用：  

![tab和space混用](/2025-06-27-first_impressions_of_haloos/mix_tab_space.png)

