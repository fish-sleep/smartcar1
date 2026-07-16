#include "PwmController.h"
#include <iostream>
#include <string>

int main() {
    int pwmchip, pwmnum;

    // 选择 PWM 芯片
    std::cout << "Enter PWM chip number (e.g., 0 for pwmchip0): ";
    std::cin >> pwmchip;

    // 选择 PWM 设备编号
    std::cout << "Enter PWM number (e.g., 0 for pwm0): ";
    std::cin >> pwmnum;

    // 创建PWM控制器实例
    PwmController pwm(pwmchip, pwmnum);

    // 初始化PWM设备
    if (!pwm.initialize()) {
        std::cerr << "Failed to initialize PWM." << std::endl;
        return -1;
    }

    // 启用PWM
    if (!pwm.enable()) {
        std::cerr << "Failed to enable PWM." << std::endl;
        return -1;
    }

    // 循环测试程序
    std::string input;
    while (true) {
        unsigned int period_ns = 0;
        unsigned int duty_cycle_ns = 0;

        std::cout << "\nEnter PWM period (in nanoseconds, or 'q' to quit): ";
        std::cin >> input;
        if (input == "q") {
            break;  // 用户输入'q'，退出循环
        }
        period_ns = std::stoul(input);

        std::cout << "Enter PWM duty cycle (in nanoseconds): ";
        std::cin >> duty_cycle_ns;

        if (duty_cycle_ns > period_ns) {
            std::cerr << "Error: Duty cycle cannot be greater than period!" << std::endl;
            continue;  // 返回输入
        }

        // 设置周期
        if (!pwm.setPeriod(period_ns)) {
            std::cerr << "Failed to set PWM period." << std::endl;
            return -1;
        }

        // 设置占空比
        if (!pwm.setDutyCycle(duty_cycle_ns)) {
            std::cerr << "Failed to set PWM duty cycle." << std::endl;
            return -1;
        }

        std::cout << "PWM updated: Period = " << period_ns 
                  << " ns, Duty Cycle = " << duty_cycle_ns << " ns" << std::endl;
    }

    // 退出前禁用PWM
    if (!pwm.disable()) {
        std::cerr << "Failed to disable PWM." << std::endl;
        return -1;
    }

    std::cout << "PWM disabled. Exiting program." << std::endl;
    return 0;
}
