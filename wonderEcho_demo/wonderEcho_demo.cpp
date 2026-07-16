/*
 * @Author: Ilikara 3435193369@qq.com
 * @Date: 2025-02-11 15:23:11
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2025-03-10 13:06:56
 * @FilePath: /2k300_smartcar/wonderEcho_demo/wonderEcho_demo.cpp
 * @Description:
 *
 * Copyright (c) 2025 by ${git_name_email}, All Rights Reserved.
 */
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <thread>
#include <atomic>
#include <mutex>

std::mutex i2c_mutex;            // 互斥锁，用于保护I2C设备访问
std::atomic<bool> running{true}; // 控制线程运行状态

int i2c_read_byte_data(int file, __u8 reg)
{
    union i2c_smbus_data data;
    struct i2c_smbus_ioctl_data args;

    args.read_write = I2C_SMBUS_READ;
    args.command = reg;
    args.size = I2C_SMBUS_BYTE_DATA;
    args.data = &data;

    if (ioctl(file, I2C_SMBUS, &args) == -1)
    {
        return -1;
    }

    return data.byte & 0xFF;
}

int i2c_write_i2c_block_data(int file, __u8 reg, __u8 length, const __u8 *values)
{
    union i2c_smbus_data data;
    struct i2c_smbus_ioctl_data args;

    if (length > I2C_SMBUS_BLOCK_MAX)
    {
        return -1;
    }

    for (int i = 0; i < length; i++)
    {
        data.block[i + 1] = values[i];
    }
    data.block[0] = length;

    args.read_write = I2C_SMBUS_WRITE;
    args.command = reg;
    args.size = I2C_SMBUS_I2C_BLOCK_DATA;
    args.data = &data;

    if (ioctl(file, I2C_SMBUS, &args) == -1)
    {
        return -1;
    }

    return 0;
}

// 轮询线程函数
void poll_i2c_device(int file)
{
    while (running)
    {
        {
            std::lock_guard<std::mutex> lock(i2c_mutex); // 加锁
            __u8 reg = 0x64;
            __u8 value = i2c_read_byte_data(file, reg);

            if (value != 0x00)
            {
                std::cout << "Value at 0x64 is not 0x00: " << std::hex << (int)value << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 1Hz轮询
    }
}

// 用户输入线程函数
void handle_user_input(int file)
{
    while (running)
    {
        __u8 data[2];
        std::cout << "Enter two bytes to send to 0x6e (first byte must be 0x00 or 0xFF): ";
        std::cin >> std::hex >> (int &)data[0] >> (int &)data[1];

        // 检查第一个字节是否为0x00或0xFF
        if (data[0] != 0x00 && data[0] != 0xFF)
        {
            std::cerr << "First byte must be 0x00 or 0xFF!" << std::endl;
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(i2c_mutex); // 加锁
            __u8 reg = 0x6e;
            if (i2c_write_i2c_block_data(file, reg, sizeof(data), data) < 0)
            {
                std::cerr << "Failed to write to the i2c device" << std::endl;
            }
            else
            {
                std::cout << "Data sent successfully: " << std::hex << (int)data[0] << " " << (int)data[1] << std::endl;
            }
        }
    }
}

int main()
{
    int file;
    const char *filename = "/dev/i2c-2"; // I2C总线设备文件
    int addr = 0x34;                     // I2C设备地址

    // 打开I2C设备
    if ((file = open(filename, O_RDWR)) < 0)
    {
        std::cerr << "Failed to open the i2c bus" << std::endl;
        return 1;
    }

    // 设置I2C设备地址
    if (ioctl(file, I2C_SLAVE, addr) < 0)
    {
        std::cerr << "Failed to acquire bus access and/or talk to slave" << std::endl;
        close(file);
        return 1;
    }

    // 创建轮询线程和用户输入线程
    std::thread poll_thread(poll_i2c_device, file);
    std::thread input_thread(handle_user_input, file);

    // 等待用户输入线程结束（按Ctrl+C退出）
    input_thread.join();

    // 停止轮询线程
    running = false;
    poll_thread.join();

    // 关闭I2C设备
    close(file);

    return 0;
}
