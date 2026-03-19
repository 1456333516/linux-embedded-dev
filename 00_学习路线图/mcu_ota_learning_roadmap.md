# mcu_ota 项目补充学习路线图

> 在已有三份路线图（camera + monitor + monitor_extra）的基础上，本文档**仅记录新发现的、未覆盖的**知识点。
> 来源：对 `D:\code\mcu_ota` 项目进行完整深读后发现。

---

## 项目整体认识

mcu_ota 是自动驾驶平台的 **MCU 固件 OTA 升级服务**，运行在车载工控机上，核心职责：
- 通过 **CAN 总线** 向各 MCU 节点（VCU、PMU、CCU、EPS 等）传输固件包
- 通过 **ROS2 Action** 接口向上层暴露升级任务（带进度反馈）
- 支持**进程守护模式**（fork + waitpid），崩溃后自动重启
- 包含 **数据采集录制** 子系统（MCAP 格式 + LZ4 压缩 + Foxglove 可视化）

---

## 模块一：ROS2 Action 接口

### 为什么需要学

已有路线图覆盖了 ROS2 的 Topic（发布/订阅）和 Service（请求/响应），但 **Action** 是第三种通信模式，专为**长时间运行任务**设计，是本项目 OTA 升级任务的核心接口。

### 1.1 Action 与 Service 的本质区别

| 特性 | Service | Action |
|------|---------|--------|
| 通信时长 | 短（毫秒级）| 长（秒~分钟级）|
| 反馈机制 | 无，只有一次 Response | 支持多次 Feedback 流式进度 |
| 可取消 | 不可取消 | 可随时取消（cancel）|
| 典型场景 | 查询状态、设置参数 | 固件升级、路径规划、数据采集 |

### 1.2 Action 文件语法（`.action`）

```
# 请求（Goal）：调用方的输入
string id
string addr
string local_file
int32 status
---
# 结果（Result）：任务完成后返回一次
string id
int32 status
string err
---
# 反馈（Feedback）：任务执行过程中可多次发布
string id
int32 status
int32 progress    # 0~100
string err
```

**生成 C++ 代码：**
```bash
# CMakeLists.txt 中用 rosidl_generate_interfaces 自动生成
rosidl_generate_interfaces(${PROJECT_NAME}
    "action/McuOta.action"
)
```

### 1.3 C++ Action Server 实现

```cpp
#include <rclcpp_action/rclcpp_action.hpp>
#include <monitor_msgs/action/mcu_ota.hpp>

using McuOtaAction = monitor_msgs::action::McuOta;
using GoalHandle   = rclcpp_action::ServerGoalHandle<McuOtaAction>;

// 创建 Action Server
mActionServer = rclcpp_action::create_server<McuOtaAction>(
    node,
    "mcu_ota",
    // 1. 是否接受 Goal（接受/拒绝）
    [this](const rclcpp_action::GoalUUID &uuid,
           std::shared_ptr<const McuOtaAction::Goal> goal)
        -> rclcpp_action::GoalResponse {
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
    },
    // 2. 是否接受取消请求
    [this](const std::shared_ptr<GoalHandle> goal_handle)
        -> rclcpp_action::CancelResponse {
        return rclcpp_action::CancelResponse::ACCEPT;
    },
    // 3. 开始执行（Goal 已被接受）
    [this](const std::shared_ptr<GoalHandle> goal_handle) {
        this->execute(goal_handle);
    }
);

// 执行函数中发布 Feedback
void execute(const std::shared_ptr<GoalHandle> goal_handle) {
    auto feedback = std::make_shared<McuOtaAction::Feedback>();
    feedback->progress = 50;
    goal_handle->publish_feedback(feedback);  // 发布进度

    // 完成时
    auto result = std::make_shared<McuOtaAction::Result>();
    result->status = 0;
    goal_handle->succeed(result);  // 成功
    // 或 goal_handle->abort(result);  // 失败
}
```

### 1.4 C++ Action Client 实现（对比参考）

