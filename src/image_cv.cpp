/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2025-01-04 06:50:56
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2025-06-04 12:40:50
 * @FilePath: /2k300_smartcar/src/image_cv.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: <url id="d0vu953of8jhk94keea0" type="url" status="parsed" title="配置" wc="45937">https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE</url> 
 */
#include "image_cv.h"  // 包含头文件image_cv.h，其中可能声明了该文件中用到的函数和变量

// 定义全局变量
cv::Mat raw_frame;  // 原始图像帧
cv::Mat grayFrame;  // 灰度图像帧
cv::Mat binarizedFrame;  // 二值化后的图像帧
cv::Mat morphologyExFrame;  // 经形态学操作后的图像帧
cv::Mat track;  // 跟踪到的道路轮廓图像
cv::Mat resized_raw_Frame;  // 缩放后的原始图像帧

// 定义全局变量，用于存储边缘列号和中线列号
std::vector<int> left_line;  // 左边缘列号数组，存储每一行左边缘的列号
std::vector<int> right_line;  // 右边缘列号数组，存储每一行右边缘的列号
std::vector<int> mid_line;  // 中线列号数组，存储每一行中线的列号
std::vector<double> left_line_filtered;  // 过滤后的左边缘列号数组
std::vector<double> right_line_filtered;  // 过滤后的右边缘列号数组
std::vector<double> mid_line_filtered;  // 过滤后的中线列号数组

// 定义全局变量，用于存储线跟踪的宽度和高度
int line_tracking_width;  // 线跟踪的宽度
int line_tracking_height;  // 线跟踪的高度

