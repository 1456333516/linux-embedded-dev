#include <iostream>
#include <vector>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <opencv2/opencv.hpp> // 记得在文件头部加上这个
#include <chrono> // 用于高精度计时

// 用于保存内存映射信息的结构体
struct CameraBuffer {
    void* start;
    size_t length;
};

int main() {
    const char* dev_name = "/dev/video11";
    
    // 1. 打开设备
    int fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
    if (fd == -1) {
        std::cerr << "无法打开设备 " << dev_name << ": " << strerror(errno) << std::endl;
        return -1;
    }

    // 2. 查询设备能力 (强制检查 MPLANE)
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        std::cerr << "查询能力失败: " << strerror(errno) << std::endl;
        close(fd);
        return -1;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
        std::cerr << "该设备不支持多平面(MPLANE)视频捕获！" << std::endl;
        close(fd);
        return -1;
    }

    // 3. 设置多平面图像格式 (匹配拓扑图里的 1920x1080 UYVY)
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = 1920;
    fmt.fmt.pix_mp.height = 1080;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_UYVY;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        std::cerr << "设置格式失败: " << strerror(errno) << std::endl;
        close(fd);
        return -1;
    }

    // 4. 申请内核缓冲区 (通常申请 4 个)
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; // 注意 MPLANE
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        std::cerr << "申请缓冲区失败: " << strerror(errno) << std::endl;
        close(fd);
        return -1;
    }

    // 5. 内存映射 (mmap) —— 将内核内存映射到用户空间
    std::vector<CameraBuffer> buffers(req.count);
    for (unsigned int i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[1]; // UYVY 虽然是 MPLANE API，但通常只占用 1 个数据平面
        
        memset(&buf, 0, sizeof(buf));
        memset(&planes, 0, sizeof(planes));
        
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        // 核心坑点：必须把 planes 数组的指针赋给 buf.m.planes，并指定平面数量
        buf.m.planes = planes;
        buf.length = 1; 

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            std::cerr << "查询缓冲区 " << i << " 失败: " << strerror(errno) << std::endl;
            close(fd);
            return -1;
        }

        buffers[i].length = buf.m.planes[0].length;
        buffers[i].start = mmap(NULL, buf.m.planes[0].length,
                                PROT_READ | PROT_WRITE, MAP_SHARED,
                                fd, buf.m.planes[0].m.mem_offset);

        if (buffers[i].start == MAP_FAILED) {
            std::cerr << "内存映射失败" << std::endl;
            close(fd);
            return -1;
        }

        // 映射完立刻放入采集队列
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            std::cerr << "缓冲区入队失败" << std::endl;
            return -1;
        }
    }

    // 6. 开启视频流
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        std::cerr << "开启视频流失败: " << strerror(errno) << std::endl;
        return -1;
    }
    std::cout << "视频流已成功开启！开始抓取图像..." << std::endl;

    // 7. 抓取一帧图像 (DQBUF)
    struct v4l2_buffer buf;
    struct v4l2_plane planes[1];
    memset(&buf, 0, sizeof(buf));
    memset(&planes, 0, sizeof(planes));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.m.planes = planes;
    buf.length = 1;

    // 为了防止非阻塞模式直接返回，这里做一个简单的轮询等待
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = {2, 0}; // 2秒超时
    int r = select(fd + 1, &fds, NULL, NULL, &tv);

    if (r == -1) {
        std::cerr << "等待图像数据出错" << std::endl;
    } else if (r == 0) {
        std::cerr << "等待图像数据超时" << std::endl;
    } else {
        // 出队，拿到包含图像数据的缓冲区
        if (ioctl(fd, VIDIOC_DQBUF, &buf) == 0) {
            std::cout << "成功抓取到一帧图像！" << std::endl;
            std::cout << "数据存储在内存地址: " << buffers[buf.index].start << std::endl;
            std::cout << "实际数据长度: " << buf.m.planes[0].bytesused << " 字节" << std::endl;
            
            // -----------------------------------------------------
            // 【数据处理区】
            // -----------------------------------------------------
            // 1. 直接用裸指针包装成 OpenCV 的 Mat 对象（零拷贝，速度极快）
            cv::Mat uyvy_mat(1080, 1920, CV_8UC2, buffers[buf.index].start);

            cv::Mat bgr_mat;

            // ===== 开始计时 =====
            auto start_time = std::chrono::high_resolution_clock::now();

            // 2. 将 UYVY 格式转换成标准的 BGR 格式
            cv::cvtColor(uyvy_mat, bgr_mat, cv::COLOR_YUV2BGR_UYVY);

            // ===== 结束计时 =====
            auto end_time = std::chrono::high_resolution_clock::now();
            
            // 计算耗时（毫秒）
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            std::cout << ">>> OpenCV 格式转换耗时: " << duration.count() << " 毫秒 <<<" << std::endl;

            // 3. 保存成图片看一眼实际画质！
            cv::imwrite("first_frame.jpg", bgr_mat);
            std::cout << "图像已成功转换为 BGR 并保存为 first_frame.jpg" << std::endl;

            // 处理完毕，将处理完的空缓冲区重新放回队列
            ioctl(fd, VIDIOC_QBUF, &buf);
        } else {
            std::cerr << "DQBUF 失败: " << strerror(errno) << std::endl;
        }
    } // 注意这里：这是对应最外层 if (r == -1) 的大括号结束

    // 8. 停止视频流并清理资源
    ioctl(fd, VIDIOC_STREAMOFF, &type);
    for (unsigned int i = 0; i < req.count; ++i) {
        munmap(buffers[i].start, buffers[i].length);
    }
    close(fd);
    std::cout << "资源已释放，程序安全退出。" << std::endl;

    return 0;
}
// g++ 01_v4l2_test.cpp -o 01_v4l2_test `pkg-config --cflags --libs opencv4`
// ./01_v4l2_test 