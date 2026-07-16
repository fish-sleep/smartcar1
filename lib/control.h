/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2025-05-29 15:49:40
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2025-05-31 15:44:55
 * @FilePath: \smartcar1\lib\control.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef CONTROL_H
#define CONTROL_H

#include <unistd.h>
#include <chrono>
#include <iostream>

#include "MotorController.h"
#include "global.h"
#include "serial.h"
#include "GPIO.h"
#include "camera.h"
#include "encoder.h"

#define SERVO_MID_DUTY  1500000

extern MotorController* motorController[2]; // 声明全局变量
extern PidObject SpeedPID;
extern int target;
extern double last_duty;
extern int target_stop_speed;

void ControlInit();
void ControlMain();
void ControlExit();
void DetermineBarrierPosition();
void AvoidObstacle();
void motorcontrol();
double Turn_Out_Filter(float turn_out);



#endif