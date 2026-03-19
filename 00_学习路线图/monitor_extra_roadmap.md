# Monitor 项目补充学习路线图

> 在已有两份路线图（camera + monitor）的基础上，本文档**仅记录新发现的、未覆盖的**知识点。
> 来源：对项目进行完整二次深读后发现。

---

## 模块一：Vue 3 前端技术栈（Web 调试界面）

### 为什么需要学

项目在 `src/monitor/html/` 目录下内置了一套 **Web 前端调试界面**，通过浏览器访问 `http://车辆IP:1225` 来调试和控制车辆。这套前端用 Vue 3 写成，与 C++ 后端通过 WebSocket/HTTP 通信。

### 1.1 Vue 3 基础

**是什么：**
渐进式 JavaScript 前端框架，用组件化方式构建 UI。

**学习要点：**
```typescript
// 组合式 API (Composition API) - Vue 3 核心写法
import { ref, reactive, computed, onMounted } from 'vue'

const count = ref(0)         // 响应式基本类型
const state = reactive({})   // 响应式对象
const double = computed(() => count.value * 2)  // 计算属性

onMounted(() => { /* 组件挂载后执行 */ })
```

**项目中出现位置：**
`html/src/view/` 下所有 `.vue` 文件

**学习资源关键词：** `Vue 3 composition API tutorial`

---

### 1.2 TypeScript 基础

**是什么：**
JavaScript 的超集，添加了静态类型检查。项目的 html 目录全部用 TypeScript 写。

**学习要点：**
- 基本类型：`string`, `number`, `boolean`, `string[]`, `Record<string, any>`
- 接口：`interface VehicleStatus { speed: number; mode: string }`
- 类型断言：`(data as VehicleStatus).speed`
- 可选属性：`name?: string`
- 泛型：`function get<T>(url: string): Promise<T>`

**学习资源关键词：** `TypeScript for beginners`, `TypeScript handbook`

---

### 1.3 Vite 构建工具

**是什么：**
Vue 3 官方推荐的构建工具，替代 Webpack，启动极快。

**学习要点：**
- `vite.config.ts` —— 配置文件
- **代理配置**（项目中实际使用）：
```typescript
// vite.config.ts
export default {
  server: {
    proxy: {
      '/monitor': {
        target: 'http://192.168.31.11:1225',  // 转发到车辆IP
        changeOrigin: true
      }
    }
  }
}
```
- `npm run dev` —— 开发服务器
- `npm run build` —— 生产构建

**学习资源关键词：** `Vite config proxy tutorial`

---

### 1.4 Vue Router 前端路由

**是什么：**
Vue 的官方路由库，实现单页应用（SPA）中的页面切换，不刷新整个页面。

**学习要点：**
```typescript
// 定义路由
const routes = [
  { path: '/', component: HomePage },
  { path: '/task', component: TaskPage },
  { path: '/fault', component: FaultPage },
]

// 在模板中使用
// <RouterLink to="/task">任务</RouterLink>
// <RouterView />  ← 渲染当前路由对应的组件
```

**学习资源关键词：** `Vue Router 4 tutorial`

---

### 1.5 Pinia 状态管理

**是什么：**
Vue 3 官方推荐的全局状态管理库，替代 Vuex。用于多个组件共享数据。

**学习要点：**
```typescript
// 定义 store
import { defineStore } from 'pinia'

export const useVehicleStore = defineStore('vehicle', {
  state: () => ({ speed: 0, mode: 'idle' }),
  actions: {
    updateSpeed(v: number) { this.speed = v }
  }
})

// 在组件中使用
const store = useVehicleStore()
store.updateSpeed(30)
console.log(store.speed)
```

**学习资源关键词：** `Pinia Vue 3 state management tutorial`

---

### 1.6 Element Plus UI 组件库

**是什么：**
基于 Vue 3 的桌面端 UI 组件库，提供按钮、表格、表单、弹窗等现成组件。

**学习要点（常用组件）：**
```vue
<el-button type="primary" @click="start">开始作业</el-button>
<el-table :data="taskList">
  <el-table-column prop="id" label="任务ID" />
  <el-table-column prop="status" label="状态" />
</el-table>
<el-dialog v-model="dialogVisible" title="故障详情">
  ...
</el-dialog>
```

**学习资源关键词：** `Element Plus components docs`

---

### 1.7 Axios HTTP 客户端（TypeScript 版）

