#ifndef _FRAME_BUFFER_H
#define _FRAME_BUFFER_H

#include <opencv2/opencv.hpp>

void convertMatToRGB565(const cv::Mat &frame, uint16_t *buffer, int width, int height);

#endif