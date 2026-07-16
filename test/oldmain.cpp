#include <opencv2/opencv.hpp>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include "PwmController.h"
#include "PIDController.h"
#include "image.h"

using namespace cv;
using namespace std;
using namespace chrono;

uint16_t convertRGBToRGB565(uint8_t r, uint8_t g, uint8_t b);
void convertMatToRGB565(const Mat &frame, uint16_t *buffer, int width, int height);
PwmController Mortor1(0, 0), Mortor2(1, 0), Servo(2, 0), Buzzer(3, 0);
PIDController ServoControl(1.0, 0.0, 2.0, 0.0, POSITION, 1250000);
void drawTextWithBorder(Mat &image, const std::string &text, Point position, 
                        int fontFace, double fontScale, Scalar textColor, 
                        Scalar borderColor, int thicknessText, 
                        int thicknessBorder);

std::string kp_file = "./kp";
std::string ki_file = "./ki";
std::string kd_file = "./kd";
std::string updateSetting_file = "./updateSetting";
std::string start_file = "./start";
std::string showImg_file = "./showImg";
std::string destfps_file = "./destfps";
std::string foresee_file = "./foresee";
std::string saveImg_file = "./saveImg";
std::string speed_file = "./speed";
std::string imgProcess_file = "./imageProcess";
std::string binarize_blocksize_file = "./blocksize";
std::string binarize_C_file = "./C";

int foresee;
double speed[2];
int binarize_blocksize, binarize_C;

void pwminit();
void update_data();

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

int saved_frame_count = 0;
bool saveCameraImage(cv::Mat frame, const std::string &directory)
{
    if (frame.empty())
    {
        std::cerr << "Save Error: Frame is empty." << std::endl;
        return 0;
    }

    // 构建文件名
    std::ostringstream filename;
    filename << directory << "/image_" << std::setw(5) << std::setfill('0') << saved_frame_count << ".jpg";

    saved_frame_count++;
    // 保存图像
    return cv::imwrite(filename.str(), frame);
}

