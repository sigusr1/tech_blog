---
title: "Android系统的Cloud AI与On-Device AI"
date: 2025-11-15
categories: [Android]
tags:  [AIOS]
---

我们目前使用的AI功能，基本上都是跑在云端的。比如豆包，给张示意图

手机厂商不一定会用Google的AI

这张图不错：https://medium.com/@sahin.samia/on-device-ai-what-it-is-and-how-it-works-89721ee68792

# Android系统AI能力现状
如下所示，Android为应用开发者提供了三种使用AI的方式：

+ App使用端侧AI Core，模型是Gemini Namo，不可替换
+ <font style="color:rgb(32, 33, 36);">App使用云端模型</font>
+ App使用端侧私有模型，通过LiteRT for Android，将私有模型转换为可以在Android上运行的模型

![](https://intranetproxy.alipay.com/skylark/lark/0/2025/png/341874/1763041310304-9a0a0047-9e4e-46a1-a99b-06167830d678.png)



开发者具体应该选择哪种方案，Android也根据任务类型、不同需求给出了[指导意见](https://developer.android.com/ai/overview#ai-solution-guide)：

![](https://intranetproxy.alipay.com/skylark/lark/0/2025/svg/341874/1763043439878-fbeccb70-caf4-4f16-8032-3aa74247e63e.svg)



总体感觉，Android在AI上的布局是全方位的，既考虑了端侧模型，也考虑了云端模型，并针对不同场景提供了SDK支持，提升了开发体验。

## 端侧AI Core
端侧AI Core内置模型是Gemini Namo，可以通过[GenAI APIs](https://developer.android.com/ai/gemini-nano/ml-kit-genai)访问，支持以下功能：

+ [**<font style="color:rgb(26, 115, 232);">Summarization</font>**](https://developers.google.com/ml-kit/genai/summarization/android)<font style="color:rgb(32, 33, 36);">: Summarize articles or chat conversations as a bulleted list.</font>
+ [**<font style="color:rgb(26, 115, 232);">Proofreading</font>**](https://developers.google.com/ml-kit/genai/proofreading/android): Polish short content by refining grammar and fixing spelling errors.
+ [**<font style="color:rgb(26, 115, 232);">Rewriting</font>**](https://developers.google.com/ml-kit/genai/rewriting/android)<font style="color:rgb(32, 33, 36);">: Rewrite short messages in different tones or styles.</font>
+ [**<font style="color:rgb(26, 115, 232);">Image description</font>**](https://developers.google.com/ml-kit/genai/image-description/android)<font style="color:rgb(32, 33, 36);">: Generate a short description of a given image.</font>
+ [**<font style="color:rgb(26, 115, 232);">Prompt</font>**](https://developers.google.com/ml-kit/genai/prompt/android)<font style="color:rgb(32, 33, 36);">: Generate text content based on a custom text-only or multimodal prompt.</font>

![](https://intranetproxy.alipay.com/skylark/lark/0/2025/png/341874/1763042672061-0c386465-2364-4815-aa1e-fc8e51774796.png)

## <font style="color:rgb(32, 33, 36);">云端模型</font>
<font style="color:rgb(32, 33, 36);">云端模型通过</font>[Firebase Android SDK](https://developer.android.com/ai/gemini)<font style="color:rgb(32, 33, 36);">访问，细分为：</font>

+ [Gemini Developer API](https://developer.android.com/ai/gemini/developer-api)<font style="color:rgb(32, 33, 36);">：面向普通开发者</font>
+ [Vertex AI Gemini API](https://developer.android.com/ai/vertex-ai-firebase)<font style="color:rgb(32, 33, 36);">：面向企业</font>
+ [Gemini Live API](https://developer.android.com/ai/gemini/live)<font style="color:rgb(32, 33, 36);">：实时低延迟场景</font>
+ [Imagen](https://developer.android.com/ai/imagen)<font style="color:rgb(32, 33, 36);">：图片生成</font>

![](https://intranetproxy.alipay.com/skylark/lark/0/2025/svg/341874/1763042868312-b558355d-40a1-48a4-96e1-e7252a4d1c69.svg)

## 端侧私有模型
[LiteRT for Android](https://ai.google.dev/edge/litert/android?_gl=1*ac6rqj*_up*MQ..*_ga*MTM4MDIzMTk0OS4xNzYzMDM3OTk1*_ga_P1DBVKWT6V*czE3NjMwMzc5OTUkbzEkZzAkdDE3NjMwMzc5OTUkajYwJGwwJGgxNDMwNzE2OTAw)提供运行时支持，可以将私有模型转换为可以在Android上运行的模型。

![](https://intranetproxy.alipay.com/skylark/lark/0/2025/png/341874/1763043043451-2d565b08-9448-4d14-bbe9-8d8ab324f3a2.png)



