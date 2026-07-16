/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2024-10-10 09:02:10
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2025-03-21 10:40:10
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

MotorController *motorController[2] = {nullptr, nullptr};
GPIO mortorEN(73);
PidObject SpeedPID;  // 速度环（增量式）
static PidObject BrakePID; // 刹车PID控制器

int target = 0;
double last_duty = 0; 
int target_stop_speed = 0;

// 赛道和挡板参数
const int TRACK_WIDTH = 900;       // 赛道宽度90cm(mm)
const int BARRIER_WIDTH = 400;     // 挡板宽度40cm(mm)
const int BARRIER_GAP = 900;       // 挡板间距90cm(mm)
const int OBSTACLE_DISTANCE = 1000; // 检测距离阈值50cm(mm)

// 避障参数
const int AVOIDANCE_DURATION = 1500; // 避障动作持续时间(ms)
const int AVOIDANCE_SPEED = 40;      // 避障时的速度
const int TURN_ANGLE = 30;           // 转向角度(0-100)

// 避障状态
enum AvoidanceState {
    NO_AVOIDANCE,
    DETECTING,
    BACKING,
    TURNING,
    COMPLETED
};

static AvoidanceState avoidance_state = NO_AVOIDANCE;
static std::chrono::steady_clock::time_point avoidance_start_time;
static int last_barrier_position = 0; // 0:未知, -1:左侧, 1:右侧

void ControlInit()
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
    const unsigned int period_ns = 100000; // 20 kHz
    target = readDoubleFromFile(target_file);

    for (int i = 0; i < 2; ++i)
    {
        motorController[i] = new MotorController(pwmchip[i], pwmnum[i], gpioNum[i], period_ns, duty_cycle_ns,
                                               mortor_kp, mortor_ki, mortor_kd, target,
                                               encoder_pwmchip[i], encoder_gpioNum[i], encoder_dir[i]);
    }
}

void DetermineBarrierPosition() {
    // 这里可以添加额外的传感器逻辑来判断挡板位置
    // 目前简单实现: 交替选择左右避障方向
    last_barrier_position = (last_barrier_position == 1) ? -1 : 1;
}

void AvoidObstacle() {
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - avoidance_start_time).count();

    switch (avoidance_state) 
    {
        case DETECTING:
            // 确定挡板位置并开始后退
            DetermineBarrierPosition();
            avoidance_state = BACKING;
            avoidance_start_time = current_time;
            break;

        case BACKING:
            if (elapsed < AVOIDANCE_DURATION / 3) 
            {
                // 后退阶段
                motorController[0]->updateduty(-AVOIDANCE_SPEED);
                motorController[1]->updateduty(-AVOIDANCE_SPEED);
            } else {
                // 后退完成，开始转向
                avoidance_state = TURNING;
                avoidance_start_time = current_time;
            }
            break;

        case TURNING:
            if (elapsed < AVOIDANCE_DURATION * 2 / 3) 
            {
                // 转向阶段
                if (last_barrier_position == -1) 
                {
                    // 左侧挡板，向右转
                    motorController[0]->updateduty(AVOIDANCE_SPEED + TURN_ANGLE);
                    motorController[1]->updateduty(AVOIDANCE_SPEED - TURN_ANGLE);
                } else 
                {
                    // 右侧挡板，向左转
                    motorController[0]->updateduty(AVOIDANCE_SPEED - TURN_ANGLE);
                    motorController[1]->updateduty(AVOIDANCE_SPEED + TURN_ANGLE);
                }
            } else 
            {
                // 避障完成
                avoidance_state = COMPLETED;
            }
            break;
        case COMPLETED:
            // 短暂延迟后重置状态
            if (elapsed > AVOIDANCE_DURATION) {
                avoidance_state = NO_AVOIDANCE;
            }
            break;

        default:
            break;
    }
}

