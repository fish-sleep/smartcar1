#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

using namespace cv;
using namespace std;

// 全局变量：存储预加载的模板图像
std::vector<cv::Mat> landmark_templates;

// 初始化模板图像
void initTemplates()
{
    landmark_templates.push_back(cv::imread("img/both.png", cv::IMREAD_GRAYSCALE));
    landmark_templates.push_back(cv::imread("img/left.png", cv::IMREAD_GRAYSCALE));
    landmark_templates.push_back(cv::imread("img/right.png", cv::IMREAD_GRAYSCALE));
    landmark_templates.push_back(cv::imread("img/light.png", cv::IMREAD_GRAYSCALE));

    for (auto &img : landmark_templates)
    {
        cv::bitwise_not(img, img); // 反转图像，便于匹配
    }
}

// 地标匹配函数
bool matchLandmark(const Mat &inputImage, int &matchedIndex, float &confidence)
{
    // 转换为灰度图像
    Mat gray;
    cvtColor(inputImage, gray, COLOR_BGR2GRAY);

    // 提取蓝色区域
    Mat hsv_img;
    cvtColor(inputImage, hsv_img, COLOR_BGR2HSV);
    Mat mask;
    inRange(hsv_img, Scalar(95, 100, 0), Scalar(130, 255, 255), mask); // 蓝色范围
    Mat blueRegion;
    bitwise_and(inputImage, inputImage, blueRegion, mask);

    // 转换为灰度并二值化
    Mat grayBlueRegion;
    cvtColor(blueRegion, grayBlueRegion, COLOR_BGR2GRAY);
    Mat binary;
    threshold(grayBlueRegion, binary, 0, 255, THRESH_BINARY | THRESH_OTSU);

    // 查找轮廓
    vector<vector<Point>> contours;
    findContours(binary, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    // 过滤轮廓
    vector<vector<Point>> filteredContours;
    for (const auto &contour : contours)
    {
        double perimeter = arcLength(contour, true);
        if (perimeter > 100) // 过滤掉太小的轮廓
        {
            filteredContours.push_back(contour);
        }
    }

    // 如果没有找到轮廓，返回失败
    if (filteredContours.empty())
    {
        return false;
    }

    // 创建掩码并填充轮廓
    Mat filledMask = Mat::zeros(inputImage.size(), CV_8UC1);
    drawContours(filledMask, filteredContours, -1, Scalar(255), FILLED);

    // 提取地标区域
    Mat landmarkRegion;
    bitwise_and(inputImage, inputImage, landmarkRegion, filledMask);
    cvtColor(landmarkRegion, landmarkRegion, COLOR_BGR2GRAY);
    threshold(landmarkRegion, landmarkRegion, 0, 255, THRESH_BINARY | THRESH_OTSU);

    // 查找地标区域的外接矩形
    vector<Point> nonZeroPoints;
    findNonZero(landmarkRegion, nonZeroPoints);
    if (nonZeroPoints.empty())
    {
        return false;
    }

    Rect boundingRect = cv::boundingRect(nonZeroPoints);
    if (boundingRect.width * boundingRect.height < 800) // 过滤太小的区域
    {
        return false;
    }

    // 裁剪地标区域
    Mat landmarkImage = landmarkRegion(boundingRect);

    // 模板匹配
    int minDiff = landmarkImage.size().area(); // 初始化为最大可能值
    int matchIndex = -1;
    float bestConfidence = 0.0f;

    for (size_t i = 0; i < landmark_templates.size(); ++i)
    {
        // 调整模板大小以匹配地标区域
        Mat resizedTemplate;
        resize(landmark_templates[i], resizedTemplate, landmarkImage.size());

        // 计算差异
        Mat diff;
        absdiff(resizedTemplate, landmarkImage, diff);
        int sumDiff = countNonZero(diff);

        // 计算置信度
        float currentConfidence = 100.0f - (static_cast<float>(sumDiff) / (resizedTemplate.rows * resizedTemplate.cols)) * 100.0f;

        // 更新最佳匹配
        if (sumDiff < minDiff)
        {
            minDiff = sumDiff;
            matchIndex = static_cast<int>(i);
            bestConfidence = currentConfidence;
        }
    }

    // 如果找到匹配且置信度大于40%，返回成功
    if (matchIndex != -1 && bestConfidence > 40.0f)
    {
        matchedIndex = matchIndex;
        confidence = bestConfidence;
        return true;
    }

    return false;
}

// 将 OpenCV 的 Mat 图像转换为 RGB565 格式
void convertToRGB565(const Mat &src, uint16_t *dst)
{
    for (int y = 0; y < src.rows; ++y)
    {
        for (int x = 0; x < src.cols; ++x)
        {
            Vec3b pixel = src.at<Vec3b>(y, x);
            uint16_t b = (pixel[0] >> 3) & 0x1F; // 5 bits
            uint16_t g = (pixel[1] >> 2) & 0x3F; // 6 bits
            uint16_t r = (pixel[2] >> 3) & 0x1F; // 5 bits
            dst[y * src.cols + x] = (r << 11) | (g << 5) | b;
        }
    }
}

int main()
{
    // 初始化模板
    initTemplates();

    // 打开摄像头
    VideoCapture cap(0); // 0 表示默认摄像头
    if (!cap.isOpened())
    {
        cerr << "无法打开摄像头！" << endl;
        return -1;
    }

    // 设置摄像头分辨率
    cap.set(CAP_PROP_FRAME_WIDTH, 320);
    cap.set(CAP_PROP_FRAME_HEIGHT, 240);

    // 打开帧缓冲设备
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1)
    {
        cerr << "无法打开帧缓冲设备！" << endl;
        return -1;
    }

    // 获取帧缓冲设备信息
    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == -1)
    {
        cerr << "无法获取帧缓冲设备信息！" << endl;
        close(fb_fd);
        return -1;
    }

    // 映射帧缓冲设备到内存
    size_t fb_size = vinfo.xres * vinfo.yres * 2; // RGB565 每个像素占 2 字节
    uint16_t *fb_ptr = (uint16_t *)mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_ptr == MAP_FAILED)
    {
        cerr << "无法映射帧缓冲设备！" << endl;
        close(fb_fd);
        return -1;
    }

    // 主循环
    Mat frame;
    while (true)
    {
        // 捕获一帧图像
        cap >> frame;
        if (frame.empty())
        {
            cerr << "无法捕获图像！" << endl;
            break;
        }

        // 调用地标匹配函数
        int matchedIndex;
        float confidence;
        if (matchLandmark(frame, matchedIndex, confidence))
        {
            cout << "找到地标: " << matchedIndex << "，置信度: " << confidence << "%" << endl;
        }

        // 将图像转换为 RGB565 格式并写入帧缓冲设备
        convertToRGB565(frame, fb_ptr);
    }

    // 释放资源
    munmap(fb_ptr, fb_size);
    close(fb_fd);
    cap.release();

    return 0;
}