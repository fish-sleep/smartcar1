/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2025-01-04 06:51:37
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2025-06-15 13:32:39
 * @FilePath: /smartcar/lib/image_main.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
void image_main();

extern cv::Mat raw_frame;
extern cv::Mat grayFrame;
extern cv::Mat binarizedFrame;
extern cv::Mat morphologyExFrame;
extern cv::Mat track;
extern cv::Mat resized_raw_Frame;

extern std::vector<int> left_line;  // 左边缘列号数组
extern std::vector<int> right_line; // 右边缘列号数组
extern std::vector<int> mid_line;   // 中线列号数组
extern std::vector<double> left_line_filtered; // 中线列号数组
extern std::vector<double> right_line_filtered; // 中线列号数组
extern std::vector<double> mid_line_filtered;   // 中线列号数组
extern int line_tracking_width;
extern int line_tracking_height;




cv::Mat image_binerize(cv::Mat &frame);
cv::Mat find_road(cv::Mat &frame);


#endif // IMAGE_CV_H

