#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

using namespace cv;
using namespace std;
using namespace chrono;

// 将RGB转换为RGB565格式
uint16_t convertRGBToRGB565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// 将Mat图像转换为RGB565格式
void convertMatToRGB565(const Mat &frame, uint16_t *buffer, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            Vec3b color = frame.at<Vec3b>(y, x);
            buffer[y * width + x] = convertRGBToRGB565(color[2], color[1], color[0]);
        }
    }
}

int main(int argc, char **argv) {
    // 打开图像文件
    Mat img = imread("demo.png");
    if (img.empty()) {
        cerr << "无法打开图像文件" << endl;
        return -1;
    }

    // 获取帧缓冲区设备信息
    int fb = open("/dev/fb1", O_RDWR);
    if (fb == -1) {
        cerr << "无法打开帧缓冲区设备" << endl;
        return -1;
    }

    struct fb_var_screeninfo vinfo;
    ioctl(fb, FBIOGET_VSCREENINFO, &vinfo);

    // 设置屏幕参数
    int screenWidth = 160;
    int screenHeight = 128;

    // 创建帧缓冲区
    uint16_t *fb_buffer = new uint16_t[screenWidth * screenHeight];

    Mat frame, resizedFrame, fbImage(screenHeight, screenWidth, CV_8UC3, Scalar(0, 0, 0)); // 帧缓冲区图像

    // 获取图像的宽高比
    double imgAspectRatio = static_cast<double>(img.cols) / img.rows;
    double screenAspectRatio = static_cast<double>(screenWidth) / screenHeight;

    // 判断是否需要旋转图像（即使长宽比更适合竖屏）
    bool rotateImage = false;
    if (imgAspectRatio > 1 && screenAspectRatio < 1) {
        rotateImage = true; // 图像是横向的，而屏幕是竖向的
    } else if (imgAspectRatio < 1 && screenAspectRatio > 1) {
        rotateImage = true; // 图像是竖向的，而屏幕是横向的
    }

    // 如果需要旋转图像，进行90度旋转
    if (rotateImage) {
        transpose(img, img); // 先进行矩阵转置
        flip(img, img, 1);   // 再进行水平翻转，完成顺时针90度旋转
    }

    // 获取旋转后的图像宽高比
    int imgWidth = img.cols;
    int imgHeight = img.rows;
    double newAspectRatio = static_cast<double>(imgWidth) / imgHeight;

    // 根据新的长宽比调整缩放
    int newWidth, newHeight;
    if (screenWidth / static_cast<double>(screenHeight) > newAspectRatio) {
        // 屏幕更宽，以高度为基准
        newHeight = screenHeight;
        newWidth = static_cast<int>(newHeight * newAspectRatio);
    } else {
        // 屏幕更高，以宽度为基准
        newWidth = screenWidth;
        newHeight = static_cast<int>(newWidth / newAspectRatio);
    }

    // 缩放图像到新尺寸
    resize(img, resizedFrame, Size(newWidth, newHeight));

    // 将缩放后的图像居中放置在帧缓冲区图像中，填充黑色边框
    fbImage.setTo(Scalar(0, 0, 0)); // 清空缓冲区（填充黑色）
    Rect roi((screenWidth - newWidth) / 2, (screenHeight - newHeight) / 2, newWidth, newHeight);
    resizedFrame.copyTo(fbImage(roi));

    // 将帧缓冲区图像转换为RGB565格式
    convertMatToRGB565(fbImage, fb_buffer, screenWidth, screenHeight);

    // 写入帧缓冲区
    lseek(fb, 0, SEEK_SET);
    write(fb, fb_buffer, screenWidth * screenHeight * 2);

    // 释放资源
    delete[] fb_buffer;
    close(fb);

    return 0;
}
