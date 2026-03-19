# Monitor 项目学习路线图

> 基于已有的 Camera 项目知识（Linux系统编程、现代C++、V4L2、CUDA、ROS2、CMake、spdlog等），
> 本文档**仅列出 Monitor 项目中新增的、不重复的**知识点。

---

## 项目整体认识

Monitor 是一个自动驾驶车辆监控系统，运行在车载工控机上，核心职责：
- 通过 **WebSocket/HTTP** 与云端/远程客户端通信
- 通过 **ROS1/ROS2** 订阅车辆各模块话题（定位、感知、底盘等）
- 通过 **CAN 总线** 与底盘硬件交互
- 提供一个基于 **Qt/QML** 的车载 HMI 界面
- 管理进程监控、故障诊断、定时任务、数据录制、文件存储等功能

架构核心：自研 **消息总线**（`MessageRoute` + `BaseModule`），所有模块通过消息收发解耦通信。

---

## 模块一：Qt 框架（最核心的新知识）

Camera 项目不涉及 Qt，Monitor 是 Qt 应用，Qt 是理解整个项目的前提。

### 1.1 Qt 核心机制：信号与槽（Signals & Slots）

**是什么：**
Qt 的核心通信机制，相当于一个类型安全的观察者模式。
对比 C++ 回调函数：信号槽更安全（类型检查）、可跨线程（Qt 自动处理线程切换）。

**学习要点：**
- `Q_OBJECT` 宏 —— 必须放在类声明中，启用信号槽、MOC 处理
- `signals:` 区块 —— 声明信号（不需要实现，编译器自动生成）
- `slots:` 区块 —— 声明槽函数（普通成员函数，需要自己实现）
- `emit` 关键字 —— 发射信号：`emit mySignal(arg)`
- `connect()` 函数 —— 连接信号与槽：`connect(obj1, &Foo::signal, obj2, &Bar::slot)`

**MOC（Meta Object Compiler）：**
Qt 的预处理工具，处理 `Q_OBJECT` 宏，生成信号槽的胶水代码。
CMake 中 `set(CMAKE_AUTOMOC ON)` 让 CMake 自动调用 MOC。

**项目中出现位置：**
所有 `TopbarQmlPluginPlugin` 等 QML 插件类都使用 `Q_OBJECT`。

**学习资源关键词：** `Qt signals slots tutorial`, `Qt Q_OBJECT macro`

---

### 1.2 Qt 元对象系统与属性（Q_PROPERTY）

**是什么：**
让 C++ 类的属性可以被 QML 访问，是 C++/QML 交互的桥梁。

**学习要点：**
- `Q_PROPERTY(type name READ getter WRITE setter NOTIFY signal)`
- `Q_INVOKABLE` —— 标记函数可被 QML 调用
- `QVariant` —— Qt 的万能数据类型，可存储任意 Qt 支持的类型

**学习资源关键词：** `Qt Q_PROPERTY QML integration`

---

### 1.3 Qt 资源系统（.qrc 文件）

**是什么：**
将图片、QML 文件等资源打包进可执行文件，通过 `qrc:/` 前缀访问。

**学习要点：**
- `.qrc` 文件：XML 格式，列出要打包的资源
- CMake 中 `set(CMAKE_AUTORCC ON)` 自动处理 `.qrc`
- 代码中使用：`source: "qrc:/bottombar_background.png"`

**项目中出现位置：**
`BottomBar.qml` 中 `source: "qrc:/bottombar_background.png"`

---

## 模块二：QML（Qt Modeling Language）

### 2.1 QML 基本语法

**是什么：**
Qt 的声明式 UI 语言，语法类似 JavaScript + CSS，用于描述 UI 布局和行为。

**基本结构：**
```qml
import QtQuick 2.12
import QtQuick.Controls 2.5

Page {             // 根元素（类型名）
    id: root       // 元素的唯一标识符
    width: 400
    height: 300

    Text {         // 子元素
        text: "Hello"
        anchors.centerIn: parent
    }
}
```

