/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2024-10-10 14:36:47
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2025-03-21 10:42:00
 * @FilePath: /smartcar/lib/MotorController.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include "PwmController.h"
#include "PIDController.h"
#include "GPIO.h"
#include "encoder.h"
#include <chrono>
#include <memory>

using namespace std;

class MotorController
{
public:
    MotorController(int pwmchip, int pwmnum, int gpioNum, unsigned int period_ns,unsigned int duty_cycle_ns,
                    double kp, double ki, double kd, double targetSpeed,
                    int encoder_pwmchip, int encoder_gpioNum, int encoder_dir_);
    ~MotorController(void);

    double getCurrentSpeed();  // 添加方法声明
    double encoderSpeed();

    void updateSpeed(void);
    void updateTarget(int speed);
    void updateduty(double dutyCycle);

private:
    GPIO directionGPIO;
    PwmController pwmController;
    PIDController pidController;
    std::unique_ptr<ENCODER> encoder;  // 使用智能指针
    int encoder_dir;
    double last_speed;
    std::chrono::steady_clock::time_point last_update;
};

#endif // MOTOR_CONTROLLER_H