void ControlMain()
{
     // 获取两个电机的当前速度
    //  double current_speed_left = motorController[0]->getCurrentSpeed();
    //  double current_speed_right = motorController[1]->getCurrentSpeed();
    //  double avg_speed = (current_speed_left + current_speed_right) / 2.0;
    // 检查是否有障碍物
    std::cout << "latest_range_mm : " << latest_range_mm << std::endl;
    if (latest_range_mm > 0 && latest_range_mm < OBSTACLE_DISTANCE) 
    {
        if (avoidance_state == NO_AVOIDANCE) {
            avoidance_state = DETECTING;
            avoidance_start_time = std::chrono::steady_clock::now();
        }
    }

    if (avoidance_state != NO_AVOIDANCE) 
    {
        // 执行避障动作
        AvoidObstacle();
    }
    else if (readFlag(start_file)) 
    {
        if(sign == 1)
        {
            target_stop_speed = 0;
            // 使用PID控制每个电机（示例）
            for (int i = 0; i < 2; ++i) 
            {
                // motorController[i]->updateduty(0);
            
                double edcoder_speed = -motorController[i]->encoderSpeed();
                double speed_error = target_stop_speed - edcoder_speed;

                BrakePID.kP = stop_kp;
                BrakePID.kI = stop_ki;
                BrakePID.kD = stop_kd;
                BrakePID.targetVal = target_stop_speed;
                BrakePID.measuredVal = edcoder_speed;
                BrakePID.isOutputEnabled = true;
                double duty_stop = BrakePID.pidCalculate();
            
                motorController[i]->updateduty(duty_stop);
            }
            
        }
        else
        {
            // 正常行驶 - 使用PID控制
            target_speed = readDoubleFromFile(speed_file);
            for (int i = 0; i < 2; ++i) 
            {
                double edcoder_speed = -motorController[i]->encoderSpeed();
                double speed_error = target_speed - edcoder_speed;

                SpeedPID.kP = mortor_kp;
                SpeedPID.kI = mortor_ki;
                SpeedPID.kD = mortor_kd;
                SpeedPID.targetVal = target_speed;
                SpeedPID.measuredVal = edcoder_speed;
                SpeedPID.isOutputEnabled = true;
                double duty_adjustment = SpeedPID.pidCalculate();
            
                motorController[i]->updateduty(duty_adjustment);
                
                std::cout << "Motor" << i << " encoder_speed: " << edcoder_speed 
                          << " error: " << speed_error 
                          << " duty: " << duty_adjustment << std::endl;
            }
        }

        // if(stop_sign == 1 && banmaxian_num  == 2)
        // {
            // target_stop_speed = 0;
            // // 使用PID控制每个电机（示例）
            // for (int i = 0; i < 2; ++i) 
            // {
            //     // motorController[i]->updateduty(0);
            
            //     double edcoder_speed = -motorController[i]->encoderSpeed();
            //     double speed_error = target_stop_speed - edcoder_speed;

            //     BrakePID.setPID(stop_kp, stop_ki, stop_kd);
            //     // SpeedPID.setPID(mortor_kp, mortor_ki, mortor_kd);
            //     // PID控制
            //     double duty_stop_white = BrakePID.updatemortor(speed_error);
            
            //     motorController[i]->updateduty(duty_stop_white);
            // }
        // }

        // if (readFlag(start_file)) {
            // 正常行驶
            // for (int i = 0; i < 2; ++i) {
            //     motorController[i]->updateduty(target_speed);
            //     // motorController[i]->updateSpeed();
            // }
            // mortorEN.setValue(1);
        // // 使用PID控制每个电机（示例）
        // for (int i = 0; i < 2; ++i) {
        //     double current_speed = motorController[i]->getCurrentSpeed();
            
        //     double edcoder_speed = -motorController[i]->encoderSpeed();
        //     double speed_error = target_speed - edcoder_speed;

        //     SpeedPID.setPID(mortor_kp, mortor_ki, mortor_kd);
        //     // PID控制
        //     double duty_adjustment = SpeedPID.update(speed_error);
            
        //     // motorController[i]->updateduty(duty_adjustment);
        //     motorController[i]->updateduty(15);
        //     // std::cout << "current_speed : " << current_speed << std::endl;
        //     // std::cout << "edcoder_speed : " << edcoder_speed << std::endl;
        //     // // std::cout << "last_duty : " << last_duty << std::endl;
        //     // std::cout << "duty_adjustment : " << duty_adjustment << std::endl;
        // }
        // mortorEN.setValue(1);
    }
    else {
        // 停止
        for (int i = 0; i < 2; ++i) {
            motorController[i]->updateduty(0);
        }
        mortorEN.setValue(0);
    }
}

void ControlExit()
{
    for (int i = 0; i < 2; ++i) {
        delete motorController[i];
        std::cout << "motor" << i << "deleted\n";
    }
    mortorEN.setValue(0);
}

void motorcontrol()
{
    motorController[0]->updateSpeed();
    motorController[1]->updateSpeed();
}

/*滤波器，滤的是舵机的值*/
double Turn_Out_Filter(float turn_out)    
{
  float Turn_Out_Filtered = 0;  /*像下面这行给Pre1_Error数组赋初值是为了防止车起步时舵机抖一下*/
  
  static float Pre1_Error[6] = {SERVO_MID_DUTY ,SERVO_MID_DUTY ,SERVO_MID_DUTY ,SERVO_MID_DUTY ,SERVO_MID_DUTY ,SERVO_MID_DUTY };
  Pre1_Error[5] = Pre1_Error[4];
  Pre1_Error[4] = Pre1_Error[3];
  Pre1_Error[3] = Pre1_Error[2];
  Pre1_Error[2] = Pre1_Error[1];
  Pre1_Error[1] = Pre1_Error[0];
  Pre1_Error[0] = turn_out;
  Turn_Out_Filtered = Pre1_Error[0]*0.2+Pre1_Error[1]*0.2+Pre1_Error[2]*0.2+Pre1_Error[3]*0.2+Pre1_Error[4]*0.1+Pre1_Error[5]*0.1;
  return Turn_Out_Filtered;
}

