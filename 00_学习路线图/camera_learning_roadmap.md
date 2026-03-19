# 项目学习路线图

> 面向 MCU/STM32 背景开发者，系统学习本相机项目涉及的所有知识点。
> 按"从基础到专项"顺序排列，建议逐条对照学习。

---

## 模块一：Linux 系统编程基础（最核心，必须先学）

本项目大量使用 Linux 系统调用，这是理解 Camera.cpp 的前提。

### 1.1 文件描述符（File Descriptor）

**是什么：**
Linux 中一切皆文件。摄像头设备（`/dev/Video0`）、管道、Socket 都用一个整数（fd）来代表，就像 STM32 里的句柄（handle）。

**学习要点：**
- `open()` —— 打开文件/设备，返回 fd（整数）
- `close()` —— 关闭 fd，释放资源
- `read()` / `write()` —— 基本读写
- 返回值 -1 表示出错，用 `errno` 查错误原因
- `STDIN_FILENO=0`, `STDOUT_FILENO=1`, `STDERR_FILENO=2` 是三个固定 fd

**项目中出现位置：**
`CameraInfo.h` 中的 `fd_` 字段，`Buffer.dma_fd` —— 都是文件描述符。

**学习资源关键词：** `Linux open() 系统调用`, `文件描述符`

---

### 1.2 ioctl —— 设备控制命令

**是什么：**
`ioctl(fd, 命令, 参数)` 是 Linux 控制硬件设备的通用接口。
类比 STM32：就像 HAL_UART_Control() 这类控制函数，但统一成了一个入口。

**学习要点：**
- 语法：`int ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);`
- 第二个参数是"命令宏"，以 `VIDIOC_` 开头的是 V4L2 专用命令
- 返回 -1 表示失败，检查 `errno`
- 常见用法：查询设备能力、设置格式、申请缓冲区、入队/出队缓冲区

**项目中出现位置：**
`Camera.cpp` 中所有 `ioctl` 调用（控制摄像头的核心操作）

**学习资源关键词：** `ioctl Linux 系统调用`, `Linux 设备驱动 ioctl`

---

### 1.3 mmap —— 内存映射

**是什么：**
把文件/设备内存直接映射到进程的虚拟地址空间，避免数据拷贝。
类比 STM32：类似直接操作外设寄存器地址，但更通用。

**学习要点：**
- `mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, offset)` —— 建立映射，返回指针
- `munmap(ptr, size)` —— 解除映射
- `MAP_SHARED` 表示和其他进程/驱动共享同一块物理内存
- 用于 V4L2 缓冲区：摄像头数据直接写入，程序直接读取，**零拷贝**

**项目中出现位置：**
`Camera.cpp` 中 `mmap()` 调用，`Buffer.start` 指针就是 mmap 返回的地址。

**学习资源关键词：** `mmap 内存映射 Linux`, `V4L2 mmap`

---

### 1.4 select / poll —— I/O 多路复用

**是什么：**
同时监听多个 fd，哪个有数据就处理哪个，不用轮询浪费 CPU。
类比 STM32：类似中断机制，但是在用户态程序中实现"等待任意一个事件"。

**学习要点：**
- `select(maxfd+1, &readfds, NULL, NULL, &timeout)` —— 阻塞等待，有数据返回
- `FD_SET(fd, &set)` —— 把 fd 加入监听集合
- `FD_ISSET(fd, &set)` —— 判断哪个 fd 有数据
- `poll()` 是更现代的替代，参数更清晰
- 返回值：正数=有事件，0=超时，-1=出错

**项目中出现位置：**
`Camera.cpp` 的 `cameraThread()` 用 `select` 同时等待多路摄像头数据。

**学习资源关键词：** `select poll Linux 多路复用`, `Linux I/O multiplexing`

---

### 1.5 errno 错误处理

**是什么：**
Linux 系统调用失败时，错误码存入全局变量 `errno`，用 `strerror(errno)` 转成可读字符串。

**学习要点：**
- `#include <errno.h>`
- 常见错误码：`EACCES`(权限拒绝)，`ENOENT`(文件不存在)，`EBUSY`(设备忙)，`EINVAL`(参数无效)
- 每次系统调用后都应检查返回值
- 宏 `ERRNO_STR` 或 `strerror(errno)` 打印可读描述