**学习要点：**
- **属性绑定**：`width: parent.width * 0.5` —— 自动追踪变化，parent宽改变时自动更新
- **id**：唯一标识，用于引用其他元素：`anchors.centerIn: parent`
- **锚布局（anchors）**：`anchors.left/right/top/bottom/centerIn` —— 相对定位
- **信号与处理器**：`onClicked: { ... }`, `signal mySignal()`
- **Connections**：监听其他对象的信号

**项目中出现位置：**
所有 `src/plugins/*/qml/*.qml` 文件

**学习资源关键词：** `QML tutorial Qt Quick`, `QML anchors layout`

---

### 2.2 Qt Quick 常用组件

**学习要点：**
- `Item` —— 最基础的可视元素（相当于 HTML 的 div）
- `Rectangle` —— 矩形，可设颜色、圆角
- `Text` —— 文本显示
- `Image` —— 图片
- `Button` —— 按钮（QtQuick.Controls）
- `ListView / GridView` —— 列表/网格视图
- `RowLayout / ColumnLayout` —— 行列布局（QtQuick.Layouts）
- `Page` —— 页面容器
- `Loader` —— 动态加载 QML 组件

**项目中出现位置：**
`BottomBar.qml` 中的 `Page`, `RowLayout`, `Image`

---

### 2.3 C++ 与 QML 交互

**是什么：**
C++ 逻辑层与 QML UI 层互相调用的方式。

**学习要点：**

**C++ 暴露给 QML：**
```cpp
// 在 C++ 中注册类型（让 QML 可以 import 使用）
qmlRegisterType<MyCppClass>("MyModule", 1, 0, "MyCppClass");

// 注册单例对象（让 QML 全局访问）
engine.rootContext()->setContextProperty("router", &routerObject);
```

**QML 中使用：**
```qml
import MyModule 1.0   // 导入注册的模块
MyCppClass { id: obj }  // 实例化C++对象

Connections {
    target: router     // 监听 C++ 对象的信号
    onCurrentPageChanged: { ... }
}
```

**项目中出现位置：**
`BottomBar.qml` 中的 `target: router`，`onCurrentPageChanged` —— 这里的 `router` 是 C++ 注册的对象。

---

### 2.4 QQmlExtensionPlugin（QML 插件）

**是什么：**
将 QML 组件打包成动态库插件，让其他 QML 文件可以 `import` 使用。

**学习要点：**
- 继承 `QQmlExtensionPlugin` 基类
- 实现 `registerTypes(const char* uri)` 方法注册 C++ 类型
- `Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)` —— 插件元数据宏

**项目中出现位置：**
`BottomBar/src/topbarqmlplugin_plugin.h` 中的 `TopbarQmlPluginPlugin`
所有 `src/plugins/*/src/` 目录下的插件类

---

## 模块三：自研消息总线架构

这是 Monitor 项目最核心的内部通信机制，理解它才能读懂所有模块。

### 3.1 整体设计

**架构图：**
```
┌─────────────────────────────────────────┐
│              MessageRoute               │  ← 消息路由中心（单例）
│  subscribe/publish/sync/async dispatch  │
└──────┬───────────┬──────────────────────┘
       │           │
  ┌────▼────┐  ┌───▼────┐
  │ HttpModule│  │CANModule│  ← 各 BaseModule 子类
  └────┬────┘  └────┬───┘
       │             │
  发送消息      订阅消息
```

**核心类：**
- `MessageRoute` —— 消息路由器，负责分发
- `BaseModule` —— 模块基类，所有模块继承它，通过它订阅/发送消息

### 3.2 BaseModule 使用模式

**学习要点：**

```cpp
class MyModule : public BaseModule {
public:
    bool init() override {
        // 在这里订阅需要接收的消息类型
        subscribeMessage(Http_Message);
        subscribeMessage(Config_Message);
        return true;
    }

    // 收到消息时调用（必须实现）
    std::shared_ptr<BaseResponse> onProcessMessage(
        std::shared_ptr<BaseMessage> &message) override {
        if (message->getMessageType() == Http_Message) {
            // 处理 Http 消息
        }
        return nullptr;
    }

    // 收到消息回应时调用（必须实现）
    void onProcessResponse(std::shared_ptr<BaseResponse> &response) override {}
};
```

