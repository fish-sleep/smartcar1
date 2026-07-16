/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2025-05-29 15:49:40
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2025-06-15 13:34:16
 * @FilePath: \smartcar1\lib\global.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef GLOBAL_H
#define GLOBAL_H

#include <string>
#include <iostream>
#include <fstream>
#include <atomic>

// const std::string kp_file = "./kp";
// const std::string ki_file = "./ki";
// const std::string kd_file = "./kd";

// const std::string servo_median_file = "./servo_median";

// const std::string mortor_kp_file = "./mortor_kp";
// const std::string mortor_ki_file = "./mortor_ki";
// const std::string mortor_kd_file = "./mortor_kd";

// const std::string stop_kp_file = "./stop_kp";
// const std::string stop_ki_file = "./stop_ki";
// const std::string stop_kd_file = "./stop_kd";

// const std::string start_file = "./start";
// const std::string showImg_file = "./showImg";
// const std::string destfps_file = "./destfps";
// const std::string foresee_file = "./foresee";
// const std::string saveImg_file = "./saveImg";
// const std::string speed_file = "./speed";
// const std::string servo_mid_file = "./servo_mid";
// const std::string limit_file = "./limit";
// const std::string target_file = "./target";

// 从文件读取双精度值
double readDoubleFromFile(const std::string &filename);

// 从文件中读取标志
bool readFlag(const std::string &filename);

extern std::atomic<double> PID_rotate;

extern double target_speed;

class PidObject {
public:
    bool isPolOfMeaValCsstWithOutVal = false; // Is the polarity of measured value consistent with output value? 测量值和输出值的极性是否一致
    // 比如测量值相对于目标值为负方向时，需要一个负数的输出值才能纠正，此时 isPolOfMeaValCsstWithOutVal 为 true
    double kP = 0; // Proportional Gain 比例增益
    double kI = 0; // Proportional Gain 比例增益
    double kD = 0; // Proportional Gain 比例增益


    double targetVal = 0; // Target Value 目标值
    double measuredVal = 0; // Measured Value 测量值
    bool isErrorLimitEnabled = false; // Is the error limit enabled? 是否启用误差限幅
    // 如果启用，则 currError = valLimit(currError, errorLimit[0], errorLimit[1])
    double errorLimit[2] = {0, 0}; // Error Limit 误差限幅，[0]是最小值，[1]是最大值
    bool isFirstOrderFilterEnabled = false; // Is the first order filter enabled? 一阶滤波器是否启用
    // 如果启用，则 currError = currError * (1 - filterParam) + prevError * filterParam
    double filterParam = 0;  // 上次误差 prevError 的权重，用于一阶滤波
    double currError = 0; // Current Error 当前误差
    double prevError = 0; // Previous Error 上次误差
    
    bool isIntegLimitEnabled = false; // Is the integral limit enabled? 是否启用积分限幅
    // 如果启用，则 errorInteg = valLimit(errorInteg, integLimit[0], integLimit[1])
    double integLimit[2] = {0, 0}; // Integral Limit 积分限幅，[0]是最小值，[1]是最大值
    double errorInteg = 0; // Error Integral 误差积分
    double errorDeriv = 0; // Error Derivative 误差微分

    bool isOutputEnabled = false; // Is the output enabled? 是否启用输出
    // 如果启用，则 outputVal = pidCalculate()，否则 outputVal = 0
    double outputVal = 0; // Output Value 输出值，调用 pidCalculate 方法后，此数值将会更新
    double pidCalculate(void); // 根据给定的PID对象，计算PID控制器的输出值。
    double valLimit(double val, double min, double max); // 限制val在[min,max]之间
};


#endif

