/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2025-03-11 02:38:15
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2025-03-29 01:39:17
 * @FilePath: /2k300_smartcar/udp_receive/udp_receive.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdint>
#include <string.h>
#include <opencv2/opencv.hpp>
#include <thread>

#include "MotorController.h"
#include "PwmController.h"
#include "GPIO.h"
#include <iomanip>

cv::VideoCapture cap;

std::string directory = "./image";
int saved_frame_count;
void streamCapture(void)
{
    cv::Mat frame;
    while (1)
    {
        cap.read(frame);
        if (frame.empty())
        {
            std::cerr << "Save Error: Frame is empty." << std::endl;
            continue;
        }

        // 构建文件名
        std::ostringstream filename;
        filename << directory << "/image_" << std::setw(5) << std::setfill('0') << saved_frame_count << ".jpg";

        saved_frame_count++;
        // 保存图像
        cv::imwrite(filename.str(), frame);
    }
    return;
}

GPIO mortorEN(73);
MotorController *motorController[2] = {nullptr, nullptr};
PwmController servo(1, 0, 3000000, 1500000);

void Init()
{
    mortorEN.setDirection("out");
    mortorEN.setValue(1);
    const int pwmchip[2] = {8, 8};
    const int pwmnum[2] = {2, 1};
    const int gpioNum[2] = {12, 13};
    const int encoder_pwmchip[2] = {0, 3};
    const int encoder_gpioNum[2] = {75, 72};
    const int encoder_dir[2] = {1, -1};
    const unsigned int duty_cycle_ns = 0;
    const unsigned int period_ns = 50000; // 20 kHz

    for (int i = 0; i < 2; ++i)
    {
        motorController[i] = new MotorController(pwmchip[i], pwmnum[i], gpioNum[i], period_ns,duty_cycle_ns,
                                                 0, 0, 0, 0,
                                                 encoder_pwmchip[i], encoder_gpioNum[i], encoder_dir[i]);
        motorController[i]->updateduty(0);
    }

    servo.setPeriod(3040000);
    servo.setDutyCycle(1520000);
    servo.enable();
}

int main()
{
    // 打开默认摄像头（设备编号 0）
    cap.open(0);

    // 检查摄像头是否成功打开
    if (!cap.isOpened())
    {
        printf("无法打开摄像头\n");
        return -1;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 320);                                  // 宽度
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 240);                                // 高度
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G')); // 视频流格式
    cap.set(cv::CAP_PROP_AUTO_EXPOSURE, -1);                                   // 设置自动曝光

    std::thread camworker = std::thread(streamCapture);

    Init();

    const int PORT = 8888;
    const int BUFFER_SIZE = 2 * sizeof(float);

    // 创建UDP Socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        std::cerr << "Socket creation failed" << std::endl;
        return -1;
    }

    // 绑定地址和端口
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (sockaddr *)&server_addr, sizeof(server_addr)))
    {
        std::cerr << "Bind failed" << std::endl;
        close(sockfd);
        return -1;
    }

    // 接收数据
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    while (true)
    {
        ssize_t bytes_received = recvfrom(
            sockfd,
            buffer,
            BUFFER_SIZE,
            0,
            (sockaddr *)&client_addr,
            &client_len);

        if (bytes_received == BUFFER_SIZE)
        {
            // 解析第一个float
            uint32_t temp1;
            memcpy(&temp1, buffer, 4);
            temp1 = ntohl(temp1);
            float val1;
            memcpy(&val1, &temp1, 4);

            // 解析第二个float
            uint32_t temp2;
            memcpy(&temp2, buffer + 4, 4);
            temp2 = ntohl(temp2);
            float val2;
            memcpy(&val2, &temp2, 4);

            std::cout << "收到数据: "
                      << "滑块1=" << val1
                      << ", 滑块2=" << val2
                      << std::endl;

            double servoduty_ns = (val1) / 100 * servo.readPeriod() + 1520000;
            servo.setDutyCycle(servoduty_ns);

            motorController[0]->updateduty(val2);
            motorController[1]->updateduty(val2);
        }
        else
        {
            std::cerr << "Invalid data size" << std::endl;
        }
    }

    close(sockfd);
    return 0;
}