**消息类型：**
- **同步消息（`Sync_Trans_Message`）**：发送方阻塞等待，接收方处理完返回
- **异步消息（`Async_Trans_Message`）**：发送后立即返回，接收方异步处理
- **单播（`Message_Unicast`）**：发给订阅该类型的模块
- **广播（`Message_Broadcast`）**：发给所有模块

**Hook 机制：**
- `foreseeMessage()` —— 在消息到达订阅者之前拦截处理
- `hindsightMessage()` —— 在消息被订阅者处理完之后追加处理

**学习资源关键词：** `C++ publish-subscribe pattern`, `observer pattern C++`

---

## 模块四：WebSocket++ 与通信协议

### 4.1 WebSocket 协议基础

**是什么：**
基于 HTTP 升级的全双工通信协议，客户端和服务器可以互相推送数据。
对比 HTTP：HTTP 是请求-响应，WebSocket 建立后可持续双向通信。

**学习要点：**
- WebSocket 握手：客户端发 HTTP Upgrade 请求，服务器 101 响应
- 帧格式：文本帧、二进制帧、ping/pong 帧
- 连接标识：`connection_hdl`（句柄，标识一条连接）

---

### 4.2 websocketpp 库

**是什么：**
C++ 的 WebSocket 服务器/客户端库，基于 ASIO 实现。

**学习要点：**
```cpp
#include "server.hpp"
#include "config/asio_no_tls.hpp"

websocketpp::server<websocketpp::config::asio> server;

// 注册回调
server.set_message_handler([](connection_hdl hdl, server::message_ptr msg) {
    // 收到消息
});
server.set_open_handler([](connection_hdl hdl) {
    // 新连接
});
server.set_close_handler([](connection_hdl hdl) {
    // 连接关闭
});

// 启动监听
server.listen(port);
server.start_accept();
server.run();  // 阻塞，在 IO 线程中运行
```

**关键类型：**
- `websocketpp::connection_hdl` —— 弱引用句柄，标识一条连接
- `server::message_ptr` —— 消息指针

**项目中出现位置：**
`HttpModule.h` 中的 `mHttpServer`，以及所有 `onWebSocket*` 回调函数

**学习资源关键词：** `websocketpp tutorial`, `websocketpp server example`

---

### 4.3 ASIO 异步 I/O

**是什么：**
Boost.Asio 的独立版本（不依赖 Boost），提供异步 I/O 操作。
项目中 `ASIO_STANDALONE` 宏表示使用独立版。

**学习要点：**
- `asio::io_context` —— 事件循环，驱动所有异步操作
- `io_context.run()` —— 阻塞运行事件循环
- 异步模式：提交操作 → 注册回调 → 事件发生时调用回调
- 主要用于 websocketpp 的底层 I/O 驱动

**学习资源关键词：** `asio standalone tutorial`, `Asio io_context async`

---

## 模块五：Protocol Buffers（Protobuf）

### 5.1 基本概念

**是什么：**
Google 的二进制序列化格式，比 JSON/XML 更小更快，适合网络传输。

**学习要点：**

**`.proto` 文件语法（以项目 Websocket.proto 为例）：**
```protobuf
syntax = "proto3";
package Websocket;

message Websocket {
    string header = 1;          // 字段编号（不是值！）
    string request = 2;
    string response = 3;
    repeated bytes binary = 5;  // repeated = 数组
}
```

**生成 C++ 代码：**
```bash
protoc --cpp_out=. Websocket.proto
# 生成 Websocket.pb.h 和 Websocket.pb.cc
```

**C++ 使用：**
```cpp
#include "Websocket.pb.h"

Websocket::Websocket ws;
ws.set_header("{\"type\":\"request\"}");
ws.set_request("{\"action\":\"start\"}");

// 序列化为字节流
std::string data = ws.SerializeAsString();

// 反序列化
Websocket::Websocket parsed;
parsed.ParseFromString(data);
std::string header = parsed.header();
```

**项目中出现位置：**
`Websocket.proto`, `Websocket.pb.h` —— WebSocket 消息的封装格式

**学习资源关键词：** `protobuf3 tutorial C++`, `Protocol Buffers proto3`

---

## 模块六：SQLite 嵌入式数据库

### 6.1 SQLite 基础

**是什么：**
无需服务器的嵌入式 SQL 数据库，以单个文件存储数据。
项目用它存储任务记录、定时任务、故障日志等。

