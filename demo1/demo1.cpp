/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2025-01-07 02:21:05
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2025-03-29 01:36:06
 * @FilePath: /smartcar/demo1/demo1.cpp
 * @Description: 遥控车Demo，通过键盘控制小车的运动，WASD分别控制前后左右，按下Q退出。由于SSH可能会只捕获到一个按键，所以设定按下WS时清除转向，不按WS时不清除前进后退。
 */
#include <iostream>
#include <unordered_set>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "MotorController.h"
#include "PwmController.h"
#include "GPIO.h"

GPIO mortorEN(73);
MotorController *motorController[2] = {nullptr, nullptr};
PwmController servo(1, 0);

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
        motorController[i] = new MotorController(pwmchip[i], pwmnum[i], gpioNum[i], period_ns, duty_cycle_ns,
                                                 0, 0, 0, 0,
                                                 encoder_pwmchip[i], encoder_gpioNum[i], encoder_dir[i]);
    }

    servo.setPeriod(3000000);
    
    servo.enable();
}

// 设置非阻塞输入
void setNonBlockingInput(bool enable)
{
    termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    if (enable)
    {
        tty.c_lflag &= ~ICANON; // 禁用规范模式
        tty.c_lflag &= ~ECHO;   // 禁用回显
    }
    else
    {
        tty.c_lflag |= ICANON;
        tty.c_lflag |= ECHO;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

int main()
{
    std::unordered_set<char> pressedKeys;

    setNonBlockingInput(true);
    std::cout << "Press keys (WASD). Press 'q' to quit." << std::endl;

    Init();

    while (true)
    {
        char ch = getchar();

        if (ch == 'q')
        { // 按 'q' 退出
            break;
        }

        // 添加按下的键到集合中，并响应按下的键
        if (ch == 'w' || ch == 'W')
        {
            pressedKeys.insert('W');
            motorController[0]->updateduty(17);
            motorController[1]->updateduty(17);
            servo.setDutyCycle(1520000);
        }
        else if (ch == 'a' || ch == 'A')
        {
            pressedKeys.insert('A');
            servo.setDutyCycle(1360000);
        }
        else if (ch == 's' || ch == 'S')
        {
            pressedKeys.insert('S');
            motorController[0]->updateduty(-17);
            motorController[1]->updateduty(-17);
            servo.setDutyCycle(1520000);
        }
        else if (ch == 'd' || ch == 'D')
        {
            pressedKeys.insert('D');
            servo.setDutyCycle(1680000);
        }

        // 显示当前按下的键
        std::cout << "Pressed Keys: ";
        for (char key : pressedKeys)
        {
            std::cout << key << " ";
        }
        std::cout << std::endl;

        // 清空按下的键集合
        pressedKeys.clear();

        usleep(10000); // 10ms
    }

    setNonBlockingInput(false);
    return 0;
}