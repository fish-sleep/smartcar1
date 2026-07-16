/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2025-01-07 06:55:37
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2025-01-07 08:10:54
 * @FilePath: /smartcar/opencv_demo1/opencv_demo1.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <opencv2/opencv.hpp>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <chrono>
#include <iomanip>
#include <sstream>

using namespace cv;
using namespace std;

// 将 RGB888 转换为 RGB565
ushort rgb888_to_rgb565(const Vec3b &color)
{
    return ((color[2] >> 3) << 11) | ((color[1] >> 2) << 5) | (color[0] >> 3);
}

// 获取当前时间字符串
string getCurrentTime()
{
    auto now = chrono::system_clock::now();
    auto now_time_t = chrono::system_clock::to_time_t(now);
    auto now_tm = *localtime(&now_time_t);

    stringstream ss;
    ss << put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

int main()
{
    // 打开 USB 摄像头
    VideoCapture cap(0); // 0 表示默认摄像头
    if (!cap.isOpened())
    {
        cerr << "Error: Cannot open camera" << endl;
        return -1;
    }

    // 设置摄像头分辨率（可选）
    cap.set(CAP_PROP_FRAME_WIDTH, 320);
    cap.set(CAP_PROP_FRAME_HEIGHT, 240);

    // Framebuffer 设备
    const char *fb_device = "/dev/fb0";
    int fb_fd = open(fb_device, O_RDWR);
    if (fb_fd == -1)
    {
        cerr << "Error: Cannot open framebuffer device" << endl;
        return -1;
    }

    // 获取 Framebuffer 信息
    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo))
    {
        cerr << "Error: Cannot get framebuffer information" << endl;
        close(fb_fd);
        return -1;
    }

    // 检查 Framebuffer 是否支持 RGB565
    if (vinfo.bits_per_pixel != 16)
    {
        cerr << "Error: Framebuffer is not RGB565 format" << endl;
        close(fb_fd);
        return -1;
    }

    // 映射 Framebuffer 到内存
    size_t fb_size = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    ushort *fb_data = (ushort *)mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_data == MAP_FAILED)
    {
        cerr << "Error: Failed to map framebuffer to memory" << endl;
        close(fb_fd);
        return -1;
    }

    // 主循环
    Mat frame, resizedFrame;
    int frameCount = 0;
    while (true)
    {
        // 从摄像头捕获一帧图像
        cap >> frame;
        if (frame.empty())
        {
            cerr << "Error: Captured frame is empty" << endl;
            break;
        }

        // 调整图像大小为 160x128
        resize(frame, resizedFrame, Size(160, 128));

        // 获取当前时间和帧数
        string timeStr = getCurrentTime();
        string frameStr = "Frame: " + to_string(frameCount);

        // 在图像左上角绘制黑色边框的白色文字
        int fontFace = FONT_HERSHEY_SIMPLEX;
        double fontScale = 0.4;
        int thickness = 1;
        int baseline = 0;

        // 计算文字位置
        Point timePos(5, 15);  // 时间文字位置
        Point framePos(5, 30); // 帧数文字位置

        // 绘制黑色边框
        putText(resizedFrame, timeStr, timePos + Point(-1, -1), fontFace, fontScale, Scalar(0, 0, 0), thickness + 1);
        putText(resizedFrame, timeStr, timePos + Point(1, -1), fontFace, fontScale, Scalar(0, 0, 0), thickness + 1);
        putText(resizedFrame, timeStr, timePos + Point(-1, 1), fontFace, fontScale, Scalar(0, 0, 0), thickness + 1);
        putText(resizedFrame, timeStr, timePos + Point(1, 1), fontFace, fontScale, Scalar(0, 0, 0), thickness + 1);

        putText(resizedFrame, frameStr, framePos + Point(-1, -1), fontFace, fontScale, Scalar(0, 0, 0), thickness + 1);
        putText(resizedFrame, frameStr, framePos + Point(1, -1), fontFace, fontScale, Scalar(0, 0, 0), thickness + 1);
        putText(resizedFrame, frameStr, framePos + Point(-1, 1), fontFace, fontScale, Scalar(0, 0, 0), thickness + 1);
        putText(resizedFrame, frameStr, framePos + Point(1, 1), fontFace, fontScale, Scalar(0, 0, 0), thickness + 1);

        // 绘制白色文字
        putText(resizedFrame, timeStr, timePos, fontFace, fontScale, Scalar(255, 255, 255), thickness);
        putText(resizedFrame, frameStr, framePos, fontFace, fontScale, Scalar(255, 255, 255), thickness);

        // 将图像数据写入 Framebuffer
        for (int y = 0; y < resizedFrame.rows; y++)
        {
            for (int x = 0; x < resizedFrame.cols; x++)
            {
                Vec3b color = resizedFrame.at<Vec3b>(y, x);
                fb_data[y * vinfo.xres + x] = rgb888_to_rgb565(color);
            }
        }

        // 更新帧数
        frameCount++;

        // 等待一段时间（例如 30ms）
        usleep(30000);
    }

    // 释放资源
    munmap(fb_data, fb_size);
    close(fb_fd);
    cap.release();

    return 0;
}