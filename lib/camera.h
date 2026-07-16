/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2024-10-10 08:28:56
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2025-01-07 09:35:04
 * @FilePath: /smartcar/lib/camera.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef CAMERA_H_
#define CAMERA_H_

#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <algorithm>
#include <chrono>

#include "control.h"
#include "image_cv.h"
#include "PwmController.h"
#include "global.h"
#include "frame_buffer.h"
#include "serial.h"
#include "wonderEcho.h"

using namespace std;

#define WHITE_LINE_THRESHOLD 240   // 白线灰度阈值
#define DETECTION_ROI_HEIGHT 30     // 检测区域高度（距图像底部）
#define STOP_DISTANCE_PX 20         // 距离白线多少像素时停车
#define STOP_DURATION 3.0           // 停车持续时间(秒)


// 添加以下声明
bool saveCameraImage(const cv::Mat& frame, const std::string& directory);
extern int saved_frame_count;  // 声明全局计数器


int CameraInit(uint8_t camera_id, double dest_fps, int width, int height);
int CameraHandler(void);
void streamCapture(void);
void cameraDeInit(void);
std::string getCurrentTime();

extern bool streamCaptureRunning;
extern std::mutex speedMutex;
extern int sign;
extern int stop_sign;
// extern int banmaxian_num;
extern double start_time;


extern double kp;
extern double ki;
extern double kd;
extern double mortor_kp;
extern double mortor_ki;
extern double mortor_kd;
extern double stop_kp;
extern double stop_ki;
extern double stop_kd;

#endif