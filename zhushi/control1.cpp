/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2024-10-10 09:02:10
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2025-06-03 16:51:18
 * @FilePath: /smartcar/src/control.cpp
 * @Description:
 *
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved.
 */

#include "control.h"
#include "vl53l0x.hpp"
#include "GPIO.h"
#include <iostream>
#include <chrono>

// ==========================
// 全局变量声明及初始化
// ==========================
MotorController *motorController[2] = {nullptr, nullptr}; // 两个电机控制器指针数组
GPIO mortorEN(73);                                       // 电机使能GPIO口
// PIDController SpeedPID(0, 0, 0, 0, INCREMENTAL);  // 速度环（增量式）注释备用
static PIDController BrakePID(2.0, 0.5, 1.0, 0.0, INCREMENTAL, 100.0); // 刹车PID控制器，固定参数

int target = 40;          // 目标速度
double last_duty = 0;    // 上一次的PWM占空比
int target_stop_speed = 0;  // 停车目标速度

// ==========================
// 赛道与挡板参数定义，单位：毫米(mm)
// ==========================
const int TRACK_WIDTH = 900;       // 赛道宽度 900mm (90cm)
const int BARRIER_WIDTH = 400;     // 挡板宽度 400mm (40cm)
const int BARRIER_GAP = 900;       // 挡板间距 900mm (90cm)
const int OBSTACLE_DISTANCE = 1000; // 避障检测距离阈值 1000mm (1m)

// ==========================
// 避障控制参数
// ==========================
const int AVOIDANCE_DURATION = 1500; // 避障动作持续时间，单位毫秒
const int AVOIDANCE_SPEED = 40;      // 避障时电机速度
const int TURN_ANGLE = 30;           // 转向角度，范围0~100

// ==========================
// 避障状态机定义
// ==========================
enum AvoidanceState {
    NO_AVOIDANCE,   // 无避障需求
    DETECTING,      // 正在检测障碍物，准备避让
    BACKING,        // 后退阶段
    TURNING,        // 转向阶段
    COMPLETED       // 避障完成，恢复正常
};

static AvoidanceState avoidance_state = NO_AVOIDANCE;  // 当前避障状态初始化为无避障
static std::chrono::steady_clock::time_point avoidance_start_time;  // 避障动作开始时间
static int last_barrier_position = 0; // 挡板位置，0未知，-1左侧，1右侧

// ==========================
// 模块初始化函数
// ==========================
void ControlInit()
{
    mortorEN.setDirection("out");  // 将使能管脚设置为输出
    mortorEN.setValue(1);           // 使能电机驱动
    
    // PWM和编码器相关引脚及参数初始化
    // PWM和编码器相关引脚及参数初始化
const int pwmchip[2] = {8, 8};           // 两个PWM控制芯片编号，两个电机都使用第8号PWM芯片
const int pwmnum[2] = {2, 1};             // 在对应PWM芯片上的PWM通道号，分别是第2路和第1路PWM信号
const int gpioNum[2] = {12, 13};          // 控制两个电机的GPIO编号，可能是使能或方向控制针脚

const int encoder_pwmchip[2] = {0, 3};   // 两个电机的编码器输入信号来自的PWM芯片编号
const int encoder_gpioNum[2] = {75, 72}; // 两个编码器信号对应的GPIO编号，用于采集速度反馈
const int encoder_dir[2] = {1, -1};      // 编码器转向方向标识，第一个电机正向计，第二个电机反向计

const unsigned int duty_cycle_ns = 0.5;    // PWM信号的初始占空比，单位纳秒，这里初始为0表示停止输出
const unsigned int period_ns = 100000;   // PWM信号周期，单位纳秒，也即频率为 1 / (100000 * 10^-9) = 10kHz
                                         // 但注释写的是20kHz，可能是误写，应为1 / 100000ns = 10kHz，


    target = readDoubleFromFile(target_file); // 读取目标速度

    // 初始化两个电机控制器对象
    for (int i = 0; i < 2; ++i)
    {
        motorController[i] = new MotorController(pwmchip[i], pwmnum[i], gpioNum[i], period_ns, duty_cycle_ns,
                                               mortor_kp, mortor_ki, mortor_kd, target,
                                               encoder_pwmchip[i], encoder_gpioNum[i], encoder_dir[i]);
    }
}

// ==========================
// 判断挡板位置函数（可拓展传感器逻辑）
// 目前简单实现：左右交替避让
// ==========================
void DetermineBarrierPosition() {
    last_barrier_position = (last_barrier_position == 1) ? -1 : 1;
}

// ==========================
// 避障逻辑函数，实现状态机
// ==========================
void AvoidObstacle() {
    auto current_time = std::chrono::steady_clock::now(); // 当前时刻
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - avoidance_start_time).count(); // 经过时间

    switch (avoidance_state) 
    {
        case DETECTING:
            // 确定挡板位置，开始后退
            DetermineBarrierPosition();
            avoidance_state = BACKING;
            avoidance_start_time = current_time;
            break;

        case BACKING:
            if (elapsed < AVOIDANCE_DURATION / 3) 
            {
                // 后退阶段：两个轮子负速转动
                motorController[0]->updateduty(-AVOIDANCE_SPEED);
                motorController[1]->updateduty(-AVOIDANCE_SPEED);
            } else {
                // 后退完成，切换为转向阶段
                avoidance_state = TURNING;
                avoidance_start_time = current_time;
            }
            break;

        case TURNING:
            if (elapsed < AVOIDANCE_DURATION * 2 / 3) 
            {
                // 转向阶段，根据障碍物位置决定方向
                if (last_barrier_position == -1) 
                {
                    // 障碍物在左侧，向右转
                    motorController[0]->updateduty(AVOIDANCE_SPEED + TURN_ANGLE);
                    motorController[1]->updateduty(AVOIDANCE_SPEED - TURN_ANGLE);
                } else 
                {
                    // 障碍物在右侧，向左转
                    motorController[0]->updateduty(AVOIDANCE_SPEED - TURN_ANGLE);
                    motorController[1]->updateduty(AVOIDANCE_SPEED + TURN_ANGLE);
                }
            } else 
            {
                // 转向完成，更新状态到避障完成
                avoidance_state = COMPLETED;
            }
            break;

        case COMPLETED:
            // 避障完成，延迟一段时间后重置状态机
            if (elapsed > AVOIDANCE_DURATION) {
                avoidance_state = NO_AVOIDANCE;
            }
            break;

        default:
            break;
    }
}

