/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2024-10-10 14:36:42
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2025-03-21 10:41:17
 * @FilePath: /smartcar/src/MotorController.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "MotorController.h"  // 引入 MotorController 类的头文件
#include <global.h>            // 引入全局变量和常量的头文件

/*
 * 构造函数: 初始化电机控制所需的各个组件
 * 
 * 参数：
 * pwmchip - PWM 控制器的芯片编号
 * pwmnum - PWM 控制器的通道编号
 * gpioNum - 电机方向控制的 GPIO 引脚
 * period_ns - PWM 周期（纳秒）
 * duty_cycle_ns - PWM 占空比（纳秒）
 * kp, ki, kd - PID 控制器的参数（比例、积分、微分）
 * targetSpeed - 目标速度
 * encoder_pwmchip - 编码器的 PWM 芯片编号
 * encoder_gpioNum - 编码器的 GPIO 编号
 * encoder_dir_ - 编码器方向的定义
 */
MotorController::MotorController(int pwmchip, int pwmnum, int gpioNum, unsigned int period_ns,unsigned int duty_cycle_ns,
                                 double kp, double ki, double kd, double targetSpeed,
                                 int encoder_pwmchip, int encoder_gpioNum, int encoder_dir_)
    :  directionGPIO(gpioNum),                          // 初始化电机方向控制的 GPIO 引脚
    pwmController(pwmchip, pwmnum, period_ns, duty_cycle_ns), // 初始化 PWM 控制器
    pidController(kp, ki, kd, targetSpeed),             // 初始化 PID 控制器
    encoder(std::make_unique<ENCODER>(encoder_pwmchip, encoder_gpioNum)),  // 初始化编码器
    encoder_dir(encoder_dir_),                          // 设置编码器的方向
    last_speed(0.0),                                    // 初始化上次速度为 0
    last_update(std::chrono::steady_clock::now())        // 初始化上次更新时间
{
    pwmController.setPeriod(period_ns); // 设置 PWM 周期
    directionGPIO.setDirection("out");  // 设置方向 GPIO 为输出
    pwmController.enable(); // 启用 PWM 输出
    last_update = std::chrono::steady_clock::now(); // 更新上次更新时间
}

/*
 * 析构函数: 清理资源，禁用 PWM 控制
 */
MotorController::~MotorController(void)
{
    pwmController.disable();  // 禁用 PWM 控制器，释放资源
}

/*
 * 获取当前电机速度
 * 从编码器获取脉冲计数，计算电机的速度，并使用低通滤波器平滑速度值
 * 
 * 返回：
 * double - 当前电机的速度（单位：米/秒）
 */
double MotorController::getCurrentSpeed() 
{
    auto now = std::chrono::steady_clock::now(); // 获取当前时间
    last_update = now;  // 更新上次更新时间
    
    // 从编码器获取脉冲计数并计算实际的电机速度
    double pulse_count = encoder->pulse_counter_update() * encoder_dir;
    
    // 转换脉冲计数为实际速度
    const double pulse_per_rev = 1024.0;  // 每转的脉冲数（假设值）
    const double wheel_circumference = 0.2; // 轮周长（假设值，单位：米）
    
    double speed_rps = pulse_count / pulse_per_rev;  // 转速（每秒转数）
    double speed_mps = speed_rps * wheel_circumference;  // 转换为速度（米/秒）
    
    // 使用低通滤波器平滑速度值
    last_speed = 0.8 * last_speed + 0.2 * speed_mps;
    
    return last_speed;  // 返回计算后的当前速度
}

/*
 * 获取编码器的脉冲计数
 * 从编码器获取脉冲计数并返回，用于调节电机速度
 * 
 * 返回：
 * double - 当前编码器的脉冲计数
 */
double MotorController::encoderSpeed() 
{
    double pulse_count = encoder->pulse_counter_update() * encoder_dir;  // 获取编码器脉冲计数并考虑方向
    return pulse_count;  // 返回脉冲计数
}

/*
 * 更新电机的 PWM 占空比
 * 根据传入的占空比更新 PWM 控制器，并设置电机的旋转方向
 * 
 * 参数：
 * dutyCycle - 要设置的 PWM 占空比（单位：百分比）
 */
void MotorController::updateduty(double dutyCycle)
{
    // 计算新的 PWM 占空比
    int newduty = pwmController.readPeriod() * abs(dutyCycle) / 100.0;
    if (newduty != pwmController.readDutyCycle())  // 如果占空比有变化
    {
        pwmController.setDutyCycle(newduty);  // 更新 PWM 占空比
    }

    // 根据 PID 输出设置电机的旋转方向
    if (dutyCycle > 0)  // 正向旋转
    {
        directionGPIO.setValue(1); // 设置方向为正向
    }
    else  // 反向旋转
    {
        directionGPIO.setValue(0); // 设置方向为反向
    }
    //std::cout << encoder.pulse_counter_update() << std::endl;  // 调试信息：显示编码器脉冲数
}

/*
 * 更新电机的速度
 * 从编码器读取当前的脉冲计数，根据 PID 控制器计算并设置电机的速度
 * 
 * 无参数，返回值为 void
 */
void MotorController::updateSpeed(void)
{
    double encoderReading = encoder->pulse_counter_update() * encoder_dir;  // 获取当前的脉冲计数
    // std::cout << encoderReading << std::endl;  // 调试信息：显示编码器的脉冲计数
    double output = pidController.updateservo(encoderReading);  // 计算 PID 控制器的输出
    // int dutyCycle = static_cast<int>(output);  // 可以将输出转换为占空比
    std::cout << "  " << output << std::endl;  // 输出 PID 控制器的输出值

    if(output >= 10)  // 如果输出值大于等于 10，则限制为最大值 10
    {
        output = 10;
    }

    // 设置 PWM 占空比
    // updateduty(target_speed);  // 你可以选择直接调用 updateduty 来设置速度
    std::cout << encoderReading << "  " << output << std::endl;  // 输出调试信息：编码器读数和 PID 输出
}

/*
 * 更新 PID 控制器的目标速度
 * 设置新的目标速度，供 PID 控制器调整
 * 
 * 参数：
 * speed - 新的目标速度
 */
void MotorController::updateTarget(int speed)
{
    pidController.setTarget(speed);  // 更新 PID 控制器的目标速度
}