```cpp
auto client = rclcpp_action::create_client<McuOtaAction>(node, "mcu_ota");

auto goal = McuOtaAction::Goal();
goal.id = "task_001";

auto options = rclcpp_action::Client<McuOtaAction>::SendGoalOptions();
options.feedback_callback = [](auto, auto feedback) {
    std::cout << "进度: " << feedback->progress << "%\n";
};
options.result_callback = [](const auto &result) {
    std::cout << "完成，状态: " << result.result->status << "\n";
};

client->async_send_goal(goal, options);
```

**项目中出现位置：**
`RosModule/src/ROS/McuOtaServer.h`（`rclcpp_action::Server<McuOtaAction>`）
`common/monitor_msgs/action/McuOta.action`
`common/data_capture_msgs/action/DataCapture.action`

**学习资源关键词：** `ROS2 rclcpp_action server tutorial`, `ROS2 action interface C++`

---

## 模块二：fork/waitpid 进程守护（看门狗）模式

### 为什么需要学

项目 `main.cpp` 实现了一个**进程级看门狗**：父进程 fork 出子进程，子进程负责实际运行，父进程持续等待，若子进程意外崩溃则自动重启。这是 Linux 嵌入式/车端服务的常用高可用模式，已有路线图未覆盖。

**学习要点：**

```cpp
#include <sys/wait.h>   // waitpid
#include <unistd.h>     // fork

int monitorRun(int argc, char *argv[]) {
    while (true) {
        pid_t pid = fork();   // 复制进程

        if (pid > 0) {
            // 父进程：等待子进程退出
            int status = 0;
            waitpid(pid, &status, 0);  // 阻塞等待

            // 判断子进程退出原因
            int exitCode = status & 0x7F;  // 信号编号（0=正常退出）
            if (exitCode == 0 || exitCode == 11) {
                break;  // 正常退出，父进程也退出
            }
            // 否则继续循环（重启子进程）
        }
        else if (pid == 0) {
            // 子进程：执行实际业务
            return run(argc, argv);
        }
    }
    return 0;
}
```

**关键系统调用：**
- `fork()` —— 复制当前进程，返回值：父进程得到子进程 PID，子进程得到 0
- `waitpid(pid, &status, 0)` —— 阻塞等待指定 PID 子进程退出
- `status & 0x7F` —— 提取子进程因哪个信号退出（0=正常，6=abort，11=segfault）

**项目中出现位置：**
`service/mcu_ota/src/main.cpp` 中的 `monitorRun()` 函数

**学习资源关键词：** `Linux fork waitpid process watchdog`, `fork 进程守护 Linux`

---

## 模块三：QDateTime（Qt 日期时间）

### 为什么需要学

已有路线图覆盖了 C++ 标准库的 `std::chrono`，但项目中大量使用 Qt 自己的 `QDateTime` 类，两者 API 完全不同。`QDateTime` 更适合日期字符串格式化、时区处理。

**学习要点：**
```cpp
#include <QDateTime>

// 获取当前时间
QDateTime now = QDateTime::currentDateTime();

// 转换为时间戳（秒）
qint64 secs = now.toSecsSinceEpoch();

// 从时间戳创建
QDateTime dt = QDateTime::fromSecsSinceEpoch(1700000000);

// 时间差（秒）
QDateTime birthday = QDateTime::currentDateTime();
// ... 一段时间后
QDateTime now2 = QDateTime::currentDateTime();
qint64 elapsed = now2.toSecsSinceEpoch() - birthday.toSecsSinceEpoch();

// 格式化为字符串
QString str = now.toString("yyyy-MM-dd hh:mm:ss");

// 从字符串解析
QDateTime dt2 = QDateTime::fromString("2024-01-01 12:00:00", "yyyy-MM-dd hh:mm:ss");
```

**与 `std::chrono` 的关键区别：**
- `QDateTime` 擅长日期字符串解析/格式化、时区转换
- `std::chrono` 擅长高精度计时、时间差计算
- Qt 项目中两者经常混用