// 函数image_binerize的定义，用于对图像进行二值化处理
cv::Mat image_binerize(cv::Mat &frame)  // 参数为输入的图像帧
{
    cv::Mat output;  // 定义输出图像矩阵
    cv::Mat binarizedFrame;  // 定义二值化后的图像矩阵
    cv::Mat hsvImage;  // 定义HSV颜色空间图像矩阵
    cv::cvtColor(frame, hsvImage, cv::COLOR_BGR2HSV);  // 将输入图像从BGR颜色空间转换到HSV颜色空间

    // 将HSV图像分割成三个通道，分别存储在hsvChannels向量中
    std::vector<cv::Mat> hsvChannels;
    cv::split(hsvImage, hsvChannels);

    // 对HSV图像的第一个通道（H通道）进行阈值分割，得到二值化图像binarizedFrame
    cv::threshold(hsvChannels[0], binarizedFrame, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    // 对HSV图像的第二个通道（S通道）进行阈值分割，得到二值化图像output
    cv::threshold(hsvChannels[1], output, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    // 对output和binarizedFrame进行按位或操作，得到最终的二值化图像output
    cv::bitwise_or(output, binarizedFrame, output);

    return output;  // 返回二值化后的图像
}

// 函数find_road的定义，用于在二值化图像中查找道路轮廓
cv::Mat find_road(cv::Mat &frame)  // 参数为输入的图像帧
{
    // 定义结构元素，用于形态学操作
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(2, 2));
    // 对二值化图像进行开运算，去除噪声
    cv::morphologyEx(binarizedFrame, morphologyExFrame, cv::MORPH_OPEN, kernel);

    // 创建掩膜图像，用于泛洪填充
    cv::Mat mask = cv::Mat::zeros(line_tracking_height + 2, line_tracking_width + 2, CV_8UC1);

    // 定义种子点，用于泛洪填充的起始位置
    cv::Point seedPoint(line_tracking_width / 2, line_tracking_height - 10);

    // 在形态学处理后的图像中，以种子点为中心画一个半径为10的圆，并填充为白色
    cv::circle(morphologyExFrame, seedPoint, 10, 255, -1);

    // 定义新值、上下差异，用于泛洪填充
    cv::Scalar newVal(128);
    cv::Scalar loDiff = cv::Scalar(20);
    cv::Scalar upDiff = cv::Scalar(20);

    // 进行泛洪填充操作，将与种子点相连的区域填充为新值
    cv::floodFill(morphologyExFrame, mask, seedPoint, newVal, 0, loDiff, upDiff, 8);

    // 创建输出图像，用于存储道路轮廓
    cv::Mat outputImage = cv::Mat::zeros(line_tracking_width, line_tracking_height, CV_8UC1);

    // 将掩膜图像中的感兴趣区域复制到输出图像中
    mask(cv::Rect(1, 1, line_tracking_width, line_tracking_height)).copyTo(outputImage);

    return outputImage;  // 返回道路轮廓图像
}

// 函数image_main的定义，用于处理图像并提取道路信息
void image_main()
{
    // 将原始图像帧缩放到指定的宽度和高度
    cv::resize(raw_frame, resized_raw_Frame, cv::Size(line_tracking_width, line_tracking_height));

    // 对缩放后的图像进行二值化处理，得到二值化图像binarizedFrame
    binarizedFrame = image_binerize(resized_raw_Frame);

    // 在二值化图像中查找道路轮廓，得到道路轮廓图像track
    track = find_road(binarizedFrame);

    // 清空各个边缘和中线列号数组
    left_line.clear();
    right_line.clear();
    mid_line.clear();
    left_line_filtered.clear();
    right_line_filtered.clear();
    mid_line_filtered.clear();

    // 为各个边缘和中线列号数组分配内存，并初始化为-1
    left_line.resize(line_tracking_height, -1);
    right_line.resize(line_tracking_height, -1);
    mid_line.resize(line_tracking_height, -1);
    left_line_filtered.resize(line_tracking_height, -1);
    right_line_filtered.resize(line_tracking_height, -1);
    mid_line_filtered.resize(line_tracking_height, -1);

    // 将道路轮廓图像的数据转换为特定格式，方便后续处理
    uchar(*IMG)[line_tracking_width] = reinterpret_cast<uchar(*)[line_tracking_width]>(track.data);

    // 遍历每一行像素
    for (int i = 0; i < line_tracking_height; ++i)
    {
        int max_start = -1;  // 初始化最大区域的起始列号
        int max_end = -1;  // 初始化最大区域的结束列号
        int current_start = -1;  // 初始化当前区域的起始列号
        int current_length = 0;  // 初始化当前区域的长度
        int max_length = 0;  // 初始化最大区域的长度

        // 遍历每一列像素
        for (int j = 0; j < line_tracking_width; ++j)
        {
            // 如果当前像素属于道路轮廓
            if (IMG[i][j])
            {
                // 如果当前区域尚未开始，则记录起始列号
                if (current_length == 0)
                {
                    current_start = j;
                    current_length = 1;  // 当前区域长度加1
                }
                else
                {
                    current_length++;  // 当前区域长度加1
                }

                // 更新最大区域的起始列号、结束列号和长度
                if (current_length >= max_length)
                {
                    max_length = current_length;
                    max_start = current_start;
                    max_end = j;
                }
            }
            else
            {
                // 如果当前像素不属于道路轮廓，则重置当前区域
                current_length = 0;
                current_start = -1;
            }
        }

        // 记录当前行的左边缘和右边缘列号
        if (max_length > 0)
        {
            left_line[i] = max_start;
            right_line[i] = max_end;
        }
        else
        {
            left_line[i] = -1;
            right_line[i] = -1;
        }
    }

    // 定义平滑系数
    double a = 0.4;

    // 从下往上遍历每一行，计算中线列号并进行平滑处理
    for (int row = line_tracking_height - 1; row >= 10; --row)
    {
        // 如果当前行没有检测到边缘，则使用下一行的中线列号进行填充
        if (left_line[row] == -1 && right_line[row] == -1)
        {
            mid_line[row] = mid_line[row + 1];

            // 根据中线列号的位置，更新左边缘和右边缘列号
            if (mid_line[row] > line_tracking_width / 2)
            {
                right_line[row] = line_tracking_width - 1;
                left_line[row] = mid_line[row + 1];
            }
            else
            {
                left_line[row] = 0;
                right_line[row] = mid_line[row + 1];
            }
        }
        else
        {
            // 计算当前行的中线列号
            mid_line[row] = (left_line[row] + right_line[row]) / 2;
        }

        // 对中线列号进行平滑处理
        if (row == line_tracking_height - 1)
        {
            left_line_filtered[row] = left_line[row];
            right_line_filtered[row] = right_line[row];
            mid_line_filtered[row] = mid_line[row];
        }
        else
        {
            // 使用当前行的中线列号和下一行的过滤后的中线列号进行加权平均
            left_line_filtered[row] = a * left_line[row] + (1 - a) * left_line_filtered[row + 1];
            right_line_filtered[row] = a * right_line[row] + (1 - a) * right_line_filtered[row + 1];
            // mid_line_filtered[row] = a * mid_line[row] + (1 - a) * mid_line_filtered[row + 1];
            mid_line_filtered[row] = (left_line_filtered[row] + right_line_filtered[row]) / 2.0;
        }
    }
}