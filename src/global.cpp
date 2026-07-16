#include "global.h"

double target_speed;

double PidObject::valLimit(double val, double min, double max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

double PidObject::pidCalculate(void) {
    prevError = currError;
    currError = targetVal - measuredVal;

    if (isErrorLimitEnabled) {
        currError = valLimit(currError, errorLimit[0], errorLimit[1]);
    }

    if (isFirstOrderFilterEnabled) {
        currError = currError * (1 - filterParam) + prevError * filterParam;
    }

    errorDeriv = currError - prevError;

    if (isIntegLimitEnabled) {
        errorInteg += currError;
        errorInteg = valLimit(errorInteg, integLimit[0], integLimit[1]);
    } else {
        errorInteg += currError;
    }

    double output = kP * currError + kI * errorInteg + kD * errorDeriv;

    if (!isPolOfMeaValCsstWithOutVal) {
        output = -output;
    }

    if (isOutputEnabled) {
        outputVal = output;
    } else {
        outputVal = 0;
    }

    return outputVal;
}

// 从文件读取双精度值
double readDoubleFromFile(const std::string &filename)
{
    std::ifstream file(filename);
    double value = 0.0;
    if (file.is_open())
    {
        file >> value; // 读取文件中的值
        file.close();
    }
    else
    {
        std::cerr << "Failed to open " << filename << std::endl;
    }
    return value;
}

// 从文件中读取标志
bool readFlag(const std::string &filename)
{
    std::ifstream file(filename);
    int flag = 0;
    if (file.is_open())
    {
        file >> flag; // 读取文件中的更新标志
        file.close();
    }
    else
    {
        std::cerr << "Failed to open " << filename << std::endl;
    }
    return flag;
}
