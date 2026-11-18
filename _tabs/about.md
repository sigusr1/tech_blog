---
# the default layout is 'page'
icon: fas fa-info-circle
order: 4
---

我是一名**嵌入式软件工程师**，在**车载行业**做系统软件研发。本站是我的一块技术自留地，记录一些技术点滴，内容主要涉及**Linux/Android**平台的一些原理、问题排查手段，以及一些**网络协议**、**AI**相关内容。

{: .prompt-info }
> **目前在看车载、具身智能、AI方向的机会**，欢迎联系：
>
> - 微信：coderhuo
> - 邮箱：wtx05@126.com
> - GitHub：[github.com/sigusr1](https://github.com/sigusr1)

## 文章汇总

{: .prompt-tip }
> 本站自 2015 年起持续记录，共 **70 篇**原创技术文章，分为以下 11 类：
>
> - AI 工程化（6 篇）
> - Android / AOSP（8 篇）
> - Linux 系统编程与问题定位（10 篇）
> - 虚拟内存专题（6 篇）
> - 网络与协议（8 篇）
> - 车载以太网与自动驾驶中间件（6 篇）
> - 编程语言与软件设计（10 篇）
> - 编译、链接与工具链（6 篇）
> - 存储（4 篇）
> - 思考与读书（3 篇）
> - 工具技巧与杂项（3 篇）

### AI 工程化（6 篇）

- [JibarOS 简介：Android AICore 开源实现方案](/posts/jibaros_aicore/) `2026`
- [Agent 工具调用过程简介](/posts/agent_tool_call/) `2026`
- [如何通过 LangChain 访问 Dify 平台绑定的大模型](/posts/dify_plugin_for_langchain/) `2026`
- [自我蒸馏：抓取 Android trace 并分析 cpu 占用的 Skill](/posts/android_auto_trace_skill/) `2026`
- [AI 帮我优化了一个动态库](/posts/so_slimming/) `2026`
- [惊呆了：改了三个字节，AI 修好了一个跑不起来的 adb](/posts/ai_modify_binary/) `2026`

### Android / AOSP（8 篇）

- [AppFunctions 简介：Android 原生 MCP 方案](/posts/android_appfunctions/) `2026`
- [Android USAP 简介](/posts/android_usap/) `2025`
- [Android HWASan 简介](/posts/android_hwasan/) `2025`
- [Android 智能指针 sp/wp 原理](/posts/android_shared_ptr/) `2025`
- [Android 中信号 SIGQUIT 的特殊作用](/posts/android_sigquit/) `2025`
- [Android AOSP 代码中也有这种低级错误](/posts/google_aosp_bad_code/) `2025`
- [Android 模拟器如何 remount](/posts/remount_android/) `2026`
- [如何用模拟器调试 Android Framework](/posts/android_framework_debug/) `2026`

### Linux 系统编程与问题定位（10 篇）

- [libuv spin 空转导致的 cpu 消耗问题](/posts/libuv_cpu_problem/) `2025`
- [Cache 一致性导致的踩内存问题](/posts/DMA_mem_crash/) `2019`
- [arm 平台根据栈进行 backtrace 的方法](/posts/arm_backtrace/) `2017`
- [如何利用硬件 watchpoint 定位踩内存问题](/posts/arm_hardware_breakpoint/) `2019`
- [如何在实时操作系统(RTOS)中使用 GCC 栈溢出保护(SSP)功能](/posts/gcc_stack_protect_on_rtos/) `2019`
- [fork 导致的死锁问题](/posts/dead_lock_after_fork/) `2021`
- [Linux capability 多线程权限泄露示例](/posts/linux_capability_example/) `2022`
- [uid/gid 变更导致无法生成 coredump 的问题](/posts/setuid_not_gen_coredump/) `2022`
- [串口通信中 ICRNL 惹的祸](/posts/serial_translate_problem/) `2017`
- [Linux 控制台重定向方法](/posts/linux_console_redirect/) `2019`

### 虚拟内存专题（6 篇）

- [虚拟内存探究，深入理解进程地址空间（专题汇总）](/posts/Virtual_Memory_summary/) `2019`
- [第一篇：C strings & /proc](/posts/Virtual_Memory_C_strings_proc/) `2017`
- [第二篇：Python 字节](/posts/Virtual_Memory_python_bytes/) `2017`
- [第三篇：一步一步画虚拟内存图](/posts/Virtual_Memory_drawing_VM_diagram/) `2017`
- [第四篇：malloc, heap & the program break](/posts/Virtual_Memory_malloc_and_heap/) `2017`
- [第五篇：The Stack, registers and assembly code](/posts/Virtual_Memory_malloc_and_heap_stack_and_register/) `2019`

### 网络与协议（8 篇）

- [libcurl blind sleep 导致的耗时问题](/posts/libcurl_sleep_problem/) `2026`
- [车云通信怎么互认身份：双向认证与一车一密](/posts/vehicle_cloud_mtls/) `2026`
- [分析 RTP 码流卡顿问题的工具](/posts/rtp_parse_tool/) `2018`
- [利用空口抓包分析 Wi-Fi 问题](/posts/wifi_package_capture/) `2018`
- [TCP 连接建立、断开过程详解](/posts/tcp_establish_release/) `2018`
- [记一次文件下载失败问题定位过程](/posts/file_download_fail/) `2017`
- [如何制作自签名证书](/posts/how_to_make_self_signed_cert/) `2016`
- [udhcp 的几点总结](/posts/udhcp_summury/) `2015`

### 车载以太网与自动驾驶中间件（6 篇）

- [HaloOS vbslite 初探](/posts/first_impressions_of_haloos_vbslite/) `2025`
- [Cyber RT 模块加载流程简介](/posts/CyberRT_module_loader/) `2021`
- [cyber_recorder 报文录制简介](/posts/apollo_cyber_recorder/) `2021`
- [AVB 简介 -- 第一篇：概述](/posts/AVB_summury/) `2020`
- [AVB 简介 -- 第二篇：gPTP 简介](/posts/gptp_summury/) `2020`
- [AVB 简介 -- 第三篇：AVTP 简介](/posts/avtp_summury/) `2020`

### 编程语言与软件设计（10 篇）

- [日常工作中的设计：解耦和封装（视频链路架构重构案例）](/posts/abstraction_and_encapsulation/) `2020`
- [共享内存中自建 hash 的一种方法](/posts/hash_in_shared_memory/) `2021`
- [跨平台 printf 封装方法](/posts/wrap_printf_over_platform/) `2018`
- [程序中的整数](/posts/number_in_computer/) `2019`
- [移位溢出](/posts/left_shift_overflow/) `2017`
- [ARM 下 char 类型符号问题](/posts/arm_char_problem/) `2017`
- [可怕的 extern 关键字](/posts/extern_trap/) `2017`
- [JavaScript 中的 this 指针与回调函数](/posts/javascript_this_and_callback/) `2023`
- [Bug 分享：栈上的临时变量赋值给 class 的引用型成员](/posts/ref_mem_of_class/) `2023`
- [Json-C 用法释疑](/posts/usage_of_jsonc/) `2017`

### 编译、链接与工具链（6 篇）

- [浅析 elf 中的 .bss 和 .data](/posts/elf_bss_data_segment/) `2024`
- [静态库重复链接导致的 crash 问题](/posts/double_link_global_symbol/) `2024`
- [curl 交叉编译方法](/posts/curl_cross_build/) `2017`
- [Ubuntu 上搭建 arm 虚拟运行环境](/posts/arm_simulation_on_ubuntu/) `2018`
- [gcc 编译临时文件存放路径](/posts/gcc_tmp_file/) `2017`
- [去掉宏 \_\_FILE\_\_ 的路径](/posts/__FILE__strip_path/) `2017`

### 存储（4 篇）

- [TF 卡存储性能调优案例分析](/posts/tf_write_problem/) `2020`
- [NAND Flash 基础知识简介](/posts/flash_basics/) `2020`
- [Flash 写入性能下降问题](/posts/flash_write_performance/) `2020`
- [文件碎片对 Flash 性能的影响](/posts/file_fragment/) `2020`

### 思考与读书（3 篇）

- [他说的一定对吗？-- 请保持独立思考的习惯](/posts/is_he_right/) `2018`
- [《软件工程思想》-- 赏析](/posts/software_project/) `2020`
- [技术书籍分享](/posts/tech_book_share/) `2017`

### 工具技巧与杂项（3 篇）

- [Nginx 反向代理，建立 Google 镜像](/posts/nginx_google_proxy/) `2017`
- [Source Insight 集成 svn 命令](/posts/source_insight_svn_cmd/) `2015`
- [华为手机安装 Google 服务的方法](/posts/google_service_for_huawei/) `2018`