**项目中出现位置：**
`Camera.cpp` 错误日志中大量使用 `strerror(errno)` 输出错误原因。

---

### 1.6 POSIX 线程（pthread）

**是什么：**
Linux C 原生多线程库。项目中 `JetsonEnc.h` 使用的 `pthread_t`、`pthread_mutex_t`、`pthread_cond_t` 均属于此。

**学习要点：**
- `pthread_create(&tid, NULL, func, arg)` —— 创建线程
- `pthread_join(tid, NULL)` —— 等待线程结束
- `pthread_mutex_lock/unlock()` —— 互斥锁（相当于 STM32 的关中断保护临界区）
- `pthread_cond_wait/signal()` —— 条件变量（线程间通知机制）
- C++ 的 `std::thread` 是对 pthread 的封装，更推荐使用（见模块二）

**项目中出现位置：**
`JetsonEnc.h` 中 `pthread_t job_tid`, `pthread_mutex_t data_mutex`, `pthread_cond_t data_cond`。

**学习资源关键词：** `POSIX pthread 教程`, `Linux 多线程编程`

---

### 1.7 共享内存（Shared Memory）

**是什么：**
两个进程直接读写同一块物理内存，是最快的进程间通信方式。

**学习要点：**
- `shmget()` —— 创建/获取共享内存段
- `shmat()` —— 将共享内存连接到进程地址空间
- `shmdt()` —— 断开连接
- `sys/shm.h`, `sys/ipc.h` —— 相关头文件
- 现代替代方案：`mmap` + `/dev/shm`

**项目中出现位置：**
`Camera.cpp` 引用了 `sys/shm.h`，`sys/ipc.h`；`shm_msgs` 是基于共享内存的 ROS2 消息库。

---

## 模块二：现代 C++（C++11/14/17）

你已有 C/C++ 基础，但本项目大量使用现代 C++ 特性，必须补充。

### 2.1 智能指针

**是什么：**
自动管理内存，不需要手动 `delete`，避免内存泄漏。

**学习要点：**
- `std::shared_ptr<T>` —— 共享所有权，引用计数为0时自动释放
  - `std::make_shared<T>(args...)` —— 推荐创建方式
  - `.get()` 获取原始指针，`.use_count()` 查看引用数
- `std::unique_ptr<T>` —— 独占所有权，更轻量，不可复制只可移动
- `std::weak_ptr<T>` —— 弱引用，不增加引用计数，避免循环引用

**项目中大量出现：**
`std::shared_ptr<CameraInfo>`, `std::shared_ptr<Buffer>`, `std::shared_ptr<Queue<RawImage>>`

---

### 2.2 std::thread 多线程

**是什么：**
C++ 标准库的线程，对 pthread 的封装，更安全易用。

**学习要点：**
- `std::thread t(函数, 参数...)` —— 创建线程
- `t.join()` —— 等待线程完成
- `t.detach()` —— 线程独立运行
- 传 lambda 或成员函数：`std::thread t([this]{ this->run(); });`

**项目中出现位置：**
`Camera.h` 中 `std::thread camera_thread_`, `std::thread package_manager_th_`

---

### 2.3 std::mutex 和锁

**是什么：**
互斥锁，保护共享数据，防止多线程同时访问造成数据损坏。

**学习要点：**
- `std::mutex mtx` —— 互斥锁
- `std::unique_lock<std::mutex> lock(mtx)` —— RAII 锁，自动释放
- `std::lock_guard<std::mutex> lock(mtx)` —— 更简单的 RAII 锁
- `std::shared_mutex` —— 读写锁：多个读者可以同时持有，写者独占
  - `std::shared_lock<std::shared_mutex>` —— 读锁（允许并发）
  - `std::unique_lock<std::shared_mutex>` —— 写锁（独占）

**项目中出现位置：**
`Camera.h` 中 `std::mutex camera_info_mutex_`, `std::shared_mutex cur_package_dir_mt_`
`Queue.h` 中 `std::mutex data_mutex_` 保护队列

---

### 2.4 std::atomic 原子操作

**是什么：**
原子变量的读写是不可中断的，不需要加锁就能保证线程安全。
类比 STM32：类似关中断读写一个变量，但不需要真正关中断。

**学习要点：**
- `std::atomic<bool> flag = false;`
- `std::atomic<int> count = 0;`
- `flag.load()` —— 原子读取
- `flag.store(true)` —— 原子写入
- `count.fetch_add(1)` —— 原子加法
- `std::atomic_bool` 是 `std::atomic<bool>` 的别名

