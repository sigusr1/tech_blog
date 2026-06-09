---
title: "AppFunctions简介：Android原生MCP方案"
date: 2026-06-06
categories: [Android]
tags:  [AppFunctions]
---


> 本文基于 [android-16.0.0_r4](https://cs.android.com/android/platform/superproject/+/android-16.0.0_r4:?hl=zh-cn) 源码，结合一个[Demo](https://github.com/sigusr1/appfunction_java_demo)，介绍AppFunction工作原理。
>
> 现在能查到的 AppFunction 相关 Demo 都是基于 Kotlin 注解方式实现的，虽然对 App 开发者友好，但隐藏了底层实现细节。**本文的 Demo 是纯手撸 Java 版，直接使用 Platform API 实现**，便于理解底层工作原理。

## 1. 引言

随着端侧系统级Agent的兴起（如AI助手），一个关键问题浮出水面：**Agent如何标准化地调用其他应用的能力？**

具体到Android系统，传统的 Intent/ContentProvider 机制虽然能实现跨应用通信，但缺乏对【函数语义】的结构化描述——Agent 不知道目标应用有哪些能力、每个能力需要什么参数、返回什么结果。每接入一个新应用，都要写一套定制的解析和适配逻辑。

等等，MCP（Model Context Protocol）不是已经解决了这类问题吗？MCP 并非不可用，但将其应用于 Android 系统，存在以下局限性：

- **缺乏服务发现机制**：MCP 自身不提供服务发现能力，Agent 无法获知系统中有哪些 MCP Server 可用
- **通信协议不适配**：MCP 依赖 HTTP/gRPC 等网络协议进行通信，与 Android 原生的 Binder IPC 机制不匹配
- **权限模型不兼容**：MCP 基于 OAuth 的鉴权体系，难以对接 Android 以应用签名和权限声明为核心的安全模型


为了从系统层面解决这些问题，Android 16 引入了 **AppFunction** 框架——一套深度融合 Android 平台能力的 **Provider-Consumer** 跨应用函数调用机制：

- **Provider 应用**声明自己暴露哪些函数、参数签名是什么
- **系统自动索引**这些元数据到 AppSearch，形成全局可发现的函数注册表
- **Consumer（Agent）应用**通过标准 API 搜索发现可用函数，并安全地发起调用

本文将从架构设计、核心组件、数据流转三个维度，结合一个完整的 [Demo](https://github.com/sigusr1/appfunction_java_demo)讲解这套机制。

---

## 2. 整体架构

AppFunction 的架构核心是 **Provider-Consumer + 中心化元数据注册表** 三层结构：

```
┌────────────────────────┐             ┌─────────────────────────────┐             ┌──────────────────────────┐
│       Agent App        │             │       System Server         │             │      Provider App        │
├────────────────────────┤             ├─────────────────────────────┤             ├──────────────────────────┤
│                        │             │                             │             │                          │
│  AppSearchManager      │───query────>│  AppSearchManagerService    │───parse────>│   app_functions.xml      │
│                        │             │                             │             │ app_function_schema.xml  │
├────────────────────────┤             ├─────────────────────────────┤             ├──────────────────────────┤
│                        │             │                             │             │                          │
│  AppFunctionManager    │────call────>│ AppFunctionManagerService   │────bind────>│  MyAppFunctionService    │
│                        │             │                             │             │                          │
└────────────────────────┘             └─────────────────────────────┘             └──────────────────────────┘
```

上述架构在运行时体现为三条链路——注册、发现、执行，对应一个 AppFunction 从【声明】到【被调用】的完整生命周期：

| 链路 | 触发时机 | 关键组件 | 说明 |
|------|---------|---------|------|
| **注册链路** | 应用安装/更新 | AppsIndexerManagerService → XML 解析 → AppSearchManagerService | 解析 XML 元数据，写入 AppSearch 数据库 |
| **发现链路** | Agent 主动查询 | AppSearchManager → AppSearchManagerService | 从 AppSearch 数据库查询已注册的函数元数据 |
| **执行链路** | Agent 发起调用 | AppFunctionManager → AppFunctionManagerService → MyAppFunctionService | Binder IPC 中转，权限校验 + 服务绑定 + 回调分发 |

---

## 3. Provider 端：声明与实现函数

Provider 端需要完成三件事：**定义 Schema**、**声明函数实例**、**实现服务逻辑**。

### 3.1 定义 Schema：`app_function_schema.xml`

AppFunction 的元数据最终会被写入 AppSearch 数据库，而 AppSearch 要求先定义 Schema（类似数据库的表结构），才能写入数据。Schema 文件的作用就是**定义"函数元数据"这张表有哪些字段、每个字段的类型和索引方式**。后面的 `app_functions.xml` 则是基于这个 Schema 填入的具体数据——两者的关系类似于数据库中的“建表语句”和“INSERT 数据”。

下面是 Demo 中的 Schema 文件，定义了两类字段：

**基础属性**（框架要求必须声明）：告诉系统这个函数的身份信息——叫什么名字（`functionId`）、属于哪个应用（`packageName`）、归属哪个功能类别（`schemaName`/`schemaCategory`）、默认是否启用（`enabledByDefault`）等。这些字段的名称和索引配置必须与框架源码中 `createAppFunctionSchemaForPackage` 方法定义的完全一致，否则系统查询时会找不到。

**自定义扩展属性**（开发者自定义）：描述函数的参数签名——参数叫什么（`paramNames`）、什么类型（`paramTypes`）、是否必填（`paramRequired`）。这些是开发者自定义的字段。Agent 查询到这些信息后，就知道该怎么构造调用参数了。

```xml
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
    <xs:documentType name="AppFunctionStaticMetadata">
        <!-- 基础属性（必须重新声明，索引配置需与框架的 createAppFunctionSchemaForPackage 一致） -->
        <xs:element name="functionId"       type="xs:string"  cardinality="2" indexingType="1" tokenizerType="2" />
        <xs:element name="packageName"      type="xs:string"  cardinality="2" indexingType="1" tokenizerType="2" />
        <xs:element name="schemaName"       type="xs:string"  cardinality="2" indexingType="1" tokenizerType="2" />
        <xs:element name="schemaVersion"    type="xs:long"    cardinality="2" indexingType="1" />
        <xs:element name="schemaCategory"   type="xs:string"  cardinality="2" indexingType="1" tokenizerType="2" />
        <xs:element name="enabledByDefault" type="xs:boolean" cardinality="2" />
        <xs:element name="restrictCallersWithExecuteAppFunctions" type="xs:boolean" cardinality="2" />
        <xs:element name="displayNameStringRes"                   type="xs:long"    cardinality="2" />
        <xs:element name="mobileApplicationQualifiedId" type="xs:string" cardinality="2" joinableValueType="1" />

        <!-- 自定义扩展属性：描述函数参数信息（cardinality=1 即 REPEATED，支持多值） -->
        <xs:element name="paramNames"    type="xs:string"  cardinality="1" />
        <xs:element name="paramTypes"    type="xs:string"  cardinality="1" />
        <xs:element name="paramRequired" type="xs:boolean" cardinality="1" />
        <xs:element name="description"   type="xs:string"  cardinality="2" />
    </xs:documentType>
</xs:schema>
```

### 3.2 声明函数实例：`app_functions.xml`

这个文件的角色类似于 MCP 中 `tools/list` 返回的工具列表——它告诉系统“我提供了哪些函数，每个函数接收什么参数”。它和 3.1 节的 Schema 是配套关系：Schema 定义了“有哪些字段可以填”，这个文件则按照 Schema 的结构填入每个函数的具体数据。

以下示例声明了一个名为 `createNote` 的函数，类别属于 `productivity`，接收 3 个参数（title、content、priority），其中前两个必填、最后一个可选：

```xml
<appfunctions>
    <AppFunctionStaticMetadata>
        <!--
            id 和 functionId 的区别：
            - id：AppSearch 文档主键，会被拼接为 "packageName/id"，用于内部存储去重，Agent 不可见
            - functionId：Agent 调用时的函数标识，框架按此字段精确查询，这才是对外暴露的"函数名"
            两者通常填相同值，但可以不同
        -->
        <id>createNote</id>

        <!-- 基础属性 -->
        <functionId>createNote</functionId>
        <schemaName>actions.intent.CREATE_NOTE</schemaName>
        <schemaVersion>1</schemaVersion>
        <schemaCategory>productivity</schemaCategory>
        <enabledByDefault>true</enabledByDefault>
        <restrictCallersWithExecuteAppFunctions>false</restrictCallersWithExecuteAppFunctions>

        <!-- 函数描述 -->
        <description>Create a note with title, content and priority (1-5, default 3)</description>

        <!-- 参数列表：每个参数由 paramNames/paramTypes/paramRequired 三个同名标签按顺序对应 -->
        <paramNames>title</paramNames>
        <paramTypes>string</paramTypes>
        <paramRequired>true</paramRequired>

        <paramNames>content</paramNames>
        <paramTypes>string</paramTypes>
        <paramRequired>true</paramRequired>

        <paramNames>priority</paramNames>
        <paramTypes>int</paramTypes>
        <paramRequired>false</paramRequired>
    </AppFunctionStaticMetadata>
</appfunctions>
```

**注意：**

- **根标签固定为 `<appfunctions>`**，子标签名必须为 `AppFunctionStaticMetadata`（不可自定义，框架通过该名称建立 Schema 父类型继承链，改名会导致 Agent 查询不到函数）
- **`<id>` vs `<functionId>`**：`id` 是 AppSearch 文档主键，会被拼接为 `packageName/id` 用于内部存储去重，Agent 不可见；`functionId` 才是 Agent 调用时的函数标识，框架按此字段精确查询。两者通常填相同值，但语义不同

### 3.3 实现服务逻辑：`MyAppFunctionService`

定义好 Schema 和函数声明后，需要继承 `AppFunctionService` 来实现实际的执行逻辑。当 Agent 发起调用时，系统会绑定到这个 Service 并回调 `onExecuteFunction`，开发者需要关注以下几点：

- **函数路由**：通过 `request.getFunctionIdentifier()` 拿到 `functionId`，分发到对应的处理方法
- **参数传递**：请求参数和返回值都通过 `GenericDocument` 承载，属性名即参数名
- **安全模型**：调用方的身份和权限校验由 system_server 中的 `AppFunctionManagerService` 统一完成，Service 只需专注于业务逻辑

```java
public class MyAppFunctionService extends AppFunctionService {

    private static final String FN_CREATE_NOTE = "createNote";

    @Override
    public void onExecuteFunction(
            ExecuteAppFunctionRequest request,
            String callingPackage,
            SigningInfo callingPackageSigningInfo,
            CancellationSignal cancellationSignal,
            OutcomeReceiver<ExecuteAppFunctionResponse, AppFunctionException> callback) {

        String functionId = request.getFunctionIdentifier();

        if (FN_CREATE_NOTE.equals(functionId)) {
            handleCreateNote(request, callback);
        } else {
            callback.onError(new AppFunctionException(
                    AppFunctionException.ERROR_FUNCTION_NOT_FOUND,
                    "Unknown function: " + functionId));
        }
    }

    private void handleCreateNote(ExecuteAppFunctionRequest request,
                                  OutcomeReceiver<ExecuteAppFunctionResponse, AppFunctionException> callback) {
        GenericDocument params = request.getParameters();

        // 从 GenericDocument 中提取参数（int 类型以 long 存储）
        String title   = params.getPropertyString("title");
        String content = params.getPropertyString("content");
        long   priority = params.getPropertyLong("priority");

        // ... 参数校验省略，完整代码见 Demo ...

        String noteId = saveNote(title, content, (int) priority);

        // 构造返回值（同样是 GenericDocument）
        GenericDocument result = new GenericDocument.Builder<>("", noteId, "AppFunctionResult")
                .setPropertyString("noteId", noteId)
                .setPropertyString("message", "Note created successfully")
                .build();
        callback.onResult(new ExecuteAppFunctionResponse(result));
    }
}
```

### 3.4 注册服务：`AndroidManifest.xml`

最后一步是通过 `AndroidManifest.xml` 把Schema、函数声明和服务实现注册到系统中。这里有三个关键配置：

- **`BIND_APP_FUNCTION_SERVICE` 权限**：框架要求必须声明这个系统权限，这又决定了只有系统应用才能调用该service
- **Intent Filter Action**：AppsIndexerManagerService 通过这个 Action 发现你的服务
- **两个 property**：分别指向 3.1 节的 Schema 定义文件和 3.2 节的函数定义文件，AppsIndexerManagerService 读取它们来构建索引

```xml
<service
    android:name=".MyAppFunctionService"
    android:permission="android.permission.BIND_APP_FUNCTION_SERVICE"
    android:exported="true">
    <intent-filter>
        <action android:name="android.app.appfunctions.AppFunctionService" />
    </intent-filter>

    <!-- Schema 定义文件：声明字段类型、索引方式等 -->
    <property
        android:name="android.app.appfunctions.schema"
        android:value="app_function_schema.xml" />

    <!-- 函数数据文件：声明具体的函数实例 -->
    <property
        android:name="android.app.appfunctions.v2"
        android:value="app_functions.xml" />
</service>
```

---

## 4. 系统层：元数据索引与调用中转

### 4.1 AppsIndexerManagerService：安装时自动索引

当 Provider 应用安装或更新时，`AppsIndexerManagerService` 会自动执行以下流程：

1. 监听 PackageManagerService 发出的包安装/更新广播（`ACTION_PACKAGE_ADDED`/`REPLACED`）
2. 全量扫描所有声明了 `android.app.appfunctions.AppFunctionService` Action 的 Service，与 AppSearch 中已索引的数据做增量对比，仅处理新增或变更的包
3. 对需要更新的包，读取 Service 的 property，先用 `AppFunctionSchemaParser` 解析 Schema XML，再用 `AppFunctionDocumentParser` 解析函数定义 XML
4. 最后将解析出来的内容写入 AppSearch 数据库（package=`"android"`，namespace=`"app_functions"`）

整个注册过程对开发者完全透明——你只需要声明 XML 和 Manifest，系统会在应用安装时自动发现、解析。

### 4.2 AppFunctionManagerService：请求中转站

Agent 调用 `executeAppFunction` 时，请求并非直接到达 Provider 应用，而是经过 system_server 中转：

```
Agent → AppFunctionManager → [Binder IPC] → AppFunctionManagerService
    → 权限校验 (EXECUTE_APP_FUNCTIONS)
    → 函数启用状态检查
    → bindService (BIND_APP_FUNCTION_SERVICE)
    → IAppFunctionService.onExecuteFunction()
    → 回调结果返回 Agent
```

为什么要经过 system_server 中转而不是让 Agent 直连 Provider？核心原因是**集中管控**——权限校验、函数启停、访问审计，这些都需要一个可信的中间层来统一管控，而不是交给每个 Provider 自行实现。

### 4.3 EXECUTE_APP_FUNCTIONS 权限模型

Agent 想要调用**其他应用**的函数时，必须持有 `EXECUTE_APP_FUNCTIONS` 权限，`AppFunctionManagerService` 会在转发请求前校验此权限。它的保护级别是**条件性**的，取决于 feature flag 的状态：

| Feature Flag 状态 | protectionLevel | 说明 |
|-------------------|----------------|------|
| 开启 | `normal` | 声明即授予，普通应用可用 |
| 关闭 | `internal\|privileged` | 仅系统特权应用可用 |

> 调用自身应用的函数**不需要**此权限。

**注意**：Android 16 当前版本中该 feature flag 未开启，即 `EXECUTE_APP_FUNCTIONS` 权限为 `internal|privileged` 级别，仅系统特权应用可作为 Agent 调用其他应用的函数。

---

## 5. Agent 端：发现与调用函数

### 5.1 搜索发现

函数信息分散在两种 Schema 中：**静态元数据**（函数签名、参数描述，安装时写入）和**运行时元数据**（启用/禁用状态，随时可变），二者分开存储，可以独立更新，避免每次启停函数都要重新索引整个元数据。  
Agent 查询时使用 AppSearch 的 **Join 机制**（类似 SQL JOIN）将两者关联：外层查询负责检索函数静态定义，内层查询负责获取运行时状态，通过 `JoinSpec` 按关联键合并为一条结果。

```java
// 内层查询：运行时元数据，获取函数的启用/禁用状态
SearchSpec runtimeSpec = new SearchSpec.Builder()
        .addFilterPackageNames("android")
        .addFilterSchemas("AppFunctionRuntimeMetadata-" + targetPkg)
        .setVerbatimSearchEnabled(true)
        .build();

// Join 键：runtime 文档通过此字段关联到对应的 static 文档
JoinSpec joinSpec = new JoinSpec.Builder("appFunctionStaticMetadataQualifiedId")
        .setNestedSearch("", runtimeSpec)
        .build();

// 外层查询：静态元数据（主查询）
// ⚠️ 必须同时指定 namespace + schema，否则空查询会返回 package="android" 下所有文档
SearchSpec outerSpec = new SearchSpec.Builder()
        .addFilterPackageNames("android")
        .addFilterNamespaces("app_functions")
        .addFilterSchemas("AppFunctionStaticMetadata-" + targetPkg)
        .setJoinSpec(joinSpec)
        .setVerbatimSearchEnabled(true)
        .build();

SearchResults results = session.search("", outerSpec);
```

**说明：**

- 数据库包名固定为 `android`
- namespace固定为 `app_functions`

### 5.2 执行调用

通过 `AppFunctionManager.executeAppFunction()` 发起函数调用，需要指定**目标包名**和**函数 ID**（即 XML 中的 `functionId`），参数和返回值都通过 `GenericDocument` 承载。

不知道你有没有发现，本 Demo 的 XML 中仅定义了函数入参格式，并没定义出参格式。Agent拿到结果如何解析？  
实际情况是这样：
- 框架本身不感知返回值格式——它只负责将 Provider 构造的 `GenericDocument` 原样传回 Agent，严格来说它根本不关心出入参格式。
- 如果使用 AppFunctions SDK（Jetpack 库），SDK 会根据 Kotlin 注解在 XML 中自动生成 `<response>` 标签描述返回值的类型和字段，Agent 可以通过 AppSearch 查询这些元数据，解析返回值格式。
- 本文 Demo 的 XML 是手写的，简化起见未提供返回值格式定义。


```java
// 构造参数 GenericDocument
GenericDocument params = new GenericDocument.Builder<>("", "", "")
        .setPropertyString("title", title)
        .setPropertyString("content", content)
        .setPropertyLong("priority", priority)
        .build();

// 构建请求
ExecuteAppFunctionRequest request = new ExecuteAppFunctionRequest.Builder(
        "com.example.provider",   // 目标包名
        "createNote")             // 函数 ID，必须与 XML 完全匹配
        .setParameters(params)
        .build();

// 异步执行
appFunctionManager.executeAppFunction(request, executor, cancelSignal,
        new OutcomeReceiver<ExecuteAppFunctionResponse, AppFunctionException>() {
            @Override
            public void onResult(ExecuteAppFunctionResponse response) {
                GenericDocument result = response.getResultDocument();
                String noteId = result.getPropertyString("noteId");
                // 处理成功结果...
            }

            @Override
            public void onError(AppFunctionException e) {
                // ERROR_DENIED(1) / ERROR_DISABLED(2) /
                // ERROR_FUNCTION_NOT_FOUND(3) / ERROR_INVALID_ARGUMENT(4)
            }
        });
```

---

## 6. 数据流转全景图

以一个完整的 `createNote` 调用为例，数据在各组件间的流转如下：

```
[Agent 通过 AppSearchManager 查询函数元数据]
  发现 functionId="createNote"，参数：title(string), content(string), priority(int)
        │
        ▼
[Agent 构造参数]
  GenericDocument { title:"Meeting", content:"...", priority:2 }
        │
        ▼
[AppFunctionManager.executeAppFunction]
  ExecuteAppFunctionAidlRequest (Binder 序列化)
        │
        ▼
[system_server: AppFunctionManagerService]
  ① 校验 EXECUTE_APP_FUNCTIONS 权限
  ② 查询 AppSearch 确认函数存在且 enabled
  ③ bindService → Provider 的 MyAppFunctionService
        │
        ▼
[Provider: onExecuteFunction]
  ExecuteAppFunctionRequest.getParameters() → GenericDocument
  提取 title/content/priority → 执行业务逻辑
  构造结果 GenericDocument { noteId:"note_xxx", message:"..." }
        │
        ▼
[ExecuteAppFunctionResponse(result)]
  通过 OutcomeReceiver 回调返回
        │
        ▼
[Agent: onResult]
  response.getResultDocument() → 提取 noteId/message
```

---

## 7. 其他

AppFunction 有望成为 Android 生态中 **AI Agent 与应用交互的标准协议**。随着更多应用接入，Agent 将能够像调用本地函数一样，安全、高效地编排跨应用能力。
AppFunction 目前还在实验阶段，建议开发者持续关注。

- Android16上只有系统应用才能调用AppFunction，可以参考[Android模拟器如何remount](https://tech.coderhuo.tech/posts/remount_android)，然后将Agent apk push 到 `/system/priv-app/`。
- 实际工程中，建议使用AppFunctions SDK（Jetpack 库）进行开发，下面是两个参考demo：
  - [AppFunctionsPilot](https://github.com/FilipFan/AppFunctionsPilot)
  - [Android官方AppFunctions Samples](https://github.com/android/appfunctions#)

---

## 8. 参考资料

- https://developer.android.com/ai/appfunctions
- https://github.com/android/skills/tree/main/device-ai/appfunctions
- https://github.com/FilipFan/AppFunctionsPilot
- https://github.com/android/appfunctions