**是什么：**
前端发送 HTTP/WebSocket 请求的库，用于调用 C++ 后端的 REST 接口。

**学习要点：**
```typescript
import axios from 'axios'

// GET 请求
const resp = await axios.get('/monitor/vehicle/task/get')
const task = resp.data

// POST 请求
await axios.post('/monitor/vehicle/task/start', { taskId: '123' })

// 拦截器（统一处理错误/token）
axios.interceptors.response.use(
  res => res,
  err => { console.error(err.response.status); return Promise.reject(err) }
)
```

**学习资源关键词：** `Axios TypeScript tutorial`

---

### 1.8 Sass/SCSS 样式

**是什么：**
CSS 预处理器，让 CSS 支持变量、嵌套、函数，减少重复。

**学习要点：**
```scss
$primary: #409eff;    // 变量

.vehicle-card {
  color: $primary;
  &:hover {          // 嵌套（等价于 .vehicle-card:hover）
    opacity: 0.8;
  }
  .title {           // 嵌套子选择器
    font-size: 16px;
  }
}
```

**学习资源关键词：** `SCSS basics tutorial`

---

## 模块二：QAbstractListModel（Qt 列表数据绑定）

### 为什么需要学

项目中大量使用 `QAbstractListModel` 将 C++ 数据（如故障列表、任务列表、传感器数据）暴露给 QML 的 `ListView`，这是 Qt/QML 开发的核心模式，已有路线图未覆盖。

**学习要点：**
```cpp
class TaskListModel : public QAbstractListModel {
    Q_OBJECT
public:
    // 必须实现的3个函数：
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;  // 定义角色名（QML用来访问字段）
};
```

**完整示例：**
```cpp
// C++ 侧
enum Roles { IdRole = Qt::UserRole + 1, StatusRole };

QHash<int, QByteArray> TaskListModel::roleNames() const {
    return { {IdRole, "taskId"}, {StatusRole, "status"} };
}

QVariant TaskListModel::data(const QModelIndex &index, int role) const {
    if (role == IdRole)     return mList[index.row()].id;
    if (role == StatusRole) return mList[index.row()].status;
    return {};
}

// QML 侧
ListView {
    model: taskListModel      // C++ 注册的 QAbstractListModel
    delegate: Text {
        text: taskId + ": " + status  // 直接用 roleNames 中定义的名字
    }
}
```

**项目中出现位置：**
`UIModule/src/roscommunicate/common/qmlobjectlistmodel.h`
`Engineering/src/logsettings/qmlobjectlistmodel.h`

**学习资源关键词：** `QAbstractListModel QML tutorial`, `Qt model view QML`

---

## 模块三：Qt 元对象系统深入（QMetaProperty / QMetaMethod）

### 为什么需要学

项目中 `MetaObjectPropertyBinder` 类使用了 Qt 反射机制（运行时动态获取/设置属性），是实现数据绑定框架的底层工具。

**学习要点：**
```cpp
QObject *obj = ...;

// 运行时获取属性（不需要知道属性名是什么，可以动态遍历）
const QMetaObject *meta = obj->metaObject();
for (int i = 0; i < meta->propertyCount(); i++) {
    QMetaProperty prop = meta->property(i);
    QString name = prop.name();                     // 属性名
    QVariant value = prop.read(obj);                // 读取值
    prop.write(obj, QVariant("newValue"));          // 写入值
}

// 运行时调用方法
QMetaObject::invokeMethod(obj, "mySlot",
    Q_ARG(int, 42));
```

**项目中出现位置：**
`UIModule/src/meta/metaobjectpropertybinder.h`

**学习资源关键词：** `Qt QMetaProperty runtime reflection`, `QMetaObject dynamic property`

---

## 模块四：QtConcurrent 并发框架

### 为什么需要学

项目 `main.cpp` 中用 `QtConcurrent::run` 在 Qt 线程池中运行消息路由，这与 `std::thread` 有本质区别——它是 Qt 管理的线程池，结果通过 `QFuture` 获取。

**学习要点：**
```cpp
#include <QtConcurrent>
#include <QFutureWatcher>

// 在线程池中异步执行函数
QFuture<bool> future = QtConcurrent::run([&]() -> bool {
    // 耗时操作（在后台线程执行）
    messageRoute.beginWork();
    return true;
});

// 监听完成事件（在主线程中回调）
QFutureWatcher<bool> watcher;
connect(&watcher, &QFutureWatcher<bool>::finished, [&]() {
    qDebug() << "result:" << watcher.result();
});
watcher.setFuture(future);
```