**项目中出现位置：**
`service/mcu_ota/src/main.cpp`（`QDateTime::currentDateTime()`，进程重启时间判断）
`Common/src/info/UpgradeTaskInfo.h`（任务时间戳）

**学习资源关键词：** `Qt QDateTime tutorial`, `QDateTime format string`

---

## 模块四：CRC 校验算法（CRC8 / CRC16）

### 为什么需要学

已有路线图提到 OpenSSL 密码学（DES/RSA/MD5），但 **CRC 校验**是嵌入式通信中最常用的**数据完整性验证**算法，与密码学 hash 不同，CRC 更轻量、专为通信差错检测设计，项目 OTA 协议直接使用 CRC16。

**概念对比：**

| 算法 | 输出长度 | 用途 | 速度 |
|------|---------|------|------|
| CRC8 | 1 字节 | 短帧校验（CAN帧头）| 极快 |
| CRC16 | 2 字节 | 中等帧校验（OTA包）| 很快 |
| CRC32 | 4 字节 | 文件完整性（Zip/Ethernet）| 快 |
| MD5 | 16 字节 | 文件指纹（不可逆）| 中等 |
| SHA256 | 32 字节 | 安全哈希 | 慢 |

**CRC16 基本原理：**
```cpp
// 查表法 CRC16-CCITT 实现
uint16_t calcCrc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;  // 多项式
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
```

**项目中使用方式：**
```cpp
// OTA 包尾部附加 CRC16
uint16_t crc = Common::calcCrc16(buffer, packetSize - 2);
buffer[packetSize - 2] = (uint8_t)(crc >> 8);    // 高字节
buffer[packetSize - 1] = (uint8_t)(crc & 0xFF);  // 低字节
```

**项目中出现位置：**
`Common/src/Common.h`（`calcCrc8()`, `calcCrc16()`）
`OTAModuleAgent.cpp`（每个 530 字节 OTA 包末尾 2 字节）

**学习资源关键词：** `CRC16 CCITT algorithm C`, `CRC checksum embedded`, `CRC8 CRC16 原理`

---

## 模块五：MD5 文件哈希验证

### 为什么需要学

已有路线图提到 OpenSSL，但未专门说明 **MD5 用于固件完整性验证** 的使用模式。这是 OTA 升级的标准安全步骤：云端下发固件时附带 MD5，本地下载完成后验证，防止传输损坏或替换。

**学习要点：**
```cpp
#include <openssl/md5.h>

// 计算文件的 MD5（16字节 = 32位十六进制字符串）
std::string calcFileMd5(const std::string &filePath) {
    FILE *f = fopen(filePath.c_str(), "rb");
    MD5_CTX ctx;
    MD5_Init(&ctx);

    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        MD5_Update(&ctx, buf, n);
    }
    fclose(f);

    uint8_t digest[16];
    MD5_Final(digest, &ctx);

    // 转为十六进制字符串
    char hex[33];
    for (int i = 0; i < 16; i++) {
        sprintf(hex + i*2, "%02x", digest[i]);
    }
    return std::string(hex);
}

// 使用：下载完固件后对比 MD5
std::string localMd5 = calcFileMd5("/autocity/upgrade/mcu.bin");
if (localMd5 != taskInfo.md5) {
    // 文件损坏，重新下载
}
```

**项目中出现位置：**
`Common/src/Common.h`（`calcMd5()`）
`Common/src/info/UpgradeTaskInfo.h`（`md5` 字段）

**学习资源关键词：** `OpenSSL MD5 file hash C++`, `firmware integrity verification OTA`

---

## 模块六：OTA MCU 固件升级协议设计

### 为什么需要学

这是本项目**最核心的业务逻辑**，涉及嵌入式固件升级的完整协议设计，包括：状态机、二进制帧封装、CAN 分帧传输、Bootloader 跳转机制。这些是车载嵌入式开发的专项知识。

