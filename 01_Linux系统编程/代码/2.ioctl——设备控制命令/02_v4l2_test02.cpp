#include <iostream>
#include <cerrno>
#include <cstring>
#include <fcntl.h>      // 提供 open() 和 O_RDWR 宏
#include <unistd.h>     // 提供 close() 函数
#include <sys/ioctl.h>  // 提供 ioctl() 函数
#include <linux/videodev2.h> // 提供 V4L2 核心宏，如 VIDIOC_QUERYCAP

int main() {
    // 1. 打开摄像头设备节点 (通常 USB 摄像头是 /dev/video0)
    const char* dev_name = "/dev/video0";
    int fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
    
    if (fd == -1) {
        std::cerr << "无法打开设备 " << dev_name << ": " << strerror(errno) << std::endl;
        return -1;
    }

    // 2. 你的核心代码：查询设备能力
    struct v4l2_capability cap;
    int ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    
    if (ret == -1) {
        std::cerr << "查询摄像头能力失败: " << strerror(errno) << std::endl;
        close(fd);
        return -1;
    }

    // 3. 打印查询到的信息，验证是否成功
    std::cout << "===== 摄像头信息 =====" << std::endl;
    std::cout << "驱动名称: " << cap.driver << std::endl;
    std::cout << "设备名称: " << cap.card << std::endl;
    std::cout << "总线信息: " << cap.bus_info << std::endl;
    
    // 检查是否支持视频捕获
    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        std::cout << "状态: 该设备支持视频捕获 (Video Capture)！" << std::endl;
    } else {
        std::cerr << "警告: 该设备不支持视频捕获！" << std::endl;
    }

    // 4. 释放资源
    close(fd);
    return 0;
}

// g++ ./02_v4l2_test02.cpp  -o 02_v4l2_test02