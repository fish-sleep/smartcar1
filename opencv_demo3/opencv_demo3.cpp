/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2025-01-07 06:26:01
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2025-01-07 08:10:45
 * @FilePath: /smartcar/opencv_demo2/opencv_demo2.cpp
 * @Description: 使用Haar级联分类器从USB摄像头获取图像，并将图像居中显示到160x128的RGB565 framebuffer设备
 */
#include <opencv2/opencv.hpp>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

using namespace cv;
using namespace std;

// 将 RGB888 转换为 RGB565
ushort rgb888_to_rgb565(const Vec3b &color)
{
    return ((color[2] >> 3) << 11) | ((color[1] >> 2) << 5) | (color[0] >> 3);
}

int main()
{
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

    // 打开USB摄像头
    VideoCapture cap(0); // 0表示第一个摄像头
    if (!cap.isOpened())
    {
        cerr << "Error: Cannot open camera" << endl;
        return -1;
    }

    // 设置摄像头参数
    cap.set(CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CAP_PROP_FRAME_HEIGHT, 360);
    cap.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(CAP_PROP_AUTO_EXPOSURE, -1); // 启用自动曝光

    // 加载Haar级联分类器
    CascadeClassifier face_cascade;
    if (!face_cascade.load("opencv/share/opencv4/haarcascades/haarcascade_frontalface_default.xml"))
    {
        cerr << "Error: Cannot load Haar cascade classifier" << endl;
        return -1;
    }

    // 创建 OpenCV 图像（160x128，RGB888）
    Mat image(128, 160, CV_8UC3, Scalar(0, 0, 0));

    while (true)
    {
        Mat frame;
        cap >> frame; // 从摄像头获取一帧图像
        if (frame.empty())
        {
            cerr << "Error: Cannot grab frame from camera" << endl;
            break;
        }

        // 缩放图像到160x90
        Mat resized_frame;
        resize(frame, resized_frame, Size(160, 90));

        // 转换为灰度图像
        Mat gray;
        cvtColor(resized_frame, gray, COLOR_BGR2GRAY);

        // 使用Haar级联分类器检测人脸
        vector<Rect> faces;
        face_cascade.detectMultiScale(gray, faces, 1.1, 2, 0 | CASCADE_SCALE_IMAGE, Size(30, 30));

        // 在图像上绘制检测到的人脸
        for (const auto &face : faces)
        {
            rectangle(resized_frame, face, Scalar(255, 0, 0), 2);
        }

        // 将图像居中显示到160x128的framebuffer
        image.setTo(Scalar(0, 0, 0)); // 清空图像
        int y_offset = (128 - 90) / 2;
        resized_frame.copyTo(image(Rect(0, y_offset, 160, 90)));

        // 将 OpenCV 图像（RGB888）转换为 RGB565 并写入 Framebuffer
        for (int y = 0; y < image.rows; y++)
        {
            for (int x = 0; x < image.cols; x++)
            {
                Vec3b color = image.at<Vec3b>(y, x);
                fb_data[y * vinfo.xres + x] = rgb888_to_rgb565(color);
            }
        }

        // 等待一段时间
        usleep(33333);
    }

    // 释放资源
    munmap(fb_data, fb_size);
    close(fb_fd);
    cap.release();

    return 0;
}