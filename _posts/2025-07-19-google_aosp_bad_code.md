---
title: "Android AOSP 代码中也有这种低级错误"
date: 2025-07-19  
categories: [杂谈]
tags: [Android, aosp]
---


最近因为工作原因看了些Android AOSP的代码，在zygote相关代码中看到一个低级错误，比较惊讶，Android这么成熟的平台，也会犯这种错误？

2021年的时候，AOSP中合入了这个[patch](https://android.googlesource.com/platform/frameworks/base/+/69d44b0bfd5d4a6721ab3dccf537a147af7b6d1d):

可以看到，这个patch中重复注册了JNI函数com_android_internal_os_Zygote_nativeAddUsapTableEntry：

![重复注册JNI函数](2025-07-19-google_aosp_bad_code/google_asop_bad_code.jpg)

这个Bug虽然不会造成功能性的影响，但给人的感觉就是，AOSP的代码review是不是不太严格？

google收到这个bug反馈后，已经转为assigned状态，也就是已经将这个bug分配给开发人员进行修复了：

![assigned状态](/2025-07-19-google_aosp_bad_code/aosp_assigned_my_bug_report.jpg)