### 6.1 OTA 状态机

```
INIT
 ↓ 收到升级任务
ENTER_UPDATE_MODE   → 发送 OTA_MODE_SWITCH 命令
 ↓
CHECK_VERSION       → 发送 OTA_CHECK_VERSION，确认目标固件版本
 ↓
JUMP_TO_BOOT        → 发送 OTA_JUMP_TO_BOOT，让 MCU 跳入 Bootloader
 ↓
SEND_BIN_FILE       → 分包发送 .bin 固件数据（循环发送所有包）
 ↓
SEND_FILE_END       → 发送 OTA_CONTENT_FINISH，通知传输完成
 ↓
CHECK_NEW_VERSION   → 发送 OTA_CHECK_VERSION，验证烧录结果
 ↓
EXIT_UPDATE_MODE    → 发送 OTA_MODE_SWITCH，恢复正常模式
 ↓
SUCCESS / FAIL
```

### 6.2 OTA 协议包格式（530 字节）

```
┌───────────────────────────────────────────────────────────┐
│ PACKAGE_HEAD  │ SrcAddr │ DstAddr │ TotalNum │ PacketNum  │
│  4字节"CTRL"  │  1字节  │  1字节  │   2字节  │   2字节   │
├───────────────────────────────────────────────────────────┤
│              固件数据（512字节，不足用0xFF填充）            │
├───────────────────────────────────────────────────────────┤
│ PACKAGE_TAIL  │ CRC16  │
│ 8字节"AUTOCITY" │ 2字节  │
└───────────────────────────────────────────────────────────┘
      总计：4+1+1+2+2+512+8+2 = 532 字节
```

### 6.3 CAN 分帧传输

530 字节的 OTA 包无法用单个 CAN 帧传输（CAN 帧最大 8 字节数据），需要**分帧**：

```cpp
// 每帧 8 字节：1字节帧序号 + 7字节数据
// 530 / 7 = 75.7，向上取整 = 76 帧
int frameCount = (packetSize + 6) / 7;

for (int i = 0; i < frameCount; i++) {
    uint8_t frame[8] = {0};
    frame[0] = i;  // 帧序号（接收方用于重组）

    int copySize = std::min(7, packetSize - i * 7);
    std::memcpy(frame + 1, buffer + i * 7, copySize);

    // 发送单个 CAN 帧（can_frame.can_dlc = 8）
    writeCAN(canId, frame, 8);
    // 等待 ACK
}
```

### 6.4 Bootloader 跳转机制

```cpp
// 发送 JUMP_TO_BOOT 命令，MCU 收到后重启进入 Bootloader
sendOtaCommand(addr, OtaCmd::OTA_JUMP_TO_BOOT);

// MCU Bootloader 收到固件包后写入 Flash，完成后重启正常程序
// MCU 端（伪代码）
if (received_cmd == OTA_JUMP_TO_BOOT) {
    save_current_state();
    jump_to_bootloader_address();  // 通过修改中断向量表/栈指针跳转
}
```

**项目中出现位置：**
`OTAModuleAgent/src/OTAModuleAgent.h`、`.cpp`（完整实现）
`Common/src/Common.h`（`OtaCmd`、`OtaAddr`、`OtaStatus` 枚举定义）

**学习资源关键词：** `MCU bootloader OTA firmware update CAN`, `OTA state machine embedded`, `CAN frame firmware transfer`

---

## 模块七：MCAP 文件格式（机器人数据录制）

### 为什么需要学

项目 `common_record` 子系统使用 **MCAP** 格式录制传感器数据，这是 ROS2 生态系统最新的数据录制格式（2022年后逐渐取代旧的 rosbag2 `.db3` 格式）。已有路线图只提到了 `rosbag2_cpp::Writer`，未涉及 MCAP 本身。

**是什么：**
MCAP（Multi-Channel Append-only Prefixed）是 Foxglove 设计的二进制容器格式，用于存储多路传感器时序数据（点云、图像、CAN 帧、规划轨迹等）。

