---
title: "如何通过LangChain访问Dify平台绑定的大模型"
date: 2026-01-20
categories: [AI]
tags:  [Dify, LangChain]
---

本文内容涉及两种Agent开发模式的对比，仅代表个人观点。  
不同的开发模式有不同的应用场景，并不存在绝对的优劣之分。

## 1. 背景

[Dify](https://dify.ai/)和[LangChain](https://www.langchain.com/)都可以用来开发Agent，但它们在设计理念和使用场景上有显著差异:
- Dify的特点是低代码、可视化，通过拖拽相关组件拼搭业务（很像Scratch少儿编程），适合非技术人员快速落地想法；
- LangChain是面向开发者的模块化编程框架，适合复杂定制和深度集成的场景。

如果要实现一个workflow（即工作流：第一步干啥，第二步干啥...），用LangChain来实现的话，就是一步一步的写代码了，用Dify来实现的话，就是搭建下图所示的工作流：
![Dify工作流(图片来自Dify官网)](/assets/images/2026-01-20-dify_plugin_for_langchain/dify_workflow_overview.jpg)


二者的工程化部署也有较大的差异，比如要开发一个手机端的AI助手，Dify方案的部署方式如下：
- 主要的业务逻辑跑在Dify平台上
- 数据流向：手机--> Dify平台 --> 大模型

![Dify部署方式](/assets/images/2026-01-20-dify_plugin_for_langchain/dify-dify_project.jpg)

LangChain方案的部署方式如下：
- 主要的业务逻辑跑在端侧，运行在App进程中
- 数据流向：手机 --> 大模型

![langchain部署方式](/assets/images/2026-01-20-dify_plugin_for_langchain/dify-langchain_project.jpg)

## 2. 为何要通过LangChain访问Dify平台绑定的大模型

Dify和LangChain本来是八竿子打不着的两个框架/平台，为啥要打通它俩？

不可否认，Dify很火，降低了AI应用开发门槛，为普及AI应用开发做出了一定的贡献。但我近期参与了一个项目，使用的是Dify部署方案，期间感受到Dify的诸多不便，比如：
- 多人协作问题：同一个workflow，如果多人修改，如何合并、验证，即分支/版本管理问题；
- 维护问题：随着功能逐渐丰富，流程图上连线密密麻麻（很像数据中心没理好的网线），越来越难维护；
- 定制化能力太弱：有些功能点，编码很容易实现，但Dify不支持，经常要为此想workroud方案。

那直接使用LangChain不就行了吗，为啥要经过Dify中转呢？因为某些非技术原因无法直接访问大模型，只能通过下面的链路间接使用大模型，所以需要在LangChain中实现一个Dify插件。下图中的Dify workflow是极简的，不含任何业务逻辑。
![通过Dify访问模型能力](/assets/images/2026-01-20-dify_plugin_for_langchain/dify-langchain_vice_dify_project.jpg)

## 3. 实现方法

首先在Dify平台创建一个最简单的chatflow，不包含任何业务逻辑:
![不包含任何业务逻辑的chatflow](/assets/images/2026-01-20-dify_plugin_for_langchain/chatflow.jpg)

然后在`用户输入`节点增加一个名为`tools`的变量，如下图所示，`langchain/dify`会把工具信息填到这里，这个变量长度最好设置的大一点，以免工具信息被截断：
![tools变量](/assets/images/2026-01-20-dify_plugin_for_langchain/tool_param.jpg)

接着在`LLM`节点使用变量`tools`和`query`，`langchain/dify`会把**用户消息**和**历史消息**按顺序填在`query`中：
![llm节点配置](/assets/images/2026-01-20-dify_plugin_for_langchain/tools_and_query.jpg)

最后执行发布操作，就可以用`langchain/dify`访问了。

`langchain/dify`编译运行方法参考：[https://github.com/sigusr1/langchainjs_with_dify_plugin/tree/main/libs/providers/langchain-dify](https://github.com/sigusr1/langchainjs_with_dify_plugin/tree/main/libs/providers/langchain-dify)

目录[https://github.com/sigusr1/langchainjs_with_dify_plugin/tree/main/examples/src/provider/dify](https://github.com/sigusr1/langchainjs_with_dify_plugin/tree/main/examples/src/provider/dify)包含几个简单的demo：
- `chat.ts` 是简单的会话demo；
- `llmWithTool.ts`是通过llm调用工具的demo，调用者需要自行管理整个流程；
- `agentWithTools.ts`是通过`langchain`的`ReactAgent`调用工具的demo，`ReactAgent`框架自动管理整个流程。

*Tips: `langchain/dify`只是为了验证我的想法做的个实验性插件，实际项目使用需谨慎（估计也没人有这种奇怪的需求）。*

## 4. 其他

- 有人通过在Dify云平台上部署插件实现类似功能，比如[Dify2OpenaiApi](https://github.com/yunwuneo/Dify2OpenaiApi)，前提是有权限操作Dify云服务。
- 我认为在端侧通过`langchain/dify`插件的方式实现有两个好处：
  - 能够无缝融入`LangChain`/`LangGraph`现有的生态
  - 哪天端侧AI能力强大了，可以无缝从云端AI切换到端侧AI
- 我猜测Dify官方肯定不喜欢这类插件，因为这样就把Dify弱化为一个llm提供商了，但Dify自己实际上又不托管llm，这就很尴尬了。