**项目中出现位置：**
`Camera.h` 中 `std::atomic_bool is_exit_`（控制线程退出）
`CameraInfo.h` 中 `std::atomic<unsigned long long> frame_seq_`（帧序号）

---

### 2.5 std::condition_variable 条件变量

**是什么：**
线程等待某个条件成立，另一个线程通知它，实现生产者-消费者模式。

**学习要点：**
- `cv.wait(lock)` —— 释放锁并阻塞，被唤醒后重新加锁
- `cv.wait_for(lock, duration)` —— 带超时的等待
- `cv.notify_one()` —— 唤醒一个等待线程
- `cv.notify_all()` —— 唤醒所有等待线程
- 必须和 `std::unique_lock` 配合使用

**项目中出现位置：**
`Queue.h` 中 `std::condition_variable data_condition_` —— push 时 notify_one，pop 时 wait。

---

### 2.6 std::chrono 时间库

**是什么：**
C++ 时间处理，精度从秒到纳秒，跨平台。
类比 STM32：类似 `HAL_GetTick()` 但精度更高、接口更规范。

**学习要点：**
- `std::chrono::steady_clock::now()` —— 获取单调时间点（不受系统时间调整影响）
- `std::chrono::system_clock::now()` —— 获取系统时间
- `std::chrono::milliseconds(100)` —— 100毫秒
- `std::chrono::nanoseconds` —— 纳秒
- 计算时间差：`auto diff = t2 - t1;`
- 转换为数值：`diff.count()` 返回纳秒数

**项目中出现位置：**
`TimeTracker.h`、`CameraInfo.h` 中的 `steady_clock::time_point`

---

### 2.7 Lambda 表达式

**是什么：**
匿名函数，可以捕获外部变量。

**学习要点：**
- 基本格式：`[捕获列表](参数列表) { 函数体 }`
- `[this]` —— 捕获当前对象指针
- `[=]` —— 值捕获所有外部变量
- `[&]` —— 引用捕获所有外部变量
- 常用于传递给 `std::thread`、回调函数

**示例（项目中常见模式）：**
```cpp
std::thread t([this] { this->cameraThread(cameras); });
```

---

### 2.8 模板（Template）

**是什么：**
让类或函数支持任意类型，编译时实例化。

**学习要点：**
- `template <typename T> class Queue { ... }` —— 类模板
- `Queue<RawImage>` —— 实例化成 RawImage 队列
- `Queue<int>` —— 实例化成 int 队列
- 模板代码通常放在头文件（.h）中，不能分离到 .cpp

**项目中出现位置：**
`Queue.h` 中的 `template <typename T> class Queue`

---

### 2.9 std::filesystem（C++17）

**是什么：**
跨平台文件系统操作库。

**学习要点：**
- `std::filesystem::create_directories(path)` —— 递归创建目录
- `std::filesystem::exists(path)` —— 检查路径是否存在
- `std::filesystem::remove(path)` —— 删除文件
- `std::filesystem::path` —— 路径类型，支持 `/` 拼接操作

**项目中出现位置：**
`Camera.cpp` 中管理存储目录时使用。

---

### 2.10 enum class（强类型枚举）

**是什么：**
比 C 语言的 `enum` 更安全，不会与 int 隐式转换。

**学习要点：**
- `enum class CameraPosition : int { Unknown = 0, CenterFront, ... };`
- 使用时必须加类名：`CameraPosition::CenterFront`
- 不同枚举值不会相互污染命名空间

**项目中出现位置：**
`Common.h` 中的 `CameraPosition` 和 `CameraStatus`

---

## 模块三：V4L2（Video for Linux 2）—— 项目核心

V4L2 是 Linux 摄像头驱动框架，本项目用它来读取 GMSL 相机数据。

### 3.1 V4L2 基本概念

**是什么：**
Linux 内核提供的视频设备统一接口。摄像头、视频采集卡都通过 `/dev/videoX` 暴露出来。

**摄像头操作完整流程（按顺序学习）：**

**① 打开设备**
```c
int fd = open("/dev/video0", O_RDWR | O_NONBLOCK);
```