int main()
{
    pwminit();

    // 打开默认摄像头（设备编号 0）
    cv::VideoCapture cap(0);

    // 检查摄像头是否成功打开
    if (!cap.isOpened())
    {
        printf("无法打开摄像头\n");
        return -1;
    }

    cap.set(CAP_PROP_FRAME_WIDTH, 320);      // 宽度
    cap.set(CAP_PROP_FRAME_HEIGHT, 240);     // 高度
    cap.set(cv::CAP_PROP_AUTO_EXPOSURE, -1); // 设置自动曝光

    double dest_frame_duration;
    { // 计算帧率
        double dest_fps = readDoubleFromFile(destfps_file);
        double fps = cap.get(cv::CAP_PROP_FPS);
        printf("Camera fps:%lf\n", fps);

        // 计算每帧的延迟时间（us）
        dest_frame_duration = static_cast<double>(1000000 / min(fps, dest_fps));
    }

    // 获取帧缓冲区设备信息
    int fb = open("/dev/fb1", O_RDWR);
    if (fb == -1)
    {
        cerr << "无法打开帧缓冲区设备" << endl;
        return -1;
    }

    struct fb_var_screeninfo vinfo;
    ioctl(fb, FBIOGET_VSCREENINFO, &vinfo);

    // 设置屏幕参数
    int screenWidth = 160;
    int screenHeight = 128;

    // 创建帧缓冲区
    uint16_t *fb_buffer = new uint16_t[screenWidth * screenHeight];

    Mat frame, resizedFrame, grayFrame, binarizedFrame;
    Mat fbImage(screenHeight, screenWidth, CV_8UC3, Scalar(0, 0, 0)); // 帧缓冲区图像

    auto frame_start_time = high_resolution_clock::now();
    auto frame_end_time = high_resolution_clock::now();
    auto frame_end_time1 = high_resolution_clock::now();
    auto frame_start_time1 = high_resolution_clock::now();
    std::chrono::duration<double> frame_duration;

    int newWidth = 160, newHeight = 120;
    update_data();
    while (true)
    {
        // 检查是否启动车模
        while (!readFlag(start_file))
        {
            Mortor1.disable();
            Mortor2.disable();
            Servo.disable();
            Buzzer.disable();
            // 检查是否需要更新 PID 参数
            if (readFlag(updateSetting_file))
            {
                update_data();
            }
            usleep(20 * 1000); // 20ms
        }

        Mortor1.setDutyCycle(Mortor1.readPeriod() * (100 - speed[0]) / 100);
        Mortor2.setDutyCycle(Mortor2.readPeriod() * (100 - speed[1]) / 100);

        Mortor1.enable();
        Mortor2.enable();
        Servo.enable();
        // Buzzer.enable();

        { // 捕获图像
            frame_start_time1 = high_resolution_clock::now();

            cap.grab();
            cap.retrieve(frame);
            // 检查是否成功捕获图像
            if (frame.empty())
            {
                printf("无法捕获图像\n");
                return -1;
            }

            frame_end_time1 = std::chrono::high_resolution_clock::now();
            frame_duration = frame_end_time1 - frame_start_time1;
            // printf("Capture cost:%lfs\n", frame_duration);
        }

        // 采图
        if (readFlag(saveImg_file))
        {
            Mortor1.disable();
            Mortor2.disable();
            Servo.disable();
            Buzzer.disable();

            frame_start_time1 = high_resolution_clock::now();

            if (saveCameraImage(frame, "./image"))
            {
                printf("图像已保存\n");
            }
            else
            {
                printf("图像保存失败\n");
            }

            frame_end_time1 = std::chrono::high_resolution_clock::now();
            frame_duration = frame_end_time1 - frame_start_time1;
            // printf("Saveimg cost:%lfs\n", frame_duration);
        }
        double servoduty, servoduty_ns;
        if (readFlag(imgProcess_file))
        {
            { // 图像计算
                frame_start_time1 = high_resolution_clock::now();

                resize(frame, resizedFrame, Size(160, 120));
                
                cv::cvtColor(resizedFrame, grayFrame, cv::COLOR_BGR2GRAY);
                cv::threshold(grayFrame, binarizedFrame, 0, 255, cv::THRESH_OTSU | cv::THRESH_BINARY);

                memcpy(IMG, binarizedFrame.data, 160 * 120);
                image_main();

                frame_end_time1 = std::chrono::high_resolution_clock::now();
                frame_duration = frame_end_time1 - frame_start_time1;
                // printf("Process cost:%lfs\n", frame_duration);
            }

            {
                frame_start_time1 = high_resolution_clock::now();
                
                if (mid_line[foresee] != 255) 
                {
                    servoduty = - ServoControl.update(mid_line[foresee] - CAMERA_W / 2);
                }
                servoduty = std::clamp(servoduty, -5.0, 5.0);
                servoduty_ns = (servoduty) / 100 * Servo.readPeriod() + 1570000;
                Servo.setDutyCycle(servoduty_ns);
                frame_end_time1 = std::chrono::high_resolution_clock::now();
                frame_duration = frame_end_time1 - frame_start_time1;
                // printf("Control cost:%lfs\n", frame_duration);
            }
        }

        // 显示图片
        if (readFlag(showImg_file))
        {
            frame_start_time1 = high_resolution_clock::now();

            // 缩放视频到新尺寸
            resize(binarizedFrame, resizedFrame, Size(newWidth, newHeight));
            // 将单通道的二值化图像转换为三通道的彩色图像
            Mat coloredResizedFrame;
            cvtColor(resizedFrame, coloredResizedFrame, COLOR_GRAY2BGR); // 转换为彩色图像

            // 将缩放后的图像居中放置在帧缓冲区图像中，填充黑色边框
            fbImage.setTo(Scalar(0, 0, 0)); // 清空缓冲区（填充黑色）
            Rect roi((screenWidth - newWidth) / 2, (screenHeight - newHeight) / 2, newWidth, newHeight);
            coloredResizedFrame.copyTo(fbImage(roi));

            // 绘制左右边界线和中线
            double scaleFactor = static_cast<double>(newHeight) / CAMERA_H;
            int scaledLeftX, scaledRightX, scaledMidX;

            for (int y = 0; y < CAMERA_H; y++)
            {
                // 根据缩放比例调整X坐标
                scaledLeftX = static_cast<int>(left_line[y] * scaleFactor);
                scaledRightX = static_cast<int>(right_line[y] * scaleFactor);
                scaledMidX = static_cast<int>(mid_line[y] * scaleFactor);

                // 绘制左边界（使用红色）
                line(fbImage(roi), Point(scaledLeftX, y), Point(scaledLeftX, y), Scalar(0, 0, 255), 2);

                // 绘制右边界（使用绿色）
                line(fbImage(roi), Point(scaledRightX, y), Point(scaledRightX, y), Scalar(0, 255, 0), 2);

                // 绘制中线（使用蓝色）
                line(fbImage(roi), Point(scaledMidX, y), Point(scaledMidX, y), Scalar(255, 0, 0), 2);
            }

            // 计算帧处理时间
            frame_duration = frame_end_time - frame_start_time;
            double frame_rate = 1.0 / frame_duration.count();
            // printf("Current fps:%lf\n", frame_rate);

            // 将帧率显示在左上角
            char text[32];
            sprintf(text, "FPS: %.2f", frame_rate);
            // 绘制带边框的文本
            drawTextWithBorder(fbImage, text, Point(5, 15), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(255, 255, 255), Scalar(0, 0, 0), 1, 3);
            
            sprintf(text, "DUTY: %.2f%%", servoduty);
            // 绘制带边框的文本
            drawTextWithBorder(fbImage, text, Point(5, 30), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(255, 255, 255), Scalar(0, 0, 0), 1, 3);
            
            // 将帧缓冲区图像转换为RGB565格式
            convertMatToRGB565(fbImage, fb_buffer, screenWidth, screenHeight);

            // 写入帧缓冲区
            lseek(fb, 0, SEEK_SET);
            write(fb, fb_buffer, screenWidth * screenHeight * 2);

            frame_end_time1 = std::chrono::high_resolution_clock::now();
            frame_duration = frame_end_time1 - frame_start_time1;
            // printf("Display cost:%lfs\n", frame_duration);
        }

        { // 帧率计算和自动帧率平衡
            frame_end_time1 = std::chrono::high_resolution_clock::now();
            frame_duration = frame_end_time1 - frame_end_time;

            // 等待一段时间，以模拟视频帧率
            useconds_t spare_time = std::max(0.0, dest_frame_duration - frame_duration.count() * 1000000);
            usleep(spare_time);
            printf("Spare time:  %lfs\n", spare_time / 1000000.0);

            // 计算每帧的开始时间
            frame_start_time = frame_end_time;
            frame_end_time = high_resolution_clock::now();
        }
    }
    return 0;
}