**与 `std::thread` 的关键区别：**
- `std::thread` 需要手动管理线程生命周期
- `QtConcurrent` 使用全局线程池，自动复用线程，性能更好
- `QFuture` 可以获取返回值，`std::thread` 不行

**项目中出现位置：**
`service/monitor/src/main.cpp`

**学习资源关键词：** `QtConcurrent run QFuture tutorial`

---

## 模块五：QTimer（Qt 定时器）

### 为什么需要学

项目中点云可视化（`PointCloudViewer`）用 `QTimer` 定时刷新渲染，这是 Qt 应用中最常用的定时机制，与 `std::chrono` + `std::thread` sleep 完全不同。

**学习要点：**
```cpp
#include <QTimer>

QTimer *timer = new QTimer(this);

// 连接超时信号到槽函数
connect(timer, &QTimer::timeout, this, &MyClass::update);

timer->start(100);    // 每 100ms 触发一次
timer->stop();        // 停止

// 单次定时（只触发一次）
QTimer::singleShot(5000, this, &MyClass::onTimeout);
```

**与 `std::thread` sleep 的对比：**
- `QTimer` 在主线程事件循环中触发（不阻塞 UI）
- sleep 会阻塞当前线程（在 UI 线程中用 sleep 会卡死界面）

**项目中出现位置：**
`UIModule/src/roscommunicate/pointCloudViewer/pointCloudViewer.h`（`QSharedPointer<QTimer> m_reflashTimer`）

**学习资源关键词：** `QTimer Qt tutorial`

---

## 模块六：PCL 点云库

### 为什么需要学

项目中 `PointCloudViewer` 类使用 PCL 处理激光雷达点云数据，并通过 VTK 在 Qt 窗口中 3D 渲染。

### 6.1 PCL 基础概念

**是什么：**
Point Cloud Library，处理 3D 点云数据的 C++ 库，用于激光雷达数据处理。

**学习要点：**
```cpp
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

// 常用点类型
pcl::PointXYZ     point;   // 只有 xyz 坐标
pcl::PointXYZRGB  point;   // xyz + RGB 颜色
pcl::PointXYZI    point;   // xyz + 强度（激光雷达常用）

// 创建点云
pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
cloud->push_back({1.0f, 2.0f, 3.0f});

// 遍历
for (auto &pt : cloud->points) {
    std::cout << pt.x << " " << pt.y << " " << pt.z;
}
```

**项目中出现位置：**
`UIModule/src/roscommunicate/pointCloudViewer/pointCloudViewer.h`

**学习资源关键词：** `PCL point cloud library tutorial C++`

---

### 6.2 VTK + PCLVisualizer 可视化

**是什么：**
VTK（Visualization Toolkit）是 PCL 的底层渲染库，`PCLVisualizer` 是 PCL 提供的可视化工具，基于 VTK 实现。

**学习要点：**
```cpp
#include <pcl/visualization/pcl_visualizer.h>

pcl::visualization::PCLVisualizer::Ptr viewer(
    new pcl::visualization::PCLVisualizer("3D Viewer"));

// 添加点云
viewer->addPointCloud<pcl::PointXYZ>(cloud, "cloud_id");

// 更新点云（数据刷新）
viewer->updatePointCloud<pcl::PointXYZ>(cloud, "cloud_id");

// 设置背景色、点大小
viewer->setBackgroundColor(0, 0, 0);
viewer->setPointCloudRenderingProperties(
    pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 2, "cloud_id");
```

**嵌入 Qt 窗口（项目实际做法）：**
PCLVisualizer 的 VTK RenderWindow 可以嵌入到 `QVTKWidget` 中，实现在 Qt 对话框内显示 3D 点云。

**学习资源关键词：** `PCLVisualizer Qt VTK integration`, `PCL visualization tutorial`

---

## 模块七：/proc 文件系统（进程与资源监控）

### 为什么需要学

项目 `ProcessModule` 通过读取 `/proc/[pid]/stat` 获取进程 CPU/内存占用，`ResourceModule` 通过 `/proc/stat`、`/proc/meminfo` 获取系统资源使用率。这是 Linux 系统监控的标准方法，已有路线图未覆盖。

**学习要点：**

**① 进程统计（/proc/[pid]/stat）：**
```
cat /proc/1234/stat
# 输出：1234 (myprocess) S 1 1234 1234 0 ... utime stime ...
# 字段含义：pid, name, state, ppid, ..., utime(用户态时间), stime(内核态时间), ...
```

