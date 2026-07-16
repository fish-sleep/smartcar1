/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2025-01-07 06:26:01
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2025-03-26 12:26:22
 * @FilePath: /smartcar/opencv_demo2/opencv_demo2.cpp
 * @Description:
 */
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <map>
#include <dirent.h>
#include <termios.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <sstream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

#include "image_cv.h"

using namespace std;

using ImageCallback = void (*)(const string &fullpath);

const int cameraWidth = 320, cameraHeight = 240;
int screenWidth, screenHeight;
const int calc_scale = 2;
int newWidth, newHeight;

ImageCallback image_callback = nullptr;
string base_path;

struct termios orig_termios;
map<int, string> file_map;
int current_num = -1;

const char *fb_device = "/dev/fb0";
int fb_fd;
ushort *fb_data;
struct fb_var_screeninfo vinfo;

std::chrono::_V2::system_clock::time_point frame_begin, frame_end;
std::chrono::duration<double> frame_duration;

int displayMode = 0;
const int displayModeCount = 7;

void imgInit()
{
    // 动态设置屏幕分辨率
    screenWidth = vinfo.xres;
    screenHeight = vinfo.yres;

    // 计算 newWidth 和 newHeight，确保图像适应屏幕
    double widthRatio = static_cast<double>(screenWidth) / cameraWidth;
    double heightRatio = static_cast<double>(screenHeight) / cameraHeight;
    double scale = std::min(widthRatio, heightRatio); // 选择较小的比例，确保图像不超出屏幕

    newWidth = static_cast<int>(cameraWidth * scale);
    newHeight = static_cast<int>(cameraHeight * scale);

    line_tracking_width = newWidth / calc_scale;
    line_tracking_height = newHeight / calc_scale;
}

// 将 RGB888 转换为 RGB565
ushort rgb888_to_rgb565(const cv::Vec3b &color)
{
    return ((color[2] >> 3) << 11) | ((color[1] >> 2) << 5) | (color[0] >> 3);
}

cv::Mat img;

