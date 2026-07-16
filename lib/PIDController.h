#ifndef PIDCONTROLLER_H
#define PIDCONTROLLER_H

#include <algorithm> // 用于 std::clamp

enum PIDMode
{
    POSITION,   // 位置式 PID
    INCREMENTAL // 增量式 PID
};

class PIDController
{
public:
    // 构造函数，初始化 PID 参数
    PIDController(double kp, double ki, double kd, double target, PIDMode mode = POSITION,
                  double output_limit = 100.0, double integral_limit = 100.0);

    // 更新 PID 控制器
    double updateservo(double measured_value);
    double updatemortor(double error);

    // 设置新的目标值
    void setTarget(double target);

    // 设置 PID 参数
    void setPID(double kp, double ki, double kd);

    // 设置 PID 控制模式（位置式或增量式）
    void setMode(PIDMode mode);

    // 设置输出和积分的限幅值
    void setLimits(double output_limit, double integral_limit);

private:
    // 位置式 PID 实现
    double positionPID(double error);

    // 增量式 PID 实现
    double incrementalPID(double error);

    double kp_;     // 比例增益
    double ki_;     // 积分增益
    double kd_;     // 微分增益
    double target_; // 目标值

    double prev_error_;      // 前一次误差
    double prev_prev_error_; // 前两次误差 (用于增量式)
    double integral_;        // 积分项
    double prev_output_;     // 前一次的控制器输出 (用于增量式)

    PIDMode mode_; // 控制模式 (位置式或增量式)

    double output_limit_;   // 输出限幅
    double integral_limit_; // 积分限幅
};

// class PID
//     {
//         public:
//             typedef enum
//             {
//                 INCREMENTAL,
//                 POSITIONAL
//             }PID_KIND;

//             float kp;
//             float ki;
//             float kd;
//             float outputLimit;
//             float iLimit;

//             PID(PID_KIND pidKind);
//             float getValue(float target, float feedback);
//         private:
//             PID_KIND _pidKind;
//             float _value;
//             float _delta;
//             float _nowErr;
//             float _lastErr;
//             float _lastLastErr;
//             float _sigmaErr;
//     };

//     extern PID PID_Servo;
//     extern PID PID_Motor_L;
//     extern PID PID_Motor_R;

#endif // PIDCONTROLLER_H
