#ifndef VIDEO_H_
#define VIDEO_H_

#include <opencv2/opencv.hpp>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include "Timer.h"
#include "frame_buffer.h"

class Video
{
public:
    Video(const std::string &filename, double fps);
    ~Video(void);

    Timer timer;
    cv::Mat frame;
    std::mutex frameMutex;
private:
    cv::VideoCapture cap;
    cv::Mat fbImage;
    cv::Mat resizedFrame;
    void streamCapture(void);
    int screenHeight, screenWidth;
    int newWidth, newHeight;
    int fb;
    uint16_t *fb_buffer;
};

#endif