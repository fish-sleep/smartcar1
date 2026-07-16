/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2024-10-10 15:02:10
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2024-12-01 03:50:49
 * @FilePath: /smartcar/src/GPIO.cpp
 * @Description: 
 * 
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved. 
 */
#include "GPIO.h"

GPIO::GPIO(int gpioNum) : gpioNum(gpioNum), fd(-1)
{
    gpioPath = "/sys/class/gpio/gpio" + std::to_string(gpioNum);

    // 导出 GPIO
    if (!writeToFile("/sys/class/gpio/export", std::to_string(gpioNum)))
    {
        throw std::runtime_error("Failed to export GPIO " + std::to_string(gpioNum));
    }

    // 打开 value 文件，读写方式
    fd = open((gpioPath + "/value").c_str(), O_RDWR);
    if (fd == -1)
    {
        throw std::runtime_error("Failed to open GPIO value file: " + std::string(strerror(errno)));
    }
}

GPIO::~GPIO()
{
    if (fd != -1)
    {
        close(fd); // 关闭文件描述符
    }
}

bool GPIO::setDirection(const std::string &direction)
{
    return writeToFile(gpioPath + "/direction", direction);
}

bool GPIO::setEdge(const std::string &edge)
{
    return writeToFile(gpioPath + "/edge", edge);
}

bool GPIO::setValue(bool value)
{
    if (fd == -1)
    {
        std::cerr << "GPIO file descriptor is invalid" << std::endl;
        return false;
    }

    // 使用文件描述符写入 GPIO 值 ('1' 或 '0')
    const char *val_str = value ? "1" : "0";
    if (write(fd, val_str, 1) != 1)
    {
        std::cerr << "Failed to write GPIO value: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool GPIO::readValue()
{
    if (fd == -1)
    {
        std::cerr << "GPIO file descriptor is invalid" << std::endl;
        return false;
    }

    char value;
    lseek(fd, 0, SEEK_SET); // 重置文件偏移量
    if (read(fd, &value, 1) != 1)
    {
        std::cerr << "Failed to read GPIO value: " << strerror(errno) << std::endl;
        return false;
    }
    return value == '1'; // 如果读取的值为 '1'，则返回 true，否则返回 false
}

int GPIO::getFileDescriptor() const
{
    return fd;
}

bool GPIO::writeToFile(const std::string &path, const std::string &value)
{
    std::ofstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file " << path << ": " << strerror(errno) << std::endl;
        return false;
    }
    file << value;
    return file.good();
}