void display()
{
    cv::Mat fbMat(screenHeight, screenWidth, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Mat resizedFrame;
    cv::Mat coloredResizedFrame;

    // 将图像从 BGR 转换为 HSV
    cv::Mat hsvImage;
    cv::cvtColor(img, hsvImage, cv::COLOR_BGR2HSV);

    // 分离 HSV 通道
    std::vector<cv::Mat> hsvChannels;
    cv::split(hsvImage, hsvChannels);

    cv::Mat bframe;
    switch (displayMode)
    {
    case 0:
        // 缩放视频到新尺寸
        cv::resize(binarizedFrame, resizedFrame, cv::Size(newWidth, newHeight));
        // 将单通道的二值化图像转换为三通道的彩色图像
        cv::cvtColor(resizedFrame, coloredResizedFrame, cv::COLOR_GRAY2BGR); // 转换为彩色图像
        break;
    case 1:
        cv::resize(raw_frame, coloredResizedFrame, cv::Size(newWidth, newHeight));
        break;
    case 2:
        // 缩放视频到新尺寸
        cv::resize(morphologyExFrame, resizedFrame, cv::Size(newWidth, newHeight));
        // 将单通道的二值化图像转换为三通道的彩色图像
        cv::cvtColor(resizedFrame, coloredResizedFrame, cv::COLOR_GRAY2BGR); // 转换为彩色图像
        break;
    case 3:
    case 4:
    case 5:
        // 缩放视频到新尺寸
        cv::resize(hsvChannels[displayMode - 3], resizedFrame, cv::Size(newWidth, newHeight));
        cv::threshold(resizedFrame, bframe, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
        // 将单通道的二值化图像转换为三通道的彩色图像
        cv::cvtColor(bframe, coloredResizedFrame, cv::COLOR_GRAY2BGR); // 转换为彩色图像
        break;
    case 6:
        // 缩放视频到新尺寸
        cv::resize(hsvImage, resizedFrame, cv::Size(newWidth, newHeight));
        cv::Mat yellowMask;
        inRange(resizedFrame, cv::Scalar(10, 100, 100), cv::Scalar(40, 255, 255), yellowMask);
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2));
        morphologyEx(yellowMask, yellowMask, cv::MORPH_OPEN, kernel);
        morphologyEx(yellowMask, yellowMask, cv::MORPH_CLOSE, kernel);
        bframe = yellowMask;
        // 将单通道的二值化图像转换为三通道的彩色图像
        cv::cvtColor(bframe, coloredResizedFrame, cv::COLOR_GRAY2BGR); // 转换为彩色图像
        break;
    }

    // 将缩放后的图像居中放置在帧缓冲区图像中，填充黑色边框
    fbMat.setTo(cv::Scalar(0, 0, 0)); // 清空缓冲区（填充黑色）
    cv::Rect roi((screenWidth - newWidth) / 2, (screenHeight - newHeight) / 2, newWidth, newHeight);
    coloredResizedFrame.copyTo(fbMat(roi));

    // 绘制左右边界线和中线
    int scaledLeftX, scaledRightX, scaledMidX, scaledLeftFilteredX, scaledRightFilteredX, scaledMidFilteredX, scaledY;

    cv::line(fbMat(roi), cv::Point(newWidth / 2, 0), cv::Point(newWidth / 2, newHeight), cv::Scalar(0, 0, 0), 1);
    cv::line(fbMat(roi), cv::Point(0, 40), cv::Point(newWidth, 40), cv::Scalar(0, 0, 0), 1);
    for (int y = 0; y < line_tracking_height; y++)
    {
        // 根据缩放比例调整X坐标
        scaledLeftX = static_cast<int>(left_line[y] * calc_scale);
        scaledRightX = static_cast<int>(right_line[y] * calc_scale);
        scaledMidX = static_cast<int>(mid_line[y] * calc_scale);
        scaledLeftFilteredX = static_cast<int>(left_line_filtered[y] * calc_scale);
        scaledRightFilteredX = static_cast<int>(right_line_filtered[y] * calc_scale);
        scaledMidFilteredX = static_cast<int>(mid_line_filtered[y] * calc_scale);
        scaledY = static_cast<int>(y * calc_scale);

        cv::line(fbMat(roi), cv::Point(scaledLeftX, scaledY), cv::Point(scaledLeftX, scaledY), cv::Scalar(0, 0, 255), calc_scale);
        cv::line(fbMat(roi), cv::Point(scaledLeftFilteredX, scaledY), cv::Point(scaledLeftFilteredX, scaledY), cv::Scalar(255, 255, 0), calc_scale);

        cv::line(fbMat(roi), cv::Point(scaledRightX, scaledY), cv::Point(scaledRightX, scaledY), cv::Scalar(0, 255, 0), calc_scale);
        cv::line(fbMat(roi), cv::Point(scaledRightFilteredX, scaledY), cv::Point(scaledRightFilteredX, scaledY), cv::Scalar(255, 0, 255), calc_scale);

        cv::line(fbMat(roi), cv::Point(scaledMidX, scaledY), cv::Point(scaledMidX, scaledY), cv::Scalar(255, 0, 0), calc_scale);
        cv::line(fbMat(roi), cv::Point(scaledMidFilteredX, scaledY), cv::Point(scaledMidFilteredX, scaledY), cv::Scalar(0, 255, 255), calc_scale);
    }

    for (int y = 0; y < fbMat.rows; y++)
    {
        for (int x = 0; x < fbMat.cols; x++)
        {
            cv::Vec3b color = fbMat.at<cv::Vec3b>(y, x);
            fb_data[y * vinfo.xres + x] = rgb888_to_rgb565(color);
        }
    }
}

void defaultImageCallback(const string &fullpath)
{
    img = cv::imread(fullpath);
    if (img.empty())
    {
        cerr << "Error: Cannot grab frame" << endl;
        return;
    }
    raw_frame = img;

    frame_begin = std::chrono::high_resolution_clock::now();

    image_main();

    frame_end = std::chrono::high_resolution_clock::now();
    frame_duration = frame_end - frame_begin;

    display();
}

void triggerCallback(int num)
{
    if (!image_callback)
        return;

    auto it = file_map.find(num);
    if (it != file_map.end())
    {
        string fullpath = base_path + "/" + it->second;
        image_callback(fullpath);
    }
}

