#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include "PwmController.h"

using namespace cv;
using namespace std;
using namespace chrono;

PwmController Mortor1(0, 0), Mortor2(1, 0), Servo(2, 0), Buzzer(3, 0);
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
 
void pwminit();

int main() {
    // 打开默认摄像头（设备编号 0）
    cv::VideoCapture cap(0);

    // 检查摄像头是否成功打开
    if (!cap.isOpened()) {
        printf("无法打开摄像头\n");
        return -1;
    }

    cap.set(cv::CAP_PROP_AUTO_EXPOSURE, -1);    //设置自动曝光
    double fps = cap.get(cv::CAP_PROP_FPS);
    printf("Camera fps:%lf\n", fps);

    // 计算每帧的延迟时间（毫秒）
    double frame_delay = static_cast<double>(1000000 / fps);

    pwminit();
    
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

    int frameCount = 0;
    auto startTime = steady_clock::now();

    Mat frame, resizedFrame, fbImage(screenHeight, screenWidth, CV_8UC3, Scalar(0, 0, 0)); // 帧缓冲区图像

    int frame_count = 0;
    auto frame_start_time = high_resolution_clock::now();
    auto frame_end_time = high_resolution_clock::now();
    auto frame_end_time1 = high_resolution_clock::now();

    Mortor1.disable();
    Mortor2.disable();
    Servo.disable();
    Buzzer.disable();

    cap.read(frame);
    // 保持视频长宽比
    int videoWidth = frame.cols;
    int videoHeight = frame.rows;
    double aspectRatio = static_cast<double>(videoWidth) / videoHeight;
    
    // 根据屏幕大小计算缩放后的宽度和高度
    int newWidth, newHeight;
    if (screenWidth / static_cast<double>(screenHeight) > aspectRatio) {
        // 屏幕更宽，以高度为基准
        newHeight = screenHeight;
        newWidth = static_cast<int>(newHeight * aspectRatio);
    } else {
        // 屏幕更高，以宽度为基准
        newWidth = screenWidth;
        newHeight = static_cast<int>(newWidth / aspectRatio);
    }

    // 将缩放后的图像居中放置在帧缓冲区图像中，填充黑色边框
    fbImage.setTo(Scalar(0, 0, 0)); // 清空缓冲区（填充黑色）
    Rect roi((screenWidth - newWidth) / 2, (screenHeight - newHeight) / 2, newWidth, newHeight);

    while(true) {
        cap.read(frame);
        // 检查是否成功捕获图像
        if (frame.empty()) {
            printf("无法捕获图像\n");
            return -1;
        }
        /*
        //保存图像到文件
        const char* filename = "captured_image.jpg";
        if (cv::imwrite(filename, frame)) {
            printf("图像已保存: %s\n", filename);
        } else {
            printf("图像保存失败\n");
        }
        */
        // 计算帧率
        frameCount++;

        // 缩放视频到新尺寸
        resize(frame, resizedFrame, Size(newWidth, newHeight));

        // 将缩放后的图像居中放置在帧缓冲区图像中，填充黑色边框
        // fbImage.setTo(Scalar(0, 0, 0)); // 清空缓冲区（填充黑色）
        // Rect roi((screenWidth - newWidth) / 2, (screenHeight - newHeight) / 2, newWidth, newHeight);
        resizedFrame.copyTo(fbImage(roi));

        // 计算帧处理时间
        std::chrono::duration<double> frame_duration = frame_end_time - frame_start_time;
        double frame_rate = 1.0 / frame_duration.count();

        // 将帧率和帧数显示在左上角
        char text[32];
        sprintf(text, "Frame: %d", frameCount);
        // 先绘制黑色边框
        putText(fbImage, text, Point(5, 15), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 0, 0), 3);
        // 再绘制白色文字
        putText(fbImage, text, Point(5, 15), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(255, 255, 255), 1);
        sprintf(text, "FPS: %.2f", frame_rate);
        // 先绘制黑色边框
        putText(fbImage, text, Point(5, 30), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 0, 0), 3);
        // 再绘制白色文字
        putText(fbImage, text, Point(5, 30), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(255, 255, 255), 1);

        // 将帧缓冲区图像转换为RGB565格式
        convertMatToRGB565(fbImage, fb_buffer, screenWidth, screenHeight);

        // 写入帧缓冲区
        lseek(fb, 0, SEEK_SET);
        write(fb, fb_buffer, screenWidth * screenHeight * 2);

        frame_end_time1 = std::chrono::high_resolution_clock::now();
        frame_duration = frame_end_time1 - frame_end_time;

        // 等待一段时间，以模拟视频帧率
        usleep(std::max(0.0, frame_delay - frame_duration.count() * 1000000));

        // 计算每帧的开始时间
        frame_start_time = frame_end_time;
        frame_end_time = high_resolution_clock::now();
    }
    return 0;
}

void pwminit() {
    Mortor1.setPeriod(50000);   //50us  20kHz
    Mortor2.setPeriod(50000);   //50us  20kHz
    Servo.setPeriod(5000000);   //5ms   200Hz
    Buzzer.setPeriod(250000);   //250us 4000Hz
    Mortor1.setDutyCycle(50000 - 0);    //0%
    Mortor2.setDutyCycle(50000 - 0);    //0%
    Servo.setDutyCycle(2500000);        //50%
    Buzzer.setPeriod(125000);           //50%
    Mortor1.enable();
    Mortor2.enable();
    Servo.enable();
    Buzzer.enable();
    usleep(1000000);
    Buzzer.disable();
}