**特点：**
- **多通道**：一个文件可存储任意多路数据流
- **时间索引**：支持快速按时间范围检索
- **Schema 内嵌**：文件自包含数据描述（Protobuf schema 打包在内）
- **压缩**：可选 LZ4 或 Zstd 块压缩

**与 rosbag2 的对比：**

| 特性 | rosbag2 (.db3) | MCAP |
|------|---------------|------|
| 底层格式 | SQLite3 | 自定义二进制 |
| 性能 | 中等 | 高 |
| 可视化工具 | ros2 bag play | Foxglove Studio |
| 跨平台 | ROS2 依赖 | 无依赖，纯二进制 |
| 压缩 | 无 | LZ4/Zstd |

**项目中出现位置：**
`common/common_record/mcap_vendor/`（第三方 MCAP 库）

**学习资源关键词：** `MCAP file format robotics`, `MCAP C++ library`, `mcap vs rosbag2`

---

## 模块八：Foxglove Studio（机器人数据可视化）

### 为什么需要学

Foxglove Studio 是 MCAP 格式的标准可视化工具，是现代 ROS2 开发的重要调试工具，逐步取代 RViz + rqt 的使用场景。已有路线图未提及。

**是什么：**
一个跨平台的机器人数据可视化与调试 IDE，支持：
- 回放 MCAP/rosbag2 文件
- 实时连接 ROS2 节点
- 显示点云、图像、时序图表、地图、TF 变换
- 支持 Protobuf 消息的 schema 注册

**常用场景：**
```bash
# 安装 Foxglove Studio（桌面端）
# 从 foxglove.dev 下载

# 命令行录制 MCAP
ros2 bag record -s mcap -o my_recording.mcap /topic1 /topic2
```

**项目中出现位置：**
`common/common_record/foxglove/`（Foxglove 相关集成代码）

**学习资源关键词：** `Foxglove Studio tutorial`, `Foxglove ROS2 visualization`, `foxglove.dev`

---

## 模块九：LZ4 压缩库

### 为什么需要学

项目数据录制子系统用 **LZ4** 对 MCAP 块进行压缩，这是机器人/嵌入式领域最流行的实时压缩算法。已有路线图未覆盖任何压缩库。

**是什么：**
LZ4 是一种极高速的无损压缩算法，压缩速度可达 400-500 MB/s，解压更快（可超过 1 GB/s），适合实时数据流压缩。

**学习要点：**
```cpp
#include <lz4.h>

// 压缩
char src[] = "your raw sensor data...";
int srcSize = strlen(src);

int maxDstSize = LZ4_compressBound(srcSize);  // 最大压缩输出大小
char *dst = new char[maxDstSize];

int compressedSize = LZ4_compress_default(src, dst, srcSize, maxDstSize);
// compressedSize 是实际压缩后大小

// 解压
char *recovered = new char[srcSize];
int decompressedSize = LZ4_decompress_safe(dst, recovered, compressedSize, srcSize);
```

**压缩率对比（传感器数据）：**

| 算法 | 压缩速度 | 压缩率 | 典型用途 |
|------|---------|--------|---------|
| LZ4 | 极快 | ~中等 | 实时流、嵌入式 |
| Zstd | 快 | 高 | 日志归档、MCAP |
| gzip | 慢 | 高 | 文件传输 |

**项目中出现位置：**
`common/common_record/lz4/`（LZ4 源码集成）

**学习资源关键词：** `LZ4 compression library C`, `lz4 github`, `LZ4 vs zstd`

---

## 模块十：进程参数解析（argc/argv）

### 为什么需要学

这是一个小但实用的知识点：已有路线图未明确介绍 Linux 程序命令行参数解析。项目展示了标准的手动解析模式，是所有服务程序的基础。