**② 系统 CPU 使用率（/proc/stat）：**
```
cat /proc/stat
# cpu  262538 363 70046 7291779 12394 0 2312 0 0 0
# 字段：user nice system idle iowait irq softirq ...
# CPU使用率 = (total - idle) / total * 100%
```

**③ 内存（/proc/meminfo）：**
```
cat /proc/meminfo
# MemTotal: 16384000 kB
# MemFree:  8192000 kB
# Buffers:  512000 kB
# Cached:   2048000 kB
# 实际可用 = MemFree + Buffers + Cached
```

**④ 磁盘（/proc/diskstats 或 statvfs）：**
```cpp
struct statvfs stat;
statvfs("/autocity/data", &stat);
long long total = stat.f_blocks * stat.f_frsize;
long long free  = stat.f_bfree  * stat.f_frsize;
```

**项目中出现位置：**
`ProcessModule/src/common/ProcessStat.h`（解析 `/proc/[pid]/stat`）
`ResourceModule/src/common/CPU.h`、`Memory.h`

**学习资源关键词：** `Linux /proc filesystem process monitoring`, `proc stat cpu usage calculation`

---

## 模块八：RSA 非对称加密

### 为什么需要学

已有路线图只提到了 OpenSSL 的 DES（对称加密），但项目中 `Crypto.h` 还用了 **RSA 非对称加密**，用于设备认证和云端通信的安全验证。

**概念先行：**
- **对称加密（DES/AES）**：加密和解密用同一把密钥，速度快，用于数据加密
- **非对称加密（RSA）**：公钥加密、私钥解密（或私钥签名、公钥验签），用于身份验证/密钥交换

**学习要点：**
```cpp
#include <openssl/rsa.h>
#include <openssl/pem.h>

// 公钥加密（任何人都能加密，只有持有私钥的人能解密）
RSA *rsa = PEM_read_bio_RSAPublicKey(bio, NULL, NULL, NULL);
RSA_public_encrypt(dataLen, (uint8_t*)data.c_str(),
                   (uint8_t*)output, rsa, RSA_PKCS1_PADDING);

// 私钥解密
RSA *rsa = PEM_read_bio_RSAPrivateKey(bio, NULL, NULL, NULL);
RSA_private_decrypt(encLen, (uint8_t*)enc.c_str(),
                    (uint8_t*)output, rsa, RSA_PKCS1_PADDING);
```

**项目中出现位置：**
`Common/src/crypto/Crypto.h`（`RSAPublicKeyEncrypt`, `RSAPrivateKeyDecrypt`）

**学习资源关键词：** `OpenSSL RSA encryption C++ example`

---

## 模块九：Base64 编码

### 为什么需要学

项目中 RSA 公私钥以 Base64 形式存储在代码里，传输二进制数据时也用 Base64。这是网络编程、加密的基础知识。

**是什么：**
将任意二进制数据编码为纯 ASCII 字符（只含 A-Z、a-z、0-9、+、/），便于在文本协议（JSON、HTTP）中传输。

**学习要点：**
```cpp
// OpenSSL 提供 Base64 工具
#include <openssl/bio.h>
#include <openssl/evp.h>

// 编码
BIO *b64 = BIO_new(BIO_f_base64());
BIO *mem = BIO_new(BIO_s_mem());
BIO_push(b64, mem);
BIO_write(b64, data, dataLen);
BIO_flush(b64);
// 读出结果...

// 编码规律：3字节二进制 → 4字节ASCII（体积增加约33%）
// "Man" → "TWFu"
```

**学习资源关键词：** `Base64 encoding explained`, `OpenSSL base64 encode decode C++`

---

## 模块十：数据库版本迁移（Schema Migration）

### 为什么需要学

项目 `DataBaseVersion.h` 实现了数据库版本管理机制：应用每次启动时检测当前数据库版本，如果版本低则执行升级 SQL。这是生产级应用的标准数据库管理模式。

**是什么：**
随着软件版本迭代，数据库表结构会变化（加字段、改字段）。版本迁移保证老设备升级后数据库能自动更新，不丢失历史数据。

