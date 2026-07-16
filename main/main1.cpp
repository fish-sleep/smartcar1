#include <iostream>              // 标准输入输出库
#include <fstream>               // 文件操作库
#include <opencv2/opencv.hpp>    // OpenCV图像处理库
#include <string>                // 字符串库
#include <csignal>               // 信号处理库
#include <atomic>                // 原子操作支持

// 包含项目相关头文件
#include "global.h"
#include "camera.h"
#include "control.h"
#include "Timer.h"
#include "serial.h"
#include "encoder.h"
#include "video.h"
#include "wonderEcho.h"
#include "MotorController.h"
#include "control.h"
#include "vl53l0x.hpp"

// =============================
// 全局运行标志，控制主线程循环运行与退出
// =============================
std::atomic<bool> running(true);      // 原子变量，保证跨线程安全的读写

/**
 * @brief 信号处理函数，响应中断信号(如Ctrl+C)
 * @param signal 信号号，如SIGINT
 * @note 该函数通过将running设为false，安全退出主循环
 */
void signalHandler(int signal) {
    running.store(false);   // 设置运行标志为false，通知主循环退出
}

/**
 * @brief 主程序入口函数
 * @return int 程序退出状态，0表示正常结束
 * @note 负责初始化各模块，启动捕获线程与定时器，执行主循环及退出处理
 */
int main(void) {
    std::signal(SIGINT, signalHandler); // 注册SIGINT信号处理函数，接收Ctrl+C事件

    // 从配置文件读取目标帧率
    double dest_fps = readDoubleFromFile(destfps_file);
    // 初始化摄像头，设置设备号0，目标帧率，分辨率320x240，返回每帧时间间隔（单位ms）
    int dest_frame_duration = CameraInit(0, dest_fps, 320, 240);
    printf("%d\n", dest_frame_duration);  // 打印摄像头每帧时长

    vl53l0xInit();  // 初始化VL53L0X激光测距传感器

    // 如果摄像头初始化成功，进入后续流程
    if (dest_frame_duration != -1) {
        // 初始化语音播报模块，失败时报错
        if (wonderEchoInit() == -1) {
            std::cerr << "Failed to initialize wonderEcho module!" << std::endl;
        }

        streamCaptureRunning = true;    // 设置视频流采集线程运行标志
        std::thread camworker = std::thread(streamCapture); // 启动视频采集线程
        std::cout << "Stream Capture Service started!\n";

        ControlInit();  // 初始化电机控制相关硬件及参数
        std::cout << "Control Initialized!\n";

        // 初始化定时器：
        // CameraTimer用指定间隔调用CameraHandler函数，实现周期摄像头处理
        Timer CameraTimer(dest_frame_duration, std::bind(CameraHandler));
        // MortorTimer每8毫秒调用ControlMain函数，实现电机控制周期更新
        Timer MortorTimer(8, std::bind(ControlMain));
      
        CameraTimer.start();  // 启动摄像头处理定时器
        std::cout << "Camera Service started!\n";
        MortorTimer.start();  // 启动控制处理定时器
        std::cout << "Control Service started!\n";

        // 主循环
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));  // 睡眠500ms，降低CPU占用

            // 定时从文件动态读取并更新电机PID参数 —— 支持运行时PID调节
            mortor_kp = readDoubleFromFile(mortor_kp_file);
            mortor_ki = readDoubleFromFile(mortor_ki_file);
            mortor_kd = readDoubleFromFile(mortor_kd_file);

            // 动态读取停车时的PID参数
            stop_kp = readDoubleFromFile(stop_kp_file);
            stop_ki = readDoubleFromFile(stop_ki_file);
            stop_kd = readDoubleFromFile(stop_kd_file);

            // 方向舵机PID参数读取
            kp = readDoubleFromFile(kp_file);
            ki = readDoubleFromFile(ki_file); 
            kd = readDoubleFromFile(kd_file);

            // 若需要，可添加其它参数动态更新代码
        }

        std::cout << "Stopping!\n";                           // 输出停止提示
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 等待500ms，确保任务结束

        CameraTimer.stop();       // 停止摄像头定时器
        std::cout << "Camera Timer stopped!\n";
        MortorTimer.stop();       // 停止电机控制定时器
        std::cout << "Control Timer stopped!\n";

        ControlExit();            // 释放控制模块资源
        std::cout << "Control Service stopped!\n";

        streamCaptureRunning = false;   // 向视频采集线程发送停止信号
        if (camworker.joinable()) {     // 等待采集线程安全结束
            camworker.join();
        }

        cameraDeInit();           // 关闭摄像头并释放相关资源
        std::cout << "Camera Service stopped!\n";
    }
    return 0;          // 正常退出
}
