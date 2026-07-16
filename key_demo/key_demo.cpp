/*
 * @Author: Ilikara 3435193369@qq.com
 * @Date: 2025-01-26 17:00:24
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2025-03-10 14:23:16
 * @FilePath: /2k300_smartcar/jy62_demo/jy62_demo.cpp
 * @Description:
 *
 * Copyright (c) 2025 by ${git_name_email}, All Rights Reserved.
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <unistd.h>

#include "GPIO.h"

using namespace std;

GPIO keys[9] = {
    GPIO(0),
    GPIO(3),
    GPIO(1),
    GPIO(2),
    GPIO(27),
    GPIO(17),
    GPIO(16),
    GPIO(15),
    GPIO(14)};

string keysName[9] = {
    "Down",
    "Left",
    "Up",
    "OK",
    "Right",
    "SW1",
    "SW2",
    "SW3",
    "SW4"};


void draw_interface()
{
    cout << "\033[H\033[J";

    for (size_t i = 0; i < 9; ++i)
    {
        string status = keys[i].readValue() ? "HIGH" : "LOW";
        printf("│ GPIO %-7s │ %-9s │\n", keysName[i].c_str(), status.c_str());
    }
}

int main()
{
    for (size_t i = 0; i < 9; ++i)
    {
        keys[i].setDirection("in");
    }

    while (true)
    {
        draw_interface();
        usleep(100000);     // 100ms刷新间隔
        cout << "\033[13A"; // 将光标移动到界面起始位置
    }
    return 0;
}
