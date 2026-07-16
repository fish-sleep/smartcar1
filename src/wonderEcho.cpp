/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2025-05-29 15:49:40
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2025-05-29 15:50:40
 * @FilePath: \smartcar1\src\wonderEcho.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "wonderEcho.h"
#include <fcntl.h>
#include <iostream>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <mutex>
#include <sys/ioctl.h>
#include <unistd.h>

std::mutex i2c_mutex;
static int wonderEcho_fd;

inline int i2c_write_i2c_block_data(int file, uint8_t reg, uint8_t length, const uint8_t* values) {
    union i2c_smbus_data data;
    struct i2c_smbus_ioctl_data args;

    if (length > I2C_SMBUS_BLOCK_MAX) {
        return -1;
    }

    for (int i = 0; i < length; i++) {
        data.block[i + 1] = values[i];
    }
    data.block[0] = length;

    args.read_write = I2C_SMBUS_WRITE;
    args.command = reg;
    args.size = I2C_SMBUS_I2C_BLOCK_DATA;
    args.data = &data;

    if (ioctl(file, I2C_SMBUS, &args) == -1) {
        return -1;
    }
    return 0;
}

int wonderEchoInit() {
    int file;
    const char* filename = "/dev/i2c-2";
    int addr = 0x34;

    if ((file = open(filename, O_RDWR)) < 0) {
        std::cerr << "Failed to open the i2c bus" << std::endl;
        return -1;
    }

    if (ioctl(file, I2C_SLAVE, addr) < 0) {
        std::cerr << "Failed to acquire bus access and/or talk to slave" << std::endl;
        close(file);
        return -1;
    }

    wonderEcho_fd = file;
    return file;
}

int wonderEchoSend(uint8_t d1, uint8_t d2) {
    int file = wonderEcho_fd;
    uint8_t data[2] = { d1, d2 };

    if (data[0] != 0x00 && data[0] != 0xFF) {
        std::cerr << "First byte must be 0x00 or 0xFF!" << std::endl;
        return -1;
    }

    {
        std::lock_guard<std::mutex> lock(i2c_mutex);
        uint8_t reg = 0x6e;
        if (i2c_write_i2c_block_data(file, reg, sizeof(data), data) < 0) {
            std::cerr << "Failed to write to the i2c device" << std::endl;
        }
    }
    return 0;
}