// 将RGB转换为RGB565格式
uint16_t convertRGBToRGB565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// 将Mat图像转换为RGB565格式
void convertMatToRGB565(const Mat &frame, uint16_t *buffer, int width, int height)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            Vec3b color = frame.at<Vec3b>(y, x);
            buffer[y * width + x] = convertRGBToRGB565(color[2], color[1], color[0]);
        }
    }
}

void pwminit()
{
    Mortor1.setPeriod(50000);                          // 50us  20kHz
    Mortor2.setPeriod(50000);                          // 50us  20kHz
    Servo.setPeriod(3000000);                          // 3ms   333Hz
    Buzzer.setPeriod(250000);                          // 250us 4000Hz

    Mortor1.setDutyCycle(Mortor1.readPeriod() * 1);     // 0%
    Mortor2.setDutyCycle(Mortor2.readPeriod() * 1);     // 0%
    Servo.setDutyCycle(1570000);       // 50%
    Buzzer.setDutyCycle(Buzzer.readPeriod() * 0.5);     // 50%

    Mortor1.enable();
    Mortor2.enable();
    Servo.enable();
    Buzzer.enable();

    usleep(1000000);
    Buzzer.disable();
}

void update_data()
{
    printf("Updating PID values...\n");

    // 从 kp、ki、kd 文件中读取新值
    double new_kp = readDoubleFromFile(kp_file);
    double new_ki = readDoubleFromFile(ki_file);
    double new_kd = readDoubleFromFile(kd_file);

    // 更新 PID 控制器中的参数
    ServoControl.setPID(new_kp, new_ki, new_kd);

    printf("New kp:%lf, ki:%lf, kd:%lf\n", new_kp, new_ki, new_kd);

    foresee = readDoubleFromFile(foresee_file);

    speed[0] = speed[1] = readDoubleFromFile(speed_file);

    binarize_blocksize = readDoubleFromFile(binarize_blocksize_file);
    binarize_C = readDoubleFromFile(binarize_C_file);
}

// 绘制带边框的文本
void drawTextWithBorder(Mat &image, const std::string &text, Point position, 
                        int fontFace, double fontScale, Scalar textColor, 
                        Scalar borderColor, int thicknessText = 1, 
                        int thicknessBorder = 3)
{
    // 先绘制边框
    putText(image, text, position, fontFace, fontScale, borderColor, thicknessBorder);
    // 再绘制文字
    putText(image, text, position, fontFace, fontScale, textColor, thicknessText);
}
