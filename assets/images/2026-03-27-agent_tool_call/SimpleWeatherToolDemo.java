/*
 * Copyright 2024-2026 the original author or authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package io.agentscope.examples.quickstart;

import io.agentscope.core.ReActAgent;
import io.agentscope.core.memory.InMemoryMemory;
import io.agentscope.core.message.Msg;
import io.agentscope.core.message.MsgRole;
import io.agentscope.core.message.TextBlock;
import io.agentscope.core.model.Model;
import io.agentscope.core.model.OpenAIChatModel;
import io.agentscope.core.tool.Tool;
import io.agentscope.core.tool.ToolParam;
import io.agentscope.core.tool.Toolkit;

/**
 * Simple Weather Tool Demo - Mock Weather Data
 *
 * <p>
 * This is a simple demonstration of tool calling in AgentScope.
 * The agent has access to a weather tool that returns mock data.
 */
public class SimpleWeatherToolDemo {

    public static void main(String[] args) {
        // Create toolkit and register the weather tool
        Toolkit toolkit = new Toolkit();
        toolkit.registerTool(new WeatherTools());

        // Build the agent with the tool
        ReActAgent agent =
                ReActAgent.builder()
                        .name("WeatherAssistant")
                        .sysPrompt(
                                "You are a helpful weather assistant. "
                                        + "When asked about weather, use the get_weather tool.")
                        .model(createModel())
                        .toolkit(toolkit)
                        .memory(new InMemoryMemory())
                        .build();

        // Simple interaction: ask for weather
        String city = "Hangzhou";
        System.out.println("User: What's the weather in " + city + "?");

        Msg userMsg =
                Msg.builder()
                        .role(MsgRole.USER)
                        .content(
                                TextBlock.builder()
                                        .text("What's the weather in " + city + "?")
                                        .build())
                        .build();

        Msg response = agent.call(userMsg).block();
        if (response != null) {
            String text =
                    response.getContent().stream()
                            .filter(block -> block instanceof TextBlock)
                            .map(block -> ((TextBlock) block).getText())
                            .collect(java.util.stream.Collectors.joining("\n"));
            System.out.println("Agent: " + text);
        }
    }

    public static Model createModel() {
        return OpenAIChatModel.builder()
                .modelName("xxxxxx")
                .baseUrl("http://xxxxxxxxx")
                .apiKey("xxxxxxxxx")
                .stream(false)
                .build();
    }

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
        public String getWeather(
                @ToolParam(
                                name = "city",
                                description =
                                        "The city name, e.g., 'Beijing', 'Shanghai', 'New York'")
                        String city) {

            // Mock weather data (fixed values)
            return String.format(
                    "Weather in %s:\n- Condition: Sunny\n- Temperature: 25°C\n- Humidity: 60%%",
                    city);
        }
    }
}
