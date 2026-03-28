---
title: "Agent工具调用过程简介"
date: 2026-03-27
categories: [AI]
tags:  [Agent, 工具调用]
---

你有没有好奇过，ChatGPT 是怎么查天气、做计算的？背后的秘密就是“工具调用”。本文将带你了解工具调用的过程。

## 1. 原理

现代 AI Agent 开发框架（如 LangChain、AgentScope）通常提供开箱即用的 Agent 类，比如 `ReActAgent`。前缀 **ReAct** 代表 **Reasoning + Acting**，即推理与行动：
 - 推理就是指调用大模型分析问题
 - 行动就是指调用外部工具

Agent执行某个任务的时候，可能进行多次Reasoning + Acting，这就是Agent loop。

这个 **Reasoning + Acting 循环** 是现代 Agent 的核心范式。下图展示了完整的工作流程（简化起见，图中并未体现 memory、RAG 等组件），步骤1、2、3、4组成了**Reasoning + Acting循环**：
1. Agent收到用户输入后，首先调用模型进行推理
2. Agent拿到模型推理输出后，判断是否需要调用工具：如果需要，执行步骤3；否则输出结果，流程结束
3. 调用工具
4. 收到工具执行结果，回到步骤1，再次执行推理，注意这时的推理输入是：[用户输入] + [工具执行结果]
![react_agent工作原理](/assets/images/2026-03-27-agent_tool_call/react_agent.jpg)


接下来我们通过一个工具调用的例子看下上述各部分是如何协作的。

## 2. 示例Demo

下面是用AgentScope-Java写的一个工具调用的例子：

[SimpleWeatherToolDemo](/assets/images/2026-03-27-agent_tool_call/SimpleWeatherToolDemo.java)

首先，我们用 `@Tool` 注解定义一个天气查询工具：
- 入参是城市名
- 出参是当地的天气情况，数据是mock的

```java
/**
 * WeatherTools - A simple tool class with a get_weather tool.
 */
public static class WeatherTools {

    /**
     * Get weather information for a city (mock data).
     *
     * @param city The city name
     * @return Weather information string
     */
    @Tool(name = "get_weather", description = "Get current weather information for a city")
    public String getWeather(@ToolParam(name = "city", description = "The city name, e.g., 'Beijing'") String city) {

        // Mock weather data (fixed values)
        return String.format(
                "Weather in %s:\n- Condition: Sunny\n- Temperature: 25°C\n- Humidity: 60%%",
                city);
    }
}
```

接下来创建ReActAgent，并将工具以toolkit方式注册到ReActAgent上：
```java
// Create toolkit and register the weather tool
Toolkit toolkit = new Toolkit();
toolkit.registerTool(new WeatherTools());

// Build the agent with the tool
ReActAgent agent
        = ReActAgent.builder()
                .name("WeatherAssistant")
                .sysPrompt(
                        "You are a helpful weather assistant. "
                        + "When asked about weather, use the get_weather tool.")
                .model(createModel())
                .toolkit(toolkit)
                .memory(new InMemoryMemory())
                .build();
```

现在就可以通过agent查询天气了：
```java
String city = "Hangzhou";
System.out.println("User: What's the weather in " + city + "?");

Msg userMsg = Msg.builder()
                        .role(MsgRole.USER)
                        .content(TextBlock.builder()
                                        .text("What's the weather in " + city + "?")
                                        .build())
                        .build();

Msg response = agent.call(userMsg).block();
System.out.println("Agent: " + response中解析出来的结果);
```

执行上述demo（如果要自己运行附件SimpleWeatherToolDemo.java，createModel中需要填入自己的API key等信息），输出如下：
```
User: What's the weather in Hangzhou?
Agent: The current weather in Hangzhou is sunny with a temperature of 25°C and humidity at 60%. It's a pleasant day!
```

> **注意**：Agent 的回复比工具返回的原始数据更自然。这是因为 Agent 收到工具输出后，会进行第二轮推理，用大模型将原始数据"润色"成自然语言。


下面我们从协议报文的角度看下Agent是怎么和大模型交互的。

## 3. 报文交互

不同的模型服务提供商提供的API可能不同，这里我们只讨论[OpenAI Chat Completions](https://github.com/openai/openai-openapi)格式的。

首先是Agent第一次推理时的请求报文，我们关注两点：
- messages部分，前面是system提示词（大模型的人设），接着是用户输入
- tools部分，只包含一个工具，即get_weather，它的作用、参数信息都在这里了，这样大模型就可以根据用户提问来决定是否调用这个工具，如果调用的话，入参怎么填
![Agent首次推理时的请求报文](/assets/images/2026-03-27-agent_tool_call/first_reasoning.jpg)

接着是大模型的回复：
- tool_calls部分说明要调用哪些工具，包括工具名(get_weather)和入参(Hangzhou)等信息
- 大模型的回复中，可以包含多个工具调用，也可以不包含工具调用，具体由大模型根据[用户问题]和[可用工具] 综合推理决定
- finish_reason为tool_calls，说明本次推理结束是因为工具调用暂停（这里说的暂停，只是供大模型的调用者使用，大模型本身不维护这个状态）
- usage部分包含本次推理的token消耗情况
![大模型的回复](/assets/images/2026-03-27-agent_tool_call/first_reasoning_reply.jpg)

Agent框架收到大模型的回复后，根据指示调用get_weather函数，拿到get_weather的返回值后，再次调用大模型进行推理：
- messages部分在第一次推理的基础上，又叠加了大模型的回复（role为assistant的消息），还有工具调用的结果（role为tool的消息）；这是因为大模型是没有记忆的，每次推理都是全新的，所以要把历史会话记录告诉它
- 函数get_weather还在tools中，这是因为，我们不知道大模型接下来是否继续调用工具；归根结底还是因为大模型没有记忆，每次推理的时候都要带上所有的工具信息。
![Agent再次推理时的请求报文](/assets/images/2026-03-27-agent_tool_call/second_reasoning.jpg)

大模型再次推理后的回复如下：
- 回复中没有tool_calls字段，说明不需要调用工具了
- finish_reason为stop，说明本次请求的推理结束了
- message中的内容和程序最终的输出一致
![大模型再次推理后的回复](/assets/images/2026-03-27-agent_tool_call/second_reasoning_reply.jpg)

Agent框架收到大模型的回复后，发现无需继续调用工具，就将结果直接输出给用户了。
