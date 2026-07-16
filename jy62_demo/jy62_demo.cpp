/*
 * @Author: Ilikara 3435193369@qq.com
 * @Date: 2025-01-26 17:00:24
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2025-02-28 08:42:56
 * @FilePath: /2k300_smartcar/jy62_demo/jy62_demo.cpp
 * @Description:
 *
 * Copyright (c) 2025 by ${git_name_email}, All Rights Reserved.
 */
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <cstdint>
#include <termios.h>
#include <atomic>

const size_t PACKET_LENGTH = 11;

const uint8_t HEADER = 0x55;

std::atomic<int16_t> accel[3], gyro[3], angle[3];

uint8_t calculateChecksum(const std::vector<uint8_t> &data)
{
    uint8_t checksum = 0;
    for (uint8_t byte : data)
    {
        checksum += byte;
    }
    return checksum;
}

bool parsePacket(const std::vector<uint8_t> &packet)
{
    if (packet[0] != HEADER)
    {
        return false;
    }
    if (packet[1] > 0x53 || packet[1] < 0x51)
    {
        return false;
    }

    uint8_t receivedChecksum = packet.back();
    uint8_t calculatedChecksum = calculateChecksum(std::vector<uint8_t>(packet.begin(), packet.end() - 1));

    return receivedChecksum == calculatedChecksum;
}

bool setupSerialPort(int fd, int baudRate)
{
    struct termios tty;
    memset(&tty, 0, sizeof tty);

    // 获取当前串口配置
    if (tcgetattr(fd, &tty) != 0)
    {
        std::cerr << "Error getting terminal attributes" << std::endl;
        return false;
    }

    // 设置输入输出波特率
    cfsetospeed(&tty, baudRate);
    cfsetispeed(&tty, baudRate);

    // 设置数据位为 8 位
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    // 禁用奇偶校验
    tty.c_cflag &= ~PARENB;

    // 设置停止位为 1 位
    tty.c_cflag &= ~CSTOPB;

    // 禁用硬件流控制
    tty.c_cflag &= ~CRTSCTS;

    // 启用接收和本地模式
    tty.c_cflag |= CREAD | CLOCAL;

    // 禁用软件流控制
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    // 设置原始模式（禁用规范模式）
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // 禁用输出处理
    tty.c_oflag &= ~OPOST;

    // 设置读取超时和最小读取字符数
    tty.c_cc[VMIN] = 1;  // 至少读取 1 个字符
    tty.c_cc[VTIME] = 5; // 等待 0.5 秒

    // 应用配置
    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        std::cerr << "Error setting terminal attributes" << std::endl;
        return false;
    }

    return true;
}

void data_handler(const std::vector<uint8_t> &packet)
{
    switch (packet[1])
    {
    case 0x51:
        accel[0].store((int16_t)packet[3] << 8 | packet[2]);
        accel[1].store((int16_t)packet[5] << 8 | packet[4]);
        accel[2].store((int16_t)packet[7] << 8 | packet[6]);
        break;
    case 0x52:
        gyro[0].store((int16_t)packet[3] << 8 | packet[2]);
        gyro[1].store((int16_t)packet[5] << 8 | packet[4]);
        gyro[2].store((int16_t)packet[7] << 8 | packet[6]);
        break;
    case 0x53:
        angle[0].store((int16_t)packet[3] << 8 | packet[2]);
        angle[1].store((int16_t)packet[5] << 8 | packet[4]);
        angle[2].store((int16_t)packet[7] << 8 | packet[6]);
        break;
    }
}

int main()
{
    const char *device = "/dev/ttyS2";
    int fd = open(device, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        std::cerr << "Failed to open " << device << std::endl;
        return 1;
    }

    if (!setupSerialPort(fd, B115200))
    {
        std::cerr << "Failed to setup serial port" << std::endl;
        close(fd);
        return 1;
    }

    std::vector<uint8_t> buffer;
    while (true)
    {
        uint8_t byte;
        ssize_t n = read(fd, &byte, 1);
        if (n > 0)
        {
            buffer.push_back(byte);

            if (buffer.size() >= PACKET_LENGTH)
            {
                if (parsePacket(buffer))
                {
                    data_handler(buffer);
                    buffer.erase(buffer.begin(), buffer.begin() + PACKET_LENGTH);
                    std::cout << "accel:" << accel[0] << " " << accel[1] << " " << accel[2] << std::endl;
                    std::cout << "gyro:" << gyro[0] << " " << gyro[1] << " " << gyro[2] << std::endl;
                    std::cout << "angle:" << angle[0] << " " << angle[1] << " " << angle[2] << std::endl;
                }
                else
                {
                    std::cerr << "Invalid packet, discarding the first byte." << std::endl;
                    buffer.erase(buffer.begin());
                }
            }
        }
        else if (n < 0)
        {
            std::cerr << "Error reading from UART." << std::endl;
            break;
        }
    }

    close(fd);
    return 0;
}