**② 查询设备能力**
```c
struct v4l2_capability cap;
ioctl(fd, VIDIOC_QUERYCAP, &cap);
// 检查 cap.capabilities & V4L2_CAP_VIDEO_CAPTURE
```

**③ 设置图像格式**
```c
struct v4l2_format fmt = {0};
fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
fmt.fmt.pix.width = 1920;
fmt.fmt.pix.height = 1080;
fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;  // 像素格式
ioctl(fd, VIDIOC_S_FMT, &fmt);
```

**④ 申请缓冲区**
```c
struct v4l2_requestbuffers req = {0};
req.count = 4;    // 申请4个缓冲区（项目中 mBufferNumber = 4）
req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
req.memory = V4L2_MEMORY_MMAP;   // 使用内存映射
ioctl(fd, VIDIOC_REQBUFS, &req);
```

**⑤ 映射缓冲区到用户空间**
```c
for (int i = 0; i < 4; i++) {
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    ioctl(fd, VIDIOC_QUERYBUF, &buf);      // 查询缓冲区信息
    void* ptr = mmap(NULL, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    // 将该缓冲区入队，等待驱动填充数据
    ioctl(fd, VIDIOC_QBUF, &buf);
}
```

**⑥ 开启流**
```c
enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
ioctl(fd, VIDIOC_STREAMON, &type);
```

**⑦ 循环采集帧（核心循环）**
```c
while (running) {
    // select 等待数据（项目中等待多路摄像头）
    select(maxfd+1, &fds, NULL, NULL, &tv);

    // 出队：取出驱动填好数据的缓冲区
    struct v4l2_buffer buf;
    ioctl(fd, VIDIOC_DQBUF, &buf);

    // 处理数据：ptr[buf.index] 指向图像数据
    process_frame(buffers[buf.index].start, buf.bytesused);

    // 入队：处理完后还给驱动
    ioctl(fd, VIDIOC_QBUF, &buf);
}
```

**⑧ 关闭流**
```c
ioctl(fd, VIDIOC_STREAMOFF, &type);
```

**项目中出现位置：**
`Camera.cpp` 的 `openV4L2Device()` 和 `startCapture()` 和 `cameraThread()` 函数。

**学习资源关键词：** `V4L2 video capture tutorial`, `V4L2 API guide Linux kernel`

---

### 3.2 DMA Buffer（V4L2_MEMORY_DMABUF）

**是什么：**
比 mmap 更高效的零拷贝机制。摄像头数据直接写入可被 GPU 访问的 DMA 内存，不需要任何拷贝。

**学习要点：**
- `V4L2_MEMORY_DMABUF` —— 使用 DMA buffer 内存类型
- DMA buffer 用文件描述符（`dma_fd`）标识
- GPU（CUDA）可以直接读取 DMA buffer
- `NvBufSurface` —— NVIDIA Jetson 的 DMA buffer 管理 API

**项目中出现位置：**
`Common.h` 中 `Buffer.dma_fd`，`RawImage.h` 中 `dma_image_data_`，`Camera.cpp` 大量操作 dma_fd。

---

## 模块四：图像颜色格式与转换

理解为什么要做格式转换，以及转换的具体过程。

### 4.1 UYVY 格式（YUV 4:2:2 打包格式）

**是什么：**
摄像头直接输出的原始格式，比 RGB 数据量小一半。

**数据排列：**
每4字节表示2个像素：`U0 Y0 V0 Y1`
- Y：亮度（灰度信息）
- U/V（Cb/Cr）：色度（颜色信息）
- 人眼对亮度敏感，对颜色不敏感，所以 UV 共享

**大小：**  1920×1080 的 UYVY = 1920×1080×2 = 4,147,200 字节 ≈ 4MB

**学习要点：**
- `V4L2_PIX_FMT_UYVY` —— V4L2 中的像素格式定义
- `AV_PIX_FMT_UYVY422` —— FFmpeg 中的对应格式
- 字节序：`[U][Y0][V][Y1]` 逐字节排列

---

### 4.2 BGR / RGB 格式

**是什么：**
每个像素3字节，分别存蓝绿红（BGR 是 OpenCV 的默认顺序，RGB 是通常理解的顺序）。

**大小：**  1920×1080×3 = 6,220,800 字节 ≈ 6MB（比 UYVY 大）

**为什么需要转换：**
AI 算法、OpenCV、显示都需要 RGB/BGR 格式，摄像头只输出 UYVY。

