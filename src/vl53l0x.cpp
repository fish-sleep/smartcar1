/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2025-04-05 08:18:27
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2025-04-12 11:38:44
 * @FilePath: /smartcar/src/vl53l0x.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "vl53l0x.hpp"

#include <atomic>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

// 全局变量用于线程控制
static pthread_t read_thread;
static bool thread_running = false;
static int sensor_fd = -1;
static VL53L0X_RangingMeasurementData_t latest_data;

std::atomic<uint16_t> latest_range_mm(0);

// 线程函数
static void* read_thread_func(void* arg)
{
    while (thread_running) {
        if (vl53l0xGetData(sensor_fd, &latest_data) == 0) {
            if (latest_data.SignalRateRtnMegaCps < 20000) {
                latest_range_mm.store(8190);
            } else {
                latest_range_mm.store(latest_data.RangeMilliMeter);
            }
            std::cout << "Range: " << latest_range_mm.load() << std::endl;
        }
        usleep(125000); // 125ms间隔，可根据需要调整
    }
    return NULL;
}

int vl53l0xInit()
{
    int fd;
    struct stmvl53l0x_parameter parameter;
    unsigned int low_threshold = 0, high_threshold = 0;
    int configure_int_thresholds = 0;
    int gpio_functionnality_threshold = 0;

    fd = open("/dev/stmvl53l0x_ranging", O_RDWR | O_SYNC);
    if (fd <= 0) {
        fprintf(stderr, "Error open stmvl53l0x_ranging device: %s\n", strerror(errno));
        return -1;
    }
    // make sure it's not started
    if (ioctl(fd, VL53L0X_IOCTL_STOP, NULL) < 0) {
        fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_STOP : %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    switch (configure_int_thresholds) {
    case 1:
        gpio_functionnality_threshold = VL53L0X_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_OUT;
        break;
    case 2:
        gpio_functionnality_threshold = VL53L0X_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW;
        break;
    case 3:
        gpio_functionnality_threshold = VL53L0X_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_HIGH;
        break;
    default:
        gpio_functionnality_threshold = VL53L0X_GPIOFUNCTIONALITY_NEW_MEASURE_READY;
    }

    if (configure_int_thresholds) {

        parameter.is_read = 0;

        parameter.name = DEVICEMODE_PAR;
        parameter.value = VL53L0X_DEVICEMODE_CONTINUOUS_TIMED_RANGING;
        // parameter.value = VL53L0X_DEVICEMODE_CONTINUOUS_RANGING;
        if (ioctl(fd, VL53L0X_IOCTL_PARAMETER, &parameter) < 0) {
            fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_PARAMETER(CONTINOUS_TIMED_RANGING) : %s\n",
                strerror(errno));
            close(fd);
            return -1;
        }

        parameter.name = GPIOFUNC_PAR;
        parameter.value = gpio_functionnality_threshold;
        if (ioctl(fd, VL53L0X_IOCTL_PARAMETER, &parameter) < 0) {
            fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_PARAMETER : %s, low_threshold = %u\n",
                strerror(errno),
                low_threshold);
            close(fd);
            return -1;
        }

        if (configure_int_thresholds != 3) {
            parameter.name = LOWTHRESH_PAR;
            parameter.value = low_threshold;
            if (ioctl(fd, VL53L0X_IOCTL_PARAMETER, &parameter) < 0) {
                fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_PARAMETER : %s, low_threshold = %u\n",
                    strerror(errno),
                    low_threshold);
                close(fd);
                return -1;
            }
        }

        if (configure_int_thresholds != 2) {
            parameter.name = HIGHTHRESH_PAR;
            parameter.value = high_threshold;
            if (ioctl(fd, VL53L0X_IOCTL_PARAMETER, &parameter) < 0) {
                fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_PARAMETER : %s, high_threshold = %u\n",
                    strerror(errno),
                    high_threshold);
                close(fd);
                return -1;
            }
        }

    } else {
        parameter.name = DEVICEMODE_PAR;
        parameter.value = VL53L0X_DEVICEMODE_CONTINUOUS_RANGING;

        if (ioctl(fd, VL53L0X_IOCTL_PARAMETER, &parameter) < 0) {
            fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_PARAMETER(CONTINUOUS_RANGING) : %s\n",
                strerror(errno));
            close(fd);
            return -1;
        }
        parameter.name = GPIOFUNC_PAR;
        parameter.value = gpio_functionnality_threshold;
        if (ioctl(fd, VL53L0X_IOCTL_PARAMETER, &parameter) < 0) {
            fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_PARAMETER : %s, low_threshold = %u\n",
                strerror(errno),
                low_threshold);
            close(fd);
            return -1;
        }
    }
    // to init
    if (ioctl(fd, VL53L0X_IOCTL_INIT, NULL) < 0) {
        fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_INIT : %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    // 启动读取线程
    sensor_fd = fd;
    thread_running = true;
    if (pthread_create(&read_thread, NULL, read_thread_func, NULL)) {
        fprintf(stderr, "Error: Could not create read thread\n");
        thread_running = false;
        close(fd);
        return -1;
    }

    return 0;
}

int vl53l0xGetData(int fd, VL53L0X_RangingMeasurementData_t* data)
{
    if (fd <= 0 || !data) {
        return -1;
    }

    if (ioctl(fd, VL53L0X_IOCTL_GETDATAS, data) < 0) {
        fprintf(stderr, "VL53L0X_IOCTL_GETDATAS failed: %s\n", strerror(errno));
        return -1;
    }

    // fprintf(stdout, "Range:%4d, Error:%u, SigRate_mcps:%7d, AmbRate_mcps:%7d\r",
    //     data->RangeMilliMeter, data->RangeStatus, data->SignalRateRtnMegaCps, data->AmbientRateRtnMegaCps);

    return 0;
}

int vl53l0xStop()
{
    int fd = sensor_fd;
    fprintf(stderr, "Stop driver\n");

    // 停止读取线程
    if (thread_running) {
        thread_running = false;
        pthread_join(read_thread, NULL);
    }

    if (ioctl(fd, VL53L0X_IOCTL_STOP, NULL) < 0) {
        fprintf(stderr, "Error: Could not perform VL53L0X_IOCTL_STOP : %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);

    return 0;
}