**学习要点（C API）：**
```c
sqlite3 *db;
sqlite3_open("monitor.db", &db);           // 打开数据库文件

// 执行 SQL
char *errmsg;
sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS task (id INTEGER PRIMARY KEY, name TEXT)",
             NULL, NULL, &errmsg);

// 查询
sqlite3_stmt *stmt;
sqlite3_prepare_v2(db, "SELECT * FROM task WHERE id = ?", -1, &stmt, NULL);
sqlite3_bind_int(stmt, 1, 42);              // 绑定参数
while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* name = (char*)sqlite3_column_text(stmt, 1);
}
sqlite3_finalize(stmt);

sqlite3_close(db);
```

**常用 SQL 语句：**
- `CREATE TABLE` —— 建表
- `INSERT INTO` —— 插入数据
- `SELECT WHERE ORDER BY LIMIT` —— 查询
- `UPDATE SET WHERE` —— 更新
- `DELETE FROM WHERE` —— 删除

**项目中出现位置：**
`DatabaseModule` 模块，`DataBaseVersion.h` 管理数据库版本迁移

**学习资源关键词：** `SQLite C API tutorial`, `sqlite3_exec example`

---

## 模块七：CAN 总线通信

### 7.1 CAN 协议基础

**是什么：**
Controller Area Network，汽车行业的串行通信总线，用于车身控制器之间通信。
项目通过 CAN 与底盘（电机、转向、刹车）交互。

**关键概念：**
- **CAN ID**：帧标识符（11位标准帧/29位扩展帧），标识消息类型
- **DLC**：数据长度（0~8字节）
- **数据帧**：最多8字节有效数据
- **波特率**：常见 250Kbps、500Kbps、1Mbps

### 7.2 SocketCAN（Linux CAN 接口）

**是什么：**
Linux 内核内置的 CAN 驱动框架，通过 socket API 操作 CAN 设备。
设备名：`can0`, `can1`，类似网卡。

**学习要点：**
```c
#include <linux/can.h>
#include <linux/can/raw.h>

// 创建 CAN socket
int sock = socket(AF_CAN, SOCK_RAW, CAN_RAW);

// 绑定到 CAN 接口
struct sockaddr_can addr;
struct ifreq ifr;
strcpy(ifr.ifr_name, "can0");
ioctl(sock, SIOCGIFINDEX, &ifr);
addr.can_family = AF_CAN;
addr.can_ifindex = ifr.ifr_ifindex;
bind(sock, (struct sockaddr*)&addr, sizeof(addr));

// 发送 CAN 帧
struct can_frame frame;
frame.can_id = 0x123;
frame.can_dlc = 8;
frame.data[0] = 0xAA;
write(sock, &frame, sizeof(frame));

// 接收 CAN 帧
read(sock, &frame, sizeof(frame));
```

### 7.3 DBC 文件格式

**是什么：**
CAN 数据库文件（Database CAN），定义 CAN 报文中每个信号的位置、长度、单位、换算公式。

**示例：**
```
BO_ 0x123 VehicleStatus: 8 Vector
 SG_ Speed : 0|16@1+ (0.01,0) [0|200] "km/h" Vector
 SG_ SteerAngle : 16|16@1- (0.1,-1000) [-1000|1000] "deg" Vector
```
- `0x123`：报文ID；`8`：8字节
- `Speed`：信号名；`0|16`：起始位0，16位长；`0.01,0`：系数和偏移

**项目中出现位置：**
`CloudDriverModule/src/dbc/` —— DBC 文件存储位置

**学习资源关键词：** `SocketCAN linux tutorial`, `DBC file format CAN`, `Linux CAN socket programming`

---

## 模块八：OpenSSL 密码学

### 8.1 基础用途

**是什么：**
项目用 OpenSSL 做 DES 加解密（见 `HttpModule` 中的 `onDESEncrypt/onDESDecrypt`）和 HTTPS 支持。

**学习要点（DES 为例）：**
```cpp
#include <openssl/des.h>

// DES 加密
DES_key_schedule ks;
DES_set_key((const_DES_cblock*)key, &ks);
DES_ecb_encrypt((const_DES_cblock*)input, (DES_cblock*)output, &ks, DES_ENCRYPT);

// 解密
DES_ecb_encrypt((const_DES_cblock*)input, (DES_cblock*)output, &ks, DES_DECRYPT);
```

**学习资源关键词：** `OpenSSL DES encryption C++`, `OpenSSL API tutorial`

---

## 模块九：libssh2 SSH 库

### 9.1 基础用途

**是什么：**
C 语言 SSH 客户端库，项目中 `SSHModule` 用它远程连接车辆或服务器执行命令/传文件。

**学习要点：**
```c
#include <libssh2.h>

libssh2_init(0);
LIBSSH2_SESSION *session = libssh2_session_init();

// TCP 连接（普通 socket）+ SSH 握手
libssh2_session_handshake(session, sock);

// 密码认证
libssh2_userauth_password(session, "user", "password");

// 执行命令
LIBSSH2_CHANNEL *channel = libssh2_channel_open_session(session);
libssh2_channel_exec(channel, "ls /var/log");
// 读取输出
char buf[1024];
libssh2_channel_read(channel, buf, sizeof(buf));

libssh2_channel_close(channel);
libssh2_session_disconnect(session, "bye");
libssh2_session_free(session);
```

**学习资源关键词：** `libssh2 tutorial example C`

---

## 模块十：jsoncpp JSON 解析库

### 10.1 与 nlohmann/json 的对比

Camera 项目用 `nlohmann/json`，Monitor 项目用 `jsoncpp`，两者 API 不同。

**jsoncpp 基础用法：**
```cpp
#include <json/json.h>

// 解析 JSON 字符串
Json::Value root;
Json::Reader reader;
reader.parse("{\"name\":\"monitor\",\"version\":1}", root);

std::string name = root["name"].asString();
int version = root["version"].asInt();

// 构建 JSON
Json::Value obj;
obj["status"] = "ok";
obj["code"] = 200;
Json::FastWriter writer;
std::string result = writer.write(obj);  // 输出 {"code":200,"status":"ok"}
```

**常用方法：**
- `asString()`, `asInt()`, `asBool()`, `asDouble()` —— 类型转换
- `isMember("key")` —— 检查键是否存在
- `Json::FastWriter` —— 快速序列化（无格式）
- `Json::StyledWriter` —— 带缩进的序列化

**学习资源关键词：** `jsoncpp tutorial C++`, `jsoncpp Json::Value`

---

## 模块十一：yaml-cpp YAML 解析库

### 11.1 基础用法

```cpp
#include <yaml-cpp/yaml.h>

YAML::Node config = YAML::LoadFile("config.yaml");

std::string host = config["server"]["host"].as<std::string>();
int port = config["server"]["port"].as<int>();

// 遍历序列
for (auto item : config["devices"]) {
    std::cout << item.as<std::string>();
}
```

**学习资源关键词：** `yaml-cpp tutorial`

---

## 模块十二：ROS1 与 ROS2 的差异

Camera 项目只用 ROS2，Monitor 项目同时支持 ROS1 和 ROS2（编译时选择）。

### 12.1 关键 API 差异

| 功能 | ROS1 | ROS2 |
|------|------|------|
| 节点初始化 | `ros::init(argc, argv, "name")` | `rclcpp::init(argc, argv)` |
| 节点句柄 | `ros::NodeHandle nh` | `rclcpp::Node::make_shared("name")` |
| 发布者 | `nh.advertise<MsgType>("topic", 10)` | `node->create_publisher<MsgType>("topic", 10)` |
| 订阅者 | `nh.subscribe("topic", 10, callback)` | `node->create_subscription<MsgType>(...)` |
| 服务 | `nh.advertiseService("srv", callback)` | `node->create_service<SrvType>(...)` |
| 循环 | `ros::spin()` | `rclcpp::spin(node)` |
| 消息类型前缀 | `std_msgs::String` | `std_msgs::msg::String` |
| 时间戳 | `ros::Time::now()` | `node->now()` |

**项目中出现位置：**
`RosModule/src/ROS1/` 目录下的 ROS1 实现
`RosModule/src/ROS2/` 目录下的 ROS2 实现
CMakeLists.txt 中通过 `$ENV{ROS_VERSION}` 选择编译哪套

**学习资源关键词：** `ROS1 roscpp tutorial`, `ROS1 vs ROS2 differences`

---

## 模块十三：C++ 高级模式（新增）

### 13.1 std::enable_shared_from_this

**是什么：**
当一个对象已经被 `shared_ptr` 管理时，如何在成员函数内安全地获取指向自身的 `shared_ptr`。

**为什么需要：**
```cpp
// ❌ 错误：创建第二个独立的 shared_ptr，会导致双重释放
class Foo {
    std::shared_ptr<Foo> getThis() {
        return std::shared_ptr<Foo>(this);  // 危险！
    }
};

// ✅ 正确：继承 enable_shared_from_this
class Foo : public std::enable_shared_from_this<Foo> {
    std::shared_ptr<Foo> getThis() {
        return shared_from_this();  // 安全，共享同一个引用计数
    }
};
```

**项目中出现位置：**
`BaseModule` 继承 `std::enable_shared_from_this<BaseModule>`

---

### 13.2 std::recursive_mutex

**是什么：**
可重入的互斥锁，同一线程可以多次 `lock()` 而不死锁。

**适用场景：**
当函数 A 持有锁，又调用函数 B，B 也需要同一把锁时（普通 mutex 会死锁，recursive_mutex 不会）。

**项目中出现位置：**
`MessageRoute.h` 中的 `mAllModuleLocker` 等锁

---

## 模块十四：第三方工具库（快速了解）

| 库 | 用途 | 在项目中的位置 |
|----|------|----------------|
| `curl` | HTTP 客户端（下载文件、请求云端接口）| StorageModule 文件上传下载 |
| `miniocpp` | MinIO 对象存储客户端（S3 兼容）| 云端数据存储 |
| `aws-sdk-cpp` | AWS S3 云存储 | 文件上传到 AWS |
| `gperftools` | CPU/内存性能分析（pprof）| `profiling/` 目录 |
| `miniaudio` | 音频播放录制 | `AudioModule` 播放语音提示 |
| `log4cplus` | 另一种日志库 | 部分模块使用（与 spdlog 并存）|
| `toojpeg` | 轻量 JPEG 编码（纯 C++）| 摄像头截图保存 |

---

## 推荐学习顺序

| 阶段 | 模块 | 预计时间 | 优先级 |
|------|------|----------|--------|
| 第一阶段 | 模块三（消息总线架构）| 2天 | ★★★★★ |
| 第一阶段 | 模块一（Qt 信号槽）| 3天 | ★★★★★ |
| 第二阶段 | 模块二（QML）| 3天 | ★★★★☆ |
| 第二阶段 | 模块四（WebSocket++/ASIO）| 2天 | ★★★★☆ |
| 第三阶段 | 模块五（Protobuf）| 1天 | ★★★☆☆ |
| 第三阶段 | 模块六（SQLite）| 1天 | ★★★☆☆ |
| 第三阶段 | 模块七（CAN 总线）| 2天 | ★★★☆☆ |
| 第四阶段 | 模块十（jsoncpp）| 半天 | ★★☆☆☆ |
| 第四阶段 | 模块十二（ROS1 差异）| 1天 | ★★☆☆☆ |
| 第五阶段 | 模块八/九（OpenSSL/libssh2）| 1天 | ★★☆☆☆ |
| 第五阶段 | 模块十三（C++高级模式）| 半天 | ★★☆☆☆ |

---

## 推荐学习资源

| 知识点 | 推荐资源 |
|--------|----------|
| Qt 信号槽 | `doc.qt.io/qt-5/signalsandslots.html` |
| QML | `doc.qt.io/qt-5/qtquick-index.html` |
| Qt Quick 组件 | `doc.qt.io/qt-5/qtquickcontrols-index.html` |
| websocketpp | `github.com/zaphoyd/websocketpp`（README+examples）|
| ASIO | `think-async.com/Asio/asio-1.28.0/doc` |
| Protobuf | `protobuf.dev/programming-guides/proto3` |
| SQLite | `sqlite.org/cintro.html` |
| SocketCAN | `kernel.org/doc/html/latest/networking/can.html` |
| jsoncpp | `github.com/open-source-parsers/jsoncpp` |
| libssh2 | `libssh2.org/docs.html` |