---

### 4.3 I420 / NV12 格式（YUV 4:2:0 平面格式）

**是什么：**
视频编码（H.264/H.265）的输入格式，比 UYVY 数据量更小。

**I420 数据排列：**
- Y 平面：全部 1920×1080 个 Y 值
- U 平面：1/4 个 U 值（960×540）
- V 平面：1/4 个 V 值（960×540）
- 总大小：1920×1080×1.5 = 3MB

**NV12：**
- Y 平面 + UV 交叉排列平面（U和V交替放）
- NVIDIA Jetson 硬件编码器首选输入格式

---

### 4.4 格式转换链（本项目的数据流）

```
摄像头输出（UYVY）
    ↓ CUDA 转换（UYVYTransform.cu）
    ├──→ BGR（给 RGB 发布 / OpenCV）
    └──→ I420（给 H.264/H.265 编码器）
              ↓ Jetson 硬件编码
              H.264 / H.265 码流（给远程传输/存储）
```

---

## 模块五：CUDA 编程基础

本项目用 GPU 做图像格式转换，使用的是 NVIDIA CUDA。

### 5.1 CPU 与 GPU 的关系

**基本概念：**
- CPU（主机，Host）：核少，擅长串行逻辑
- GPU（设备，Device）：核多（几千个），擅长并行数据处理
- 图像转换（每像素独立计算）非常适合 GPU 并行

### 5.2 CUDA 内存类型

**学习要点：**
- 主机内存（Host Memory）：CPU 可访问，`malloc` / `new` 分配
- 设备内存（Device Memory）：GPU 上的内存，`cudaMalloc` 分配
- 统一内存（Unified Memory）：CPU 和 GPU 都可访问
- `cudaMemcpy()` —— 在 CPU/GPU 之间传递数据

**项目中：**
`UYVYTransform.cuh` 中 `CUdeviceptr`（GPU 设备指针），`void* bgr_dst`（CPU 指针）

### 5.3 CUDA Kernel（核函数）

**是什么：**
在 GPU 上并行运行的函数，用 `__global__` 标记。

**基本格式：**
```cuda
__global__ void convertKernel(uint8_t* src, uint8_t* dst, int width, int height) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;  // 像素的 x 坐标
    int y = blockIdx.y * blockDim.y + threadIdx.y;  // 像素的 y 坐标
    // 每个 GPU 线程处理一个像素
}

// 调用方式：
convertKernel<<<grid, block, 0, stream>>>(src, dst, width, height);
```

### 5.4 CUDA Stream

**是什么：**
CUDA 的异步执行队列，类似 DMA 传输——提交后 CPU 继续干其他事，GPU 异步执行。

**项目中出现位置：**
`UYVYTransform.cuh` 中函数参数 `cudaStream_t stream`

### 5.5 EGL 与 NvBufSurface

**是什么：**
EGL 是 GPU 渲染 API（OpenGL/CUDA）和本地窗口系统之间的接口层。
Jetson 上的 DMA buffer 可以转成 `EGLImageKHR`，供 CUDA 直接处理（零拷贝）。

**学习要点：**
- `EGLImageKHR` —— 对 DMA buffer 的 GPU 视图
- `NvBufSurface` —— NVIDIA Jetson 多平面图像 buffer
- `NvBufSurfTransform` —— Jetson VIC 硬件色彩空间转换
- `CUeglFrame` —— CUDA 对 EGL image 的访问结构

**项目中出现位置：**
`UYVYTransform.cuh` 中所有 `EGLImageKHR`, `CUeglFrame`, `CUgraphicsResource`

**学习资源关键词：** `CUDA EGL interop Jetson`, `NvBufSurface API`

---

## 模块六：H.264/H.265 视频编码

本项目将摄像头画面编码成视频流，用于存储和远程传输。

### 6.1 基本概念

**帧类型：**
- **I 帧（关键帧/IDR 帧）**：完整图像，可以独立解码，体积大
- **P 帧**：只存储与前一帧的差异，体积小
- **GOP（Group of Pictures）**：一组帧，以 I 帧开头

**配置参数（对应项目 config）：**
- `auto_video_gop_iframe = 50` —— 每50帧一个 I 帧
- `auto_video_gop_idr = 50` —— IDR 帧间隔

