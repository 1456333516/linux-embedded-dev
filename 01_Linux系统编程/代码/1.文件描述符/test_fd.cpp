#include <iostream>
#include <fcntl.h>   // 包含 open
#include <unistd.h>  // 包含 read, write, close
#include <cerrno>    // 包含 errno
#include <cstring>   // 包含 strerror

int main() {
    // 1. 尝试打开一个不存在的设备，测试报错机制
    int bad_fd = open("/dev/video99", O_RDWR);
    if (bad_fd == -1) {
        std::cerr << "预期的失败：无法打开 /dev/video99, 错误原因: " << strerror(errno) << std::endl;
    }

    // 2. 创建并打开一个普通文件测试
    int fd = open("test_camera_log.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);//数字前面加 0，代表这是一个“八进制（Octal）”数。
    if (fd == -1) {
        std::cerr << "创建文件失败!" << std::endl;
        return -1;
    }

    std::cout << "成功打开文件，分配到的文件描述符 fd = " << fd << std::endl;

    // 3. 写入数据
    const char* msg = "模拟摄像头初始化完成...\n";
    write(fd, msg, strlen(msg));
    
    // 4. 标准输出也是 fd! 我们向 1 号 fd 写点东西
    const char* screen_msg = "这是直接往 fd=1 (屏幕) 写入的文本。\n";
    write(1, screen_msg, strlen(screen_msg));

    // 5. 关闭资源
    close(fd);
    std::cout << "资源已释放。" << std::endl;

    return 0;
}
// 编译命令: g++ test_fd.cpp -o test_fd
// 运行命令: ./test_fd