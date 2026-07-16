/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2025-05-29 15:49:40
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2025-06-19 15:41:10
 * @FilePath: \smartcar1\main\main.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <iostream>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <string>
#include <csignal>
#include <atomic>

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

std::atomic<bool> running(true);
void signalHandler(int signal) {
    running.store(false);
}

int main(void) {
    std::signal(SIGINT, signalHandler);

    double dest_fps = readDoubleFromFile(destfps_file);
    int dest_frame_duration = CameraInit(0, dest_fps, 320, 240);
    printf("%d\n", dest_frame_duration);

    vl53l0xInit();
    
    if (dest_frame_duration != -1) {
        // 初始化语音模块
        if (wonderEchoInit() == -1) {
            std::cerr << "Failed to initialize wonderEcho module!" << std::endl;
        }

        streamCaptureRunning = true;
        std::thread camworker = std::thread(streamCapture);
        std::cout << "Stream Capture Service started!\n";
        ControlInit();
        std::cout << "Control Initialized!\n";

        Timer CameraTimer(dest_frame_duration, std::bind(CameraHandler));
        Timer MortorTimer(8, std::bind(ControlMain));
      
        CameraTimer.start();
        std::cout << "Camera Service started!\n";
        MortorTimer.start();
        std::cout << "Control Service started!\n";


        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            mortor_kp = readDoubleFromFile(mortor_kp_file);
            mortor_ki = readDoubleFromFile(mortor_ki_file);
            mortor_kd = readDoubleFromFile(mortor_kd_file);

            stop_kp = readDoubleFromFile(stop_kp_file);
            stop_ki = readDoubleFromFile(stop_ki_file);
            stop_kd = readDoubleFromFile(stop_kd_file);


            kp = readDoubleFromFile(kp_file);
            ki = readDoubleFromFile(ki_file); 
            kd = readDoubleFromFile(kd_file);

          

        }

        std::cout << "Stopping!\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        CameraTimer.stop();
        std::cout << "Camera Timer stopped!\n";
        MortorTimer.stop();
        std::cout << "Control Timer stopped!\n";
        

        ControlExit();
        std::cout << "Control Service stopped!\n";

        vl53l0xStop();
        std::cout << "VL53L0X Service stopped!\n";

        streamCaptureRunning = false;
        if (camworker.joinable()) {
            camworker.join();
        }

        cameraDeInit();
        std::cout << "Camera Service stopped!\n";
    }
    return 0;
}