**码率控制模式：**
- `CBR`（Constant Bit Rate）：恒定码率，网络传输友好
- `VBR`（Variable Bit Rate）：可变码率，画质更好，存储用推荐
- `auto_video_bitrate = 4096` —— 码率 4096 Kbps = 4 Mbps

**编码格式对比：**
| 格式 | 压缩率 | CPU 解码复杂度 | 用途 |
|------|--------|----------------|------|
| H.264 | 中 | 低 | 远程传输（兼容性好）|
| H.265 | 高（同质量体积约小50%）| 中 | 本地存储 |

### 6.2 Jetson 硬件编码器（V4L2 Encoder）

**是什么：**
Jetson 有专门的硬件编码单元（NVENC），通过 V4L2 接口使用，不消耗 CPU。

**项目中出现位置：**
`jetpack/jet_enc/JetsonEnc.cpp` 和 `JetsonEnc.h`，使用 `context_t_enc` 结构控制编码器，`VIDIOC_STREAMON` 等命令控制。

---

## 模块七：ROS2 基础

ROS2 是本项目的通信框架，相机数据通过 ROS2 发布给其他模块。

### 7.1 核心概念

**Node（节点）：**
一个运行的程序单元。本项目的 `CameraNode` 就是一个 ROS2 节点。

**Topic（话题）：**
节点间的数据通道，类似消息队列。
- 发布者（Publisher）：向 topic 发数据
- 订阅者（Subscriber）：从 topic 收数据
- 本项目发布 `/surrounding_front/image_raw` 等图像话题

**Message（消息）：**
话题上传输的数据类型。
- `sensor_msgs::msg::Image` —— 原始图像
- `sensor_msgs::msg::CompressedImage` —— 压缩图像（JPEG/H265）
- `std_msgs::msg::Header` —— 时间戳和坐标系信息

**Service（服务）：**
请求-响应模式，类似函数调用。
项目中 `DataRecordSplit`、`DataRecordSwitch` 是用于控制录像的 ROS2 服务。

### 7.2 Lifecycle Node（生命周期节点）

**是什么：**
比普通 Node 多了状态机，可以被外部控制启动/停止。
`CameraNode` 继承自 `rclcpp_lifecycle::LifecycleNode`，有 `configure`, `activate`, `deactivate`, `shutdown` 等状态。

### 7.3 共享内存消息（shm_msgs）

**是什么：**
发布大图像时，避免数据序列化/反序列化的开销，直接通过共享内存传递。
`shm_msgs::msg::ShmImage6m`（6MB）, `ShmImage10m`（10MB）, `ShmImage24m`（24MB）对应不同分辨率。

### 7.4 rosbag2

**是什么：**
录制和回放 ROS2 话题数据的工具。
项目用 `rosbag2_cpp::Writer` 将图像数据写入 bag 文件。

**学习资源关键词：** `ROS2 rclcpp tutorial`, `ROS2 publisher subscriber C++`

---

## 模块八：CMake 构建系统

本项目用 CMake 管理编译，替代 STM32 中 Keil/IAR 的工程文件。

### 8.1 关键概念

**学习要点：**
- `cmake_minimum_required(VERSION 3.8)` —— 最低版本要求
- `project(camera LANGUAGES CXX CUDA)` —— 定义工程名和语言
- `find_package(rclcpp REQUIRED)` —— 查找依赖库（类似 Keil 添加库）
- `add_executable(camera_node src/Main.cpp ...)` —— 定义可执行文件和源文件
- `target_link_libraries(camera_node JetsonEnc ...)` —— 链接库
- `target_include_directories(...)` —— 添加头文件搜索路径
- `ament_cmake` —— ROS2 专用的 CMake 扩展

### 8.2 编译流程

```bash
# 在 ROS2 工作空间中
colcon build --packages-select camera   # 编译
source install/setup.bash               # 加载环境变量
ros2 run camera camera_node             # 运行节点
```

---

## 模块九：配置文件格式

### 9.1 TOML 格式

本项目所有配置文件（`config/*.toml`）使用 TOML 格式。

**基本语法：**
```toml
# 注释
key = "value"            # 字符串
count = 4096             # 整数
flag = true              # 布尔
ratio = 0.5              # 浮点

[[camera_info]]          # 数组表（可重复，对应一个摄像头）
position = "center_front"
device = "/dev/Video0"
```

