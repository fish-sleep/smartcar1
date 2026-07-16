/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2024-10-10 15:02:00
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2024-12-01 03:49:47
 * @FilePath: /smartcar/lib/GPIO.h
 * @Description: 
 * 
 * Copyright (c) 2024 by ilikara 3435193369@qq.com, All Rights Reserved. 
 */

#ifndef GPIO_H
#define GPIO_H

#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>

class GPIO
{
public:
    GPIO(int gpioNum);
    ~GPIO();

    GPIO(const GPIO&) = delete;
    GPIO& operator=(const GPIO&) = delete;

    bool setDirection(const std::string &direction);
    bool setEdge(const std::string &edge);
    bool setValue(bool value);     // 设置 GPIO 输出值
    bool readValue();              // 读取 GPIO 输入值
    int getFileDescriptor() const; // 获取 GPIO 文件描述符

private:
    int gpioNum;
    int fd; // 文件描述符
    std::string gpioPath;

    bool writeToFile(const std::string &path, const std::string &value);
};

#endif // GPIO_H