// ==========================
// 主要控制函数，每个控制周期调用
// ==========================
void ControlMain()
{
    // 调试输出当前测距值
    std::cout << "latest_range_mm : " << latest_range_mm << std::endl;

    // 如果检测到障碍物且距离小于阈值，启动避障状态机
    if (latest_range_mm > 0 && latest_range_mm < OBSTACLE_DISTANCE) 
    {
        if (avoidance_state == NO_AVOIDANCE) {
            avoidance_state = DETECTING;
            avoidance_start_time = std::chrono::steady_clock::now();
        }
    }

    if (avoidance_state != NO_AVOIDANCE) 
    {
        // 避障状态机运行
        AvoidObstacle();
    }
    else if (readFlag(start_file)) 
    {
        if(sign == 1)
        {
            target_stop_speed = 0;
            // 停车状态，应用刹车PID控制器降低速度至0
            for (int i = 0; i < 2; ++i) 
            {
                double edcoder_speed = -motorController[i]->encoderSpeed();  // 编码器当前速度
                double speed_error = target_stop_speed - edcoder_speed;     // 速度误差

                BrakePID.setPID(stop_kp, stop_ki, stop_kd); // 更新刹车PID参数
                double duty_stop = BrakePID.updatemortor(speed_error); // 计算刹车PWM占空比
                
                motorController[i]->updateduty(duty_stop);  // 应用控制输出 
            }
        }

        // 以下代码为注释掉的停车和正常驾驶PID控制逻辑保留

        
        if(stop_sign == 1 && banmaxian_num  == 2)
        {
            target_stop_speed = 0;
            for (int i = 0; i < 2; ++i) 
            {
                double edcoder_speed = -motorController[i]->encoderSpeed();
                double speed_error = target_stop_speed - edcoder_speed;

                BrakePID.setPID(stop_kp, stop_ki, stop_kd);
                double duty_stop_white = BrakePID.updatemortor(speed_error);
                
                motorController[i]->updateduty(duty_stop_white);
            }
        }

        if (readFlag(start_file)) {
            // 正常行驶控制（PID）
            for (int i = 0; i < 2; ++i) {
                motorController[i]->updateduty(target_speed);
            }
            mortorEN.setValue(1);

            for (int i = 0; i < 2; ++i) {
                double current_speed = motorController[i]->getCurrentSpeed();
                double edcoder_speed = -motorController[i]->encoderSpeed();
                double speed_error = target_speed - edcoder_speed;

                SpeedPID.setPID(mortor_kp, mortor_ki, mortor_kd);
                double duty_adjustment = SpeedPID.update(speed_error);
                motorController[i]->updateduty(15);
            }
            mortorEN.setValue(1);
        }
        
    }
    else {
        // 停止状况：设置两个电机PWM为0，并关闭使能
        for (int i = 0; i < 2; ++i) {
            motorController[i]->updateduty(0);
        }
        mortorEN.setValue(0);
    }
}

// ==========================
// 控制模块资源释放函数
// ==========================
void ControlExit()
{
    for (int i = 0; i < 2; ++i) {
        delete motorController[i];  // 释放电机控制器指针
        std::cout << "motor" << i << "deleted\n";
    }
    mortorEN.setValue(0);          // 关闭电机使能
}

// ==========================
// 更新电机速度函数（调用编码器采样）
// ==========================
void motorcontrol()
{
    motorController[0]->updateSpeed();
    motorController[1]->updateSpeed();
}

// ==========================
// 滤波器函数：用于舵机控制值的平滑处理
// ==========================
double Turn_Out_Filter(float turn_out)    
{
    float Turn_Out_Filtered = 0;  
    /* 
     * 数组Pre1_Error用于存储历史的舵机输出值，
     * 通过加权平均减少抖动，
     * 初始为舵机中间位置防止起步抖动
     */
    static float Pre1_Error[6] = {SERVO_MID_DUTY ,SERVO_MID_DUTY ,SERVO_MID_DUTY ,
                                 SERVO_MID_DUTY ,SERVO_MID_DUTY ,SERVO_MID_DUTY };

    // 更新历史值数组（后移）
    Pre1_Error[5] = Pre1_Error[4];
    Pre1_Error[4] = Pre1_Error[3];
    Pre1_Error[3] = Pre1_Error[2];
    Pre1_Error[2] = Pre1_Error[1];
    Pre1_Error[1] = Pre1_Error[0];
    Pre1_Error[0] = turn_out;  // 当前舵机输入值

    // 计算加权平滑输出
    Turn_Out_Filtered = Pre1_Error[0]*0.2 + Pre1_Error[1]*0.2 + Pre1_Error[2]*0.2 + 
                        Pre1_Error[3]*0.2 + Pre1_Error[4]*0.1 + Pre1_Error[5]*0.1;

    return Turn_Out_Filtered;  // 返回滤波后的舵机控制值
}