**项目实现方式：**
```cpp
// DataBaseVersion.h
namespace DataBaseVersion {
    const std::string v0_1_0 = R"(
        CREATE TABLE IF NOT EXISTS delay_task(...);
        INSERT INTO sql_version(version) VALUES('0.1.0');
    )";

    const std::string v0_1_1 = R"(
        CREATE TABLE IF NOT EXISTS task_history(...);
        UPDATE sql_version SET version='0.1.1';
    )";

    const std::string v0_1_2 = R"(
        ALTER TABLE task_history ADD COLUMN extra_info TEXT;
        UPDATE sql_version SET version='0.1.2';
    )";
}

// 启动时执行迁移
void DatabaseModule::migrate() {
    std::string curVer = queryCurrentVersion();
    if (curVer < "0.1.0") execute(DataBaseVersion::v0_1_0);
    if (curVer < "0.1.1") execute(DataBaseVersion::v0_1_1);
    if (curVer < "0.1.2") execute(DataBaseVersion::v0_1_2);
}
```

**学习资源关键词：** `database schema migration versioning pattern`, `SQLite schema migration C++`

---

## 模块十一：BIOS/硬件指纹（设备唯一标识）

### 为什么需要学

项目 `HostInfo.h` 读取主板序列号、BIOS UUID 作为设备唯一标识，用于许可证绑定和设备认证。这是嵌入式/车端软件的常见需求。

**Linux 实现方式：**
```bash
# 读取 BIOS 序列号
sudo dmidecode -s bios-version
cat /sys/class/dmi/id/product_serial

# 读取主板序列号
sudo dmidecode -s baseboard-serial-number
cat /sys/class/dmi/id/board_serial

# 读取 BIOS UUID
sudo dmidecode -s system-uuid
cat /sys/class/dmi/id/product_uuid
```

**C++ 实现：**
```cpp
// 读取 /sys/class/dmi/id/ 下的文件
std::ifstream f("/sys/class/dmi/id/board_serial");
std::string serial;
std::getline(f, serial);
```

**项目中出现位置：**
`Common/src/unit/HostInfo.h`（`getBIOSId`, `getBaseboardId`, `getBIOSUuid`）

**学习资源关键词：** `Linux read BIOS UUID C++`, `dmidecode /sys/class/dmi`

---

## 推荐学习顺序

| 阶段 | 模块 | 预计时间 | 优先级 |
|------|------|----------|--------|
| 第一阶段 | 模块二（QAbstractListModel）| 1天 | ★★★★★ |
| 第一阶段 | 模块七（/proc 资源监控）| 1天 | ★★★★☆ |
| 第二阶段 | 模块一（Vue 3 + TypeScript + Vite）| 3天 | ★★★★☆ |
| 第二阶段 | 模块四（QtConcurrent）| 半天 | ★★★☆☆ |
| 第二阶段 | 模块五（QTimer）| 半天 | ★★★☆☆ |
| 第三阶段 | 模块一后半（Pinia / Element Plus / Axios）| 2天 | ★★★☆☆ |
| 第三阶段 | 模块八（RSA 加密）| 1天 | ★★★☆☆ |
| 第三阶段 | 模块九（Base64）| 半天 | ★★☆☆☆ |
| 第四阶段 | 模块六（PCL + VTK 点云）| 2天 | ★★☆☆☆ |
| 第四阶段 | 模块三（QMetaProperty 反射）| 1天 | ★★☆☆☆ |
| 第五阶段 | 模块十（数据库迁移模式）| 半天 | ★★☆☆☆ |
| 第五阶段 | 模块十一（BIOS硬件指纹）| 半天 | ★☆☆☆☆ |

---

## 推荐学习资源

| 知识点 | 推荐资源 |
|--------|----------|
| Vue 3 | `vuejs.org/guide/introduction` |
| TypeScript | `typescriptlang.org/docs/handbook/intro` |
| Vite | `vitejs.dev/guide` |
| Vue Router | `router.vuejs.org/guide` |
| Pinia | `pinia.vuejs.org/introduction` |
| Element Plus | `element-plus.org/zh-CN/component/overview` |
| QAbstractListModel | `doc.qt.io/qt-5/qabstractlistmodel.html` |
| QtConcurrent | `doc.qt.io/qt-5/qtconcurrent-index.html` |
| QTimer | `doc.qt.io/qt-5/qtimer.html` |
| PCL | `pointclouds.org/documentation/tutorials` |
| OpenSSL RSA | `wiki.openssl.org/index.php/RSA_public_key_encryption` |
| /proc 监控 | `man7.org/linux/man-pages/man5/proc.5.html` |
