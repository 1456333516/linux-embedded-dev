#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <cstring>

struct CameraBuffer {
    void *start;
    size_t length;
};

int main() {
    int fd = open("/dev/video11", O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("打开设备失败");
        return -1;
    }

    // 1. 设置格式 (必须使用 _MPLANE 宏)
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; // ★ 核心改变 1
    fmt.fmt.pix_mp.width = 1920;                   // 从日志得知宽
    fmt.fmt.pix_mp.height = 1080;                  // 从日志得知高
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_UYVY;// 从日志得知格式 UYVY

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("设置格式失败");
        close(fd);
        return -1;
    }

    // 2. 申请缓冲区 (REQBUFS)
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; // ★ 核心改变 2
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("申请缓冲区失败"); // 以前在这里报错，现在不会了
        close(fd);
        return -1;
    }

    CameraBuffer* buffers = new CameraBuffer[req.count];

    // 3. 查询缓冲区并建立 mmap 映射
    for (int i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        // ★ 核心改变 3: 必须定义 planes 数组并关联给 buf
        struct v4l2_plane planes[1]; 
        
        memset(&buf, 0, sizeof(buf));
        memset(&planes, 0, sizeof(planes));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; // ★ 核心改变 4
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.m.planes = planes;  // 将 planes 数组指针赋给 buf
        buf.length = 1;         // 日志显示 Number of planes: 1

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("查询缓冲区失败");
            continue;
        }

        // ★ 核心改变 5: length 和 offset 都在 planes[0] 里面！
        buffers[i].length = buf.m.planes[0].length;
        buffers[i].start = mmap(
            NULL, 
            buf.m.planes[0].length, 
            PROT_READ | PROT_WRITE, 
            MAP_SHARED, 
            fd, 
            buf.m.planes[0].m.mem_offset // 获取多平面的物理偏移地址
        );

        if (buffers[i].start == MAP_FAILED) {
            perror("mmap 失败");
        } else {
            std::cout << "缓冲区 " << i << " 映射成功, 大小: " 
                      << buffers[i].length << " 字节" << std::endl;
        }
        
        // 入队
        ioctl(fd, VIDIOC_QBUF, &buf); 
    }

    // 资源释放略...
    return 0;
}
