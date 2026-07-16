/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2024-11-30 09:06:41
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2025-01-08 07:09:44
 * @FilePath: /smartcar/encoder_demo/encoder_demo.cpp
 * @Description: 编码器测试用
 *
 * Copyright (c) 2024 by ilikara 3435193369@qq.com, All Rights Reserved.
 */
#include "encoder.h"
#include <iostream>
int main()
{
    ENCODER encoder(0, 73);
    while (1)
    {
        std::cout << encoder.pulse_counter_update() << std::endl;
        usleep(5000);
    }
    return 0;
}