int fbInit()
{
    // Framebuffer 设备
    fb_fd = open(fb_device, O_RDWR);
    if (fb_fd == -1)
    {
        cerr << "Error: Cannot open framebuffer device" << endl;
        return -1;
    }

    // 获取 Framebuffer 信息
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
    fb_data = (ushort *)mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_data == MAP_FAILED)
    {
        cerr << "Error: Failed to map framebuffer to memory" << endl;
        close(fb_fd);
        return -1;
    }

    return 0;
}

void disableRawMode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode()
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

bool collectImages(const char *path)
{
    base_path = path;
    DIR *dir = opendir(path);
    if (!dir)
    {
        cerr << "无法打开目录: " << path << endl;
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        string filename = entry->d_name;
        if (filename.length() != 15)
            continue;
        if (filename.substr(0, 6) != "image_")
            continue;
        if (filename.substr(11, 4) != ".jpg")
            continue;

        string num_str = filename.substr(6, 5);
        if (num_str.find_first_not_of("0123456789") != string::npos)
            continue;

        int num = stoi(num_str);
        file_map[num] = filename;
    }

    closedir(dir);
    return !file_map.empty();
}

void showHelp()
{
    cout << "控制命令:\n"
         << "  左方向键  - 上一张图片\n"
         << "  右方向键  - 下一张图片\n"
         << "  g        - 跳转到指定序号\n"
         << "  q        - 退出程序\n";
}

void jumpToNumber()
{
    disableRawMode();
    cout << "\n输入要跳转的图片序号: ";
    string input;
    getline(cin, input);
    tcflush(STDIN_FILENO, TCIFLUSH);

    try
    {
        int target = stoi(input);
        auto it = file_map.find(target);
        if (it != file_map.end())
        {
            current_num = target;
            triggerCallback(current_num);
        }
        else
        {
            cout << "未找到序号: " << target << endl;
        }
    }
    catch (...)
    {
        cout << "无效输入" << endl;
    }
    enableRawMode();
}

int main(int argc, char *argv[])
{
    if (fbInit())
    {
        cerr << "Framebuffer初始化失败" << endl;
        return 1;
    }
    imgInit();
    const char *path = argc > 1 ? argv[1] : ".";
    if (!collectImages(path))
    {
        cerr << "未找到符合要求的图片文件" << endl;
        return 1;
    }

    enableRawMode();
    // 设置回调函数
    image_callback = defaultImageCallback;

    // 初始化时触发第一次回调
    current_num = file_map.begin()->first;
    triggerCallback(current_num);
    showHelp();

    while (true)
    {
        auto it = file_map.find(current_num);
        if (it == file_map.end())
        {
            cerr << "当前文件不可用" << endl;
            break;
        }

        cout << "\r当前图片: " << it->second << " (序号: " << current_num << ")" << "计算耗时：" << frame_duration.count() << "秒" << " 当前模式: " << displayMode << flush;

        char c;
        if (read(STDIN_FILENO, &c, 1) == 1)
        {
            if (c == '\033')
            { // ESC序列
                char seq[2];
                if (read(STDIN_FILENO, seq, 2) == 2)
                {
                    if (seq[0] == '[')
                    {
                        auto next_it = file_map.upper_bound(current_num);
                        auto prev_it = file_map.lower_bound(current_num);

                        switch (seq[1])
                        {
                        case 'D': // 左方向键
                            if (prev_it != file_map.begin())
                            {
                                current_num = (--prev_it)->first;
                                triggerCallback(current_num);
                            }
                            break;
                        case 'C': // 右方向键
                            if (next_it != file_map.end())
                            {
                                current_num = next_it->first;
                                triggerCallback(current_num);
                            }
                            break;
                        }
                    }
                }
            }
            else if (c == 'q')
            {
                break;
            }
            else if (c == 'g')
            {
                jumpToNumber();
            }
            else if (c == 't')
            {
                displayMode = (displayMode + 1) % displayModeCount;
                display();
            }
        }
    }

    disableRawMode();
    cout << "\n程序已退出" << endl;
    return 0;
}