代码中用 `tomlplusplus` 库（`3rd/include/tomlplusplus/toml.hpp`）解析配置。

### 9.2 JSON 格式

`config/logger.json` 是日志配置，用 `nlohmann/json.hpp` 解析。

---

## 模块十：spdlog 日志库

本项目用 `spdlog` 做日志，替代 `printf`。

**学习要点：**
- 日志级别：`trace < debug < info < warn < error < critical`
- `SPDLOG_INFO("格式 {}", 变量)` —— 打印 info 级别日志（`{}` 是占位符）
- `spdlog::get("logger_name")` —— 获取命名日志器
- 异步日志：`spdlog::async_logger` —— 不阻塞主线程写日志
- 文件日志：`spdlog::basic_logger_mt("name", "file.log")` —— 写入文件

---

## 模块十一：时间戳与时钟同步（进阶）

自动驾驶中，多传感器时间同步至关重要。

### 11.1 时钟类型

- `CLOCK_MONOTONIC`（单调时钟）：从系统启动开始计时，不受 NTP 调整影响，适合计时
- `CLOCK_REALTIME`（实时时钟）：墙钟时间，可被调整，适合记录绝对时间
- `CLOCK_TAI`：国际原子时，无闰秒

### 11.2 rtcpu 时间戳

**项目特有：**
GMSL 摄像头的 V4L2 buffer 中携带 rtcpu 时间戳（Jetson 实时 CPU 的时间）。
`Camera.cpp` 中 `rtcpuToRealtime()` 将 rtcpu 时间转换为系统实时时间。
`readOffsetNs()` 读取 `/proc` 下的时间偏移量文件。

### 11.3 timeval / timespec

```c
struct timeval  { long tv_sec;  long tv_usec; };   // 微秒精度
struct timespec { long tv_sec; long tv_nsec; };    // 纳秒精度
```

---

## 推荐学习顺序

| 阶段 | 模块 | 预计时间 | 优先级 |
|------|------|----------|--------|
| 第一阶段 | 模块一（Linux 系统编程）| 1-2 周 | ★★★★★ |
| 第一阶段 | 模块二（现代 C++）| 1-2 周 | ★★★★★ |
| 第二阶段 | 模块三（V4L2）| 1 周 | ★★★★★ |
| 第二阶段 | 模块四（图像格式）| 3 天 | ★★★★☆ |
| 第三阶段 | 模块七（ROS2）| 1 周 | ★★★★☆ |
| 第三阶段 | 模块八（CMake）| 2 天 | ★★★☆☆ |
| 第四阶段 | 模块五（CUDA）| 1-2 周 | ★★★☆☆ |
| 第四阶段 | 模块六（视频编码）| 3 天 | ★★★☆☆ |
| 第五阶段 | 模块九~十一（配置/日志/时钟）| 2 天 | ★★☆☆☆ |

---

## 推荐学习资源

| 知识点 | 推荐资源 |
|--------|----------|
| Linux 系统编程 | 《Linux 系统编程》Robert Love 著 |
| V4L2 | Linux 内核文档：`docs.kernel.org/userspace-api/media/v4l` |
| 现代 C++ | cppreference.com（中文版可搜"cppreference 中文"）|
| ROS2 | 官方教程：`docs.ros.org/en/humble/Tutorials.html` |
| CUDA | NVIDIA 官方教程：`docs.nvidia.com/cuda/cuda-c-programming-guide` |
| CMake | `cmake.org/cmake/help/latest/guide/tutorial` |
| spdlog | `github.com/gabime/spdlog`（README 即文档）|

---

## 与 STM32 的对比速查

| STM32 概念 | Linux/本项目对应 |
|-----------|----------------|
| HAL_GPIO_WritePin | ioctl + 设备文件 |
| HAL_GetTick() | clock_gettime(CLOCK_MONOTONIC) |
| 关中断保护临界区 | std::mutex / std::atomic |
| DMA 传输 | mmap + V4L2_MEMORY_DMABUF |
| UART 中断回调 | std::condition_variable notify |
| 轮询等待标志位 | select / poll |
| 全局变量 | std::atomic（线程安全全局状态）|
| FreeRTOS 任务 | std::thread |
| FreeRTOS 队列 | Queue<T>（本项目实现）|
| Keil/IAR 工程文件 | CMakeLists.txt |
| printf 调试 | spdlog 日志库 |
