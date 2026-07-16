#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include "unistd.h"
#include <string>

// 将单色像素映射为 16 位数据，0xFFFF 表示全亮，0x0000 表示熄灭
uint16_t monochrome_to_rgb565(bool is_on) {
    return is_on ? 0xFFFF : 0x0000;
}

// 使用 Floyd-Steinberg 抖动算法将灰度图像转换为单色
void apply_dithering(cv::Mat& image) {
    int height = image.rows;
    int width = image.cols;
    for (int y = 0; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            // 获取当前像素的灰度值
            uint8_t old_pixel = image.at<uint8_t>(y, x);
            // 将灰度值转换为单色（0 或 255）
            uint8_t new_pixel = old_pixel > 128 ? 255 : 0;
            image.at<uint8_t>(y, x) = new_pixel;

            // 计算误差
            int error = old_pixel - new_pixel;

            // 将误差扩散到邻近像素
            image.at<uint8_t>(y, x + 1) += error * 7 / 16;
            image.at<uint8_t>(y + 1, x - 1) += error * 3 / 16;
            image.at<uint8_t>(y + 1, x) += error * 5 / 16;
            image.at<uint8_t>(y + 1, x + 1) += error * 1 / 16;
        }
    }
}

// 将图像写入到 framebuffer 设备
void write_to_framebuffer(const cv::Mat& image, const std::string& fb_device) {
    std::ofstream fb(fb_device, std::ios::out | std::ios::binary);
    if (!fb) {
        std::cerr << "无法打开 framebuffer 设备: " << fb_device << std::endl;
        return;
    }

    // 遍历每个像素，将其转换为 16 位格式写入设备
    for (int y = 0; y < image.rows; ++y) {
        for (int x = 0; x < image.cols; ++x) {
            // 获取像素值（0 或 255），并转换为 0xFFFF（全亮）或 0x0000（熄灭）
            uint16_t pixel = monochrome_to_rgb565(image.at<uint8_t>(y, x) > 250);
            fb.write(reinterpret_cast<char*>(&pixel), sizeof(pixel));
        }
    }

    fb.close();
}

// 新的drawNumber函数，接受指定的位置参数
void drawNumber(cv::Mat& img, int number, cv::Point position) {
    // 设置字体、大小和厚度
    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = 0.3;
    int thickness = 1;

    // cv::Size textSize = cv::getTextSize(std::to_string(number), fontFace, fontScale, thickness, 0);
    // std::cout << "Text width: " << textSize.width << ", height: " << textSize.height << std::endl;
    // 将数字绘制到img中，白色（255）为亮
    putText(img, std::to_string(number), position, fontFace, fontScale, cv::Scalar(255), thickness);
}
// 新的drawNumber函数，接受指定的位置参数
void drawString(cv::Mat& img, std::string str, cv::Point position) {
    // 设置字体、大小和厚度
    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = 0.3;
    int thickness = 1;

    // cv::Size textSize = cv::getTextSize(str, fontFace, fontScale, thickness, 0);
    // std::cout << "Text width: " << textSize.width << ", height: " << textSize.height << std::endl;
    // 将数字绘制到img中，白色（255）为亮
    putText(img, str, position, fontFace, fontScale, cv::Scalar(255), thickness);
}

int main() {
    std::cout << cv::getBuildInformation() << std::endl;

    cv::VideoCapture cap;  // 打开视频文件
    cap.open("input_video.mp4");
    if (!cap.isOpened()) {
        std::cerr << "无法打开视频文件" << std::endl;
        return -1;
    }

    // 获取视频的帧率
    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0) {
        std::cerr << "无法获取视频帧率，使用默认帧率 30 FPS" << std::endl;
        fps = 30.0;  // 默认帧率
    }
    double frame_rate = fps;

    // 计算每帧的延迟时间（毫秒）
    double frame_delay = static_cast<double>(1000000 / fps);

    // 打开 framebuffer 设备
    std::string fb_device = "/dev/fb1";

    // 设置显示的目标尺寸
    int target_width = 128;
    int target_height = 64;

    cv::Mat frame, gray_frame, resized_frame, output_frame;
    int frame_count = 0;
    auto frame_start_time = std::chrono::high_resolution_clock::now();
    auto frame_end_time = std::chrono::high_resolution_clock::now();
    auto frame_end_time1 = std::chrono::high_resolution_clock::now();
    while (cap.read(frame)) {
        // 将图像转换为灰度图像
        cv::cvtColor(frame, gray_frame, cv::COLOR_BGR2GRAY);

        // 计算缩放比例，保持原始比例
        double aspect_ratio = static_cast<double>(gray_frame.cols) / gray_frame.rows;
        int new_width, new_height;
        if (aspect_ratio > static_cast<double>(target_width) / target_height) {
            new_width = target_width;
            new_height = static_cast<int>(target_width / aspect_ratio);
        } else {
            new_height = target_height;
            new_width = static_cast<int>(target_height * aspect_ratio);
        }

        // 缩放图像
        cv::resize(gray_frame, resized_frame, cv::Size(new_width, new_height));

        // 创建一个目标大小的黑色图像并将缩放的图像居中放置
        output_frame = cv::Mat::zeros(target_height, target_width, CV_8UC1);
        resized_frame.copyTo(output_frame(cv::Rect((target_width - new_width) / 2, (target_height - new_height) / 2, new_width, new_height)));

        // 应用抖动算法
        // apply_dithering(output_frame);

        // 计算帧处理时间
        std::chrono::duration<double> frame_duration = frame_end_time - frame_start_time;
        double frame_rate = 1.0 / frame_duration.count();

        // 更新帧计数
        frame_count++;

        // 绘制帧耗时和帧数
        drawNumber(output_frame, frame_count, cv::Point(0, 20)); // 绘制帧数
        drawNumber(output_frame, static_cast<int>(frame_rate), cv::Point(0, 40)); // 绘制帧耗时
        // 写入 framebuffer 设备
        write_to_framebuffer(output_frame, fb_device);

        frame_end_time1 = std::chrono::high_resolution_clock::now();
        frame_duration = frame_end_time1 - frame_end_time;
        // 等待一段时间，以模拟视频帧率
        std::cout << frame_duration.count() << std::endl;
        usleep(std::max(0.0, frame_delay - frame_duration.count() * 1000000));

        // 计算每帧的开始时间
        frame_start_time = frame_end_time;
        frame_end_time = std::chrono::high_resolution_clock::now();
    }

    cap.release();
    return 0;
}