**学习要点：**
```cpp
int main(int argc, char *argv[]) {
    // argc = 参数个数（含程序名）
    // argv[0] = 程序路径，argv[1]~argv[argc-1] = 用户参数

    for (int i = 1; i < argc; ++i) {
        std::string param(argv[i]);
        if (param == "-l" && i + 1 < argc) {
            setLogLevel(argv[i + 1]);
            i++;  // 跳过下一个参数
        }
        else if (param == "-m") {
            monitor = true;
        }
        else if (param == "-h") {
            printHelp();
        }
    }
}
```

**实际运行：**
```bash
./mcu_ota -m             # 监控模式
./mcu_ota -l debug       # 设置日志级别
./mcu_ota -h             # 帮助信息
```

**更健壮的替代：** `getopt()`、`getopt_long()`（POSIX 标准）或 `boost::program_options`

**项目中出现位置：**
`service/mcu_ota/src/main.cpp`

**学习资源关键词：** `Linux getopt argc argv tutorial`, `command line argument parsing C++`

---

## 模块十一：QCoreApplication（无 GUI 的 Qt 应用）

### 为什么需要学

已有路线图的 Qt 部分主要针对 HMI 界面（`QApplication` + QML），但本项目作为一个**后台服务**，使用 `QCoreApplication`。两者是 Qt 应用的不同起点。

**学习要点：**
```cpp
#include <QCoreApplication>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);  // 无 GUI 的 Qt 事件循环

    // 初始化各模块...

    return app.exec();   // 进入事件循环（处理信号槽、定时器等）
    // 或者直接 return 0（不需要事件循环时）
}
```

**两者区别：**

| 类 | 用途 | 是否需要显示器 |
|----|------|--------------|
| `QCoreApplication` | 后台服务、命令行工具 | 否 |
| `QApplication` | 带 GUI 的桌面应用 | 是 |
| `QGuiApplication` | QML 应用（无 Widgets）| 是 |

**学习资源关键词：** `QCoreApplication vs QApplication Qt`, `Qt headless service`

---

## 推荐学习顺序

| 阶段 | 模块 | 预计时间 | 优先级 |
|------|------|----------|--------|
| 第一阶段 | 模块六（OTA 协议设计）| 2天 | ★★★★★ |
| 第一阶段 | 模块一（ROS2 Action）| 1天 | ★★★★★ |
| 第二阶段 | 模块四（CRC 校验算法）| 半天 | ★★★★☆ |
| 第二阶段 | 模块五（MD5 文件验证）| 半天 | ★★★★☆ |
| 第二阶段 | 模块二（fork/waitpid 看门狗）| 半天 | ★★★☆☆ |
| 第三阶段 | 模块三（QDateTime）| 半天 | ★★★☆☆ |
| 第三阶段 | 模块七（MCAP 文件格式）| 1天 | ★★★☆☆ |
| 第四阶段 | 模块八（Foxglove Studio）| 半天 | ★★☆☆☆ |
| 第四阶段 | 模块九（LZ4 压缩）| 半天 | ★★☆☆☆ |
| 第五阶段 | 模块十（argc/argv 解析）| 半天 | ★★☆☆☆ |
| 第五阶段 | 模块十一（QCoreApplication）| 半天 | ★★☆☆☆ |

---

## 推荐学习资源

| 知识点 | 推荐资源 |
|--------|----------|
| ROS2 Action | `docs.ros.org/en/humble/Tutorials/Intermediate/Writing-an-Action-Server-Client/Cpp.html` |
| rclcpp_action | `docs.ros2.org/latest/api/rclcpp_action` |
| fork/waitpid | `man7.org/linux/man-pages/man2/fork.2.html` |
| QDateTime | `doc.qt.io/qt-5/qdatetime.html` |
| CRC16 算法原理 | `barrgroup.com/embedded-systems/how-to/crc-calculation-c-code` |
| MD5 OpenSSL | `openssl.org/docs/man3.0/man3/MD5.html` |
| MCAP 格式 | `mcap.dev/docs` |
| Foxglove Studio | `foxglove.dev/docs` |
| LZ4 | `lz4.github.io/lz4` |
