/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2025-04-05 08:24:48
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2025-05-31 18:12:27
 * @FilePath: /smartcar/lib/vl53l0x.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <atomic>
#include <cstdint>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "vl53l0x_def.h"

#define MODE_RANGE 0
#define MODE_XTAKCALIB 1
#define MODE_OFFCALIB 2
#define MODE_HELP 3
#define MODE_PARAMETER 6

//******************************** IOCTL definitions
#define VL53L0X_IOCTL_INIT _IO('p', 0x01)
#define VL53L0X_IOCTL_XTALKCALB _IOW('p', 0x02, unsigned int)
#define VL53L0X_IOCTL_OFFCALB _IOW('p', 0x03, unsigned int)
#define VL53L0X_IOCTL_STOP _IO('p', 0x05)
#define VL53L0X_IOCTL_SETXTALK _IOW('p', 0x06, unsigned int)
#define VL53L0X_IOCTL_SETOFFSET _IOW('p', 0x07, int8_t)
#define VL53L0X_IOCTL_GETDATAS _IOR('p', 0x0b, VL53L0X_RangingMeasurementData_t)
#define VL53L0X_IOCTL_PARAMETER _IOWR('p', 0x0d, struct stmvl53l0x_parameter)

// modify the following macro accoring to testing set up
#define OFFSET_TARGET 100 // 200
#define XTALK_TARGET 600 // 400
#define NUM_SAMPLES 20 // 20

typedef enum {
    OFFSET_PAR = 0,
    XTALKRATE_PAR = 1,
    XTALKENABLE_PAR = 2,
    GPIOFUNC_PAR = 3,
    LOWTHRESH_PAR = 4,
    HIGHTHRESH_PAR = 5,
    DEVICEMODE_PAR = 6,
    INTERMEASUREMENT_PAR = 7,
    REFERENCESPADS_PAR = 8,
    REFCALIBRATION_PAR = 9,
} parameter_name_e;
/*
 *  IOCTL parameter structs
 */
struct stmvl53l0x_parameter {
    uint32_t is_read; // 1: Get 0: Set
    parameter_name_e name;
    int32_t value;
    int32_t value2;
    int32_t status;
};

extern int vl53l0xInit();
extern int vl53l0xGetData(int fd, VL53L0X_RangingMeasurementData_t* data);
extern int vl53l0xStop();

extern std::atomic<uint16_t> latest_range_mm;