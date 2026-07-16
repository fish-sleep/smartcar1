#include "camera.h"  // 包含摄像头相关的头文件
#include "GPIO.h"    // 包含GPIO控制相关的头文件

// 全局变量定义
cv::VideoCapture cap;  // OpenCV的视频捕获对象，用于摄像头操作
std::mutex speedMutex;  // 速度控制的互斥锁，用于线程安全
double kp = 0;  // PID控制器的比例系数
double ki = 0;  // PID控制器的积分系数
double kd = 0;  // PID控制器的微分系数
int screenWidth, screenHeight;  // 帧缓冲区的宽和高
int newWidth, newHeight;  // 调整后的图像宽和高
int fb;  // 帧缓冲区文件描述符
uint16_t *fb_buffer;  // 帧缓冲区内存映射指针
PwmController servo(1, 0);  // PWM控制器对象，用于控制舵机
bool streamCaptureRunning = false;  // 摄像头捕获线程运行标志
int saved_frame_count = 0;  // 保存的帧数计数
std::mutex frameMutex;  // 帧数据的互斥锁，用于线程安全
cv::Mat pubframe;  // 共享的帧数据，用于线程间传递

double mortor_kp = 0;  // 电机PID控制器的比例系数
double mortor_ki = 0.1;  // 电机PID控制器的积分系数
double mortor_kd = 0;  // 电机PID控制器的微分系数
double stop_kp = 0;  // 停车PID控制器的比例系数
double stop_ki = 0.1;  // 停车PID控制器的积分系数
double stop_kd = 0;  // 停车PID控制器的微分系数
int sign = 0;  // 停车标志
int stop_sign = 0;  // 停车标志（备用）
// int banmaxian_num = 0;  // 斑马线计数（旧变量）
double start_time = -15;  // 斑马线检测的开始时间

// 斑马线检测相关全局变量
std::atomic<int> banmaxian_num(0);  // 斑马线计数（原子变量，线程安全）
std::atomic<bool> is_stopped{false};  // 停车状态标志（原子变量，线程安全）
std::atomic<double> stop_start_time{0.0};  // 停车开始时间（原子变量，线程安全）
std::atomic<bool> last_zebra_detected{false};  // 上次检测结果（原子变量，线程安全）

// 在全局变量区域添加
std::atomic<double> zebra_cooldown_end{10.0};  // 冷却结束时间（原子变量，线程安全）

#define calc_scale 2  // 计算缩放比例的宏定义

// 斑马线检测函数
bool detectZebraCrossing(const cv::Mat& inputImage) {
    // 1. 参数定义
    const cv::Scalar lowerWhite(0, 0, 200);  // HSV白色下限
    const cv::Scalar upperWhite(180, 30, 255);  // HSV白色上限
    const int MIN_STRIPE_COUNT = 3;  // 最小条纹数
    const int MIN_AREA = 50;  // 最小区域面积
    const float MIN_ASPECT_RATIO = 3.0f;  // 最小长宽比
    const int MIN_WIDTH = 30;  // 最小宽度
    const float SPACING_VARIANCE = 0.3f;  // 最大间距变化率

    // 2. 转换为HSV颜色空间
    cv::Mat hsvImage;
    cvtColor(inputImage, hsvImage, cv::COLOR_BGR2HSV);  // 将输入图像从BGR转换为HSV

    // 3. 创建白色区域掩膜
    cv::Mat whiteMask;
    inRange(hsvImage, lowerWhite, upperWhite, whiteMask);  // 根据HSV范围创建掩膜

    // 4. 形态学处理 - 增强水平条纹
    cv::Mat kernelH = getStructuringElement(cv::MORPH_RECT, cv::Size(15, 1));  // 创建水平结构元素
    morphologyEx(whiteMask, whiteMask, cv::MORPH_CLOSE, kernelH);  // 形态学闭运算，增强水平条纹

    // 5. 查找轮廓
    std::vector<std::vector<cv::Point>> contours;
    findContours(whiteMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);  // 查找轮廓

    // 6. 斑马线特征检测
    int stripeCount = 0;  // 条纹计数
    std::vector<cv::Rect> stripeRects;  // 存储条纹的矩形区域

    for (const auto& contour : contours) {
        double area = contourArea(contour);  // 计算轮廓面积
        if (area < MIN_AREA) continue;  // 如果面积小于最小值，跳过

        cv::Rect rect = boundingRect(contour);  // 获取轮廓的边界矩形
        float aspectRatio = (float)rect.width / rect.height;  // 计算长宽比

        // 检查是否为长条形(宽度远大于高度)
        if (aspectRatio > MIN_ASPECT_RATIO && rect.width > MIN_WIDTH) {
            stripeCount++;  // 增加条纹计数
            stripeRects.push_back(rect);  // 存储条纹矩形
        }
    }

    // 7. 检查条纹特征
    if (stripeCount >= MIN_STRIPE_COUNT) {
        // 按y坐标排序
        std::sort(stripeRects.begin(), stripeRects.end(), 
            [](const cv::Rect& a, const cv::Rect& b) { return a.y < b.y; });

        // 计算平均间距
        float avgGap = 0;
        for (size_t i = 1; i < stripeRects.size(); ++i) {
            avgGap += (stripeRects[i].y - stripeRects[i-1].y);  // 累加间距
        }
        avgGap /= (stripeRects.size() - 1);  // 计算平均间距

        // 检查间距均匀性
        bool isZebra = true;
        for (size_t i = 1; i < stripeRects.size(); ++i) {
            float gap = stripeRects[i].y - stripeRects[i-1].y;  // 当前间距
            if (fabs(gap - avgGap) > avgGap * SPACING_VARIANCE) {  // 如果间距变化超过阈值
                isZebra = false;  // 不是斑马线
                break;
            }
        }

        return isZebra;  // 返回检测结果
    }

    return false;  // 如果条纹数不足，返回false
}

// 在CameraHandler函数中的斑马线处理逻辑
void handleZebraCrossing(const cv::Mat& frame) {
    // 1. 检测斑马线
    bool current_zebra = detectZebraCrossing(frame);  // 当前帧是否检测到斑马线
    bool new_zebra_detected = current_zebra && !last_zebra_detected.load();  // 是否新检测到斑马线
    const int STOP_TIME_SECONDS = 3;  // 停车时间(秒)

    // 2. 新检测到斑马线时的处理
    if (new_zebra_detected && !is_stopped) {  // 如果新检测到斑马线且未停车
        is_stopped = true;  // 设置停车标志
        stop_start_time = cv::getTickCount() / cv::getTickFrequency();  // 记录停车开始时间

        std::lock_guard<std::mutex> lock(speedMutex);  // 锁定速度互斥锁
        target_speed = 0.0;  // 设置目标速度为0
        sign = 1;  // 设置停车标志

        printf("[%s] 检测到斑马线，停车%d秒\n", 
              getCurrentTime().c_str(), STOP_TIME_SECONDS);  // 打印停车信息

        // 可选：触发语音提示
        int ret = wonderEchoSend(0xFF, 0x10);  // 发送语音提示
        if (ret != 0) {
            printf("语音发送失败，错误码：%d\n", ret);  // 打印错误信息
        }
    }

    // 3. 更新上次检测结果
    last_zebra_detected = current_zebra;  // 更新上次检测结果

    // 4. 停车超时恢复
    if (is_stopped) {  // 如果正在停车
        const double current_time = cv::getTickCount() / cv::getTickFrequency();  // 获取当前时间
        if (current_time - stop_start_time >= STOP_TIME_SECONDS) {  // 如果停车时间超过设定值
            is_stopped = false;  // 清除停车标志
            sign = 0;  // 清除停车标志
            banmaxian_num++;  // 增加斑马线计数

            const double new_speed = readDoubleFromFile(speed_file);  // 读取新的速度值
            {
                std::lock_guard<std::mutex> lock(speedMutex);  // 锁定速度互斥锁
                target_speed = new_speed;  // 设置目标速度
            }
            printf("[%s] 停车结束，恢复速度至%.1f\n", 
                  getCurrentTime().c_str(), new_speed);  // 打印恢复速度信息
        }
    }
}

// 摄像头初始化函数
int CameraInit(uint8_t camera_id, double dest_fps, int width, int height) {
    servo.setPeriod(3000000);  // 设置舵机周期
    servo.setDutyCycle(1500000);  // 设置舵机占空比
    servo.enable();  // 启用舵机

    fb = open("/dev/fb0", O_RDWR);  // 打开帧缓冲区设备
    if (fb == -1) {
        std::cerr << "无法打开帧缓冲区设备" << std::endl;  // 打印错误信息
        return -1;  // 返回错误码
    }

    struct fb_var_screeninfo vinfo;  // 帧缓冲区信息结构体
    if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) == -1) {  // 获取帧缓冲区信息
        std::cerr << "无法获取帧缓冲区信息" << std::endl;  // 打印错误信息
        close(fb);  // 关闭帧缓冲区设备
        return -1;  // 返回错误码
    }

    screenWidth = vinfo.xres;  // 获取屏幕宽度
    screenHeight = vinfo.yres;  // 获取屏幕高度
    size_t fb_size = vinfo.yres_virtual * vinfo.xres_virtual * vinfo.bits_per_pixel / 8;  // 计算帧缓冲区大小

    fb_buffer = (uint16_t *)mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);  // 映射帧缓冲区到内存
    if (fb_buffer == MAP_FAILED) {
        std::cerr << "无法映射帧缓冲区到内存" << std::endl;  // 打印错误信息
        close(fb);  // 关闭帧缓冲区设备
        return -1;  // 返回错误码
    }

    cap.open(camera_id);  // 打开摄像头
    if (!cap.isOpened()) {
        printf("无法打开摄像头\n");  // 打印错误信息
        munmap(fb_buffer, fb_size);  // 取消内存映射
        close(fb);  // 关闭帧缓冲区设备
        return -1;  // 返回错误码
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, width);  // 设置摄像头宽度
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);  // 设置摄像头高度
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));  // 设置编码格式
    cap.set(cv::CAP_PROP_AUTO_EXPOSURE, -1);  // 禁用自动曝光

    int cameraWidth = cap.get(cv::CAP_PROP_FRAME_WIDTH);  // 获取摄像头实际宽度
    int cameraHeight = cap.get(cv::CAP_PROP_FRAME_HEIGHT);  // 获取摄像头实际高度
    printf("摄像头分辨率: %d x %d\n", cameraWidth, cameraHeight);  // 打印摄像头分辨率

    double widthRatio = static_cast<double>(screenWidth) / cameraWidth;  // 计算宽度比例
    double heightRatio = static_cast<double>(screenHeight) / cameraHeight;  // 计算高度比例
    double scale = std::min(widthRatio, heightRatio);  // 取最小比例

    newWidth = static_cast<int>(cameraWidth * scale);  // 计算调整后的宽度
    newHeight = static_cast<int>(cameraHeight * scale);  // 计算调整后的高度
    printf("自适应分辨率: %d x %d\n", newWidth, newHeight);  // 打印调整后的分辨率

    double fps = cap.get(cv::CAP_PROP_FPS);  // 获取摄像头帧率
    printf("Camera fps:%lf\n", fps);  // 打印摄像头帧率

    line_tracking_width = newWidth / calc_scale;  // 计算车道线检测宽度
    line_tracking_height = newHeight / calc_scale;  // 计算车道线检测高度

    return static_cast<int>(1000.0 / std::min(fps, dest_fps));  // 返回目标帧间隔时间（ms）
}

// 摄像头反初始化函数
void cameraDeInit(void) {
    cap.release();  // 释放摄像头资源

    struct fb_var_screeninfo vinfo;  // 帧缓冲区信息结构体
    if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) == -1) {  // 获取帧缓冲区信息
        std::cerr << "无法获取帧缓冲区信息" << std::endl;  // 打印错误信息
    } else {
        size_t fb_size = vinfo.yres_virtual * vinfo.xres_virtual * vinfo.bits_per_pixel / 8;  // 计算帧缓冲区大小
        munmap(fb_buffer, fb_size);  // 取消内存映射
    }

    close(fb);  // 关闭帧缓冲区设备
}

// 保存摄像头图像的函数
bool saveCameraImage(const cv::Mat& frame, const std::string &directory) {
    if (frame.empty()) {
        std::cerr << "Save Error: Frame is empty." << std::endl;  // 打印错误信息
        return false;  // 返回失败
    }

    std::ostringstream filename;  // 创建文件名流
    filename << directory << "/image_" << std::setw(5) << std::setfill('0') << saved_frame_count << ".jpg";  // 构造文件名
    saved_frame_count++;  // 增加保存的帧数计数
    return cv::imwrite(filename.str(), frame);  // 保存图像并返回结果
}

// 摄像头捕获线程函数
void streamCapture(void) {
    cv::Mat frame;  // 创建帧变量
    while (streamCaptureRunning) {  // 捕获线程运行标志
        cap.read(frame);  // 从摄像头读取帧
        frameMutex.lock();  // 锁定帧互斥锁
        pubframe = frame;  // 更新共享帧
        frameMutex.unlock();  // 解锁帧互斥锁
    }
}

// PID控制器对象
PidObject ServoControl;  // 舵机PID控制器
// PIDController MotorControl(0, 0, 0, 0, INCREMENTAL, 1250000);  // 电机PID控制器（未启用）

// 摄像头处理函数
int CameraHandler(void) {
    cv::Mat resizedFrame;  // 创建调整大小后的帧变量
    static std::atomic<bool> is_stopped{false};  // 停车状态标志（原子变量）
    static std::atomic<double> stop_start_time{0.0};  // 停车开始时间（原子变量）
    static std::atomic<bool> last_zebra_detected{false};  // 上次检测结果（原子变量）

    // 帧率计算相关变量
    static double last_time = cv::getTickCount() / cv::getTickFrequency();  // 上次时间
    static int frame_count = 0;  // 帧计数
    static double fps = 0.0;  // 帧率

    frameMutex.lock();  // 锁定帧互斥锁
    raw_frame = pubframe.clone();  // 复制共享帧
    frameMutex.unlock();  // 解锁帧互斥锁

    if (raw_frame.empty()) {
        printf("无法捕获图像\n");  // 打印错误信息
        return -1;  // 返回错误码
    }

    // 帧率计算
    frame_count++;  // 增加帧计数
    if (frame_count >= 10) {  // 每10帧计算一次帧率
        double current_time = cv::getTickCount() / cv::getTickFrequency();  // 获取当前时间
        fps = frame_count / (current_time - last_time);  // 计算帧率
        printf("当前帧率: %.2f FPS\n", fps);  // 打印帧率
        frame_count = 0;  // 重置帧计数
        last_time = current_time;  // 更新上次时间
    }

    if (readFlag(saveImg_file)) {  // 如果需要保存图像
        if (saveCameraImage(raw_frame, std::string("./image"))) {  // 保存图像
            printf("图像%d已保存\n", saved_frame_count);  // 打印保存信息
        } else {
            printf("图像保存失败\n");  // 打印错误信息
            return -1;  // 返回错误码
        }
    }

    { image_main(); }  // 调用图像处理主函数

    // // 显示处理结果
    // if (readFlag(showImg_file)) {
    //     // 创建全黑背景的帧缓冲图像
    //     cv::Mat fbImage(screenHeight, screenWidth, CV_8UC3, cv::Scalar(0, 0, 0));
        
    //     // 尺寸调整和颜色空间转换
    //     cv::Mat processedFrame;
    //     cv::resize(track, processedFrame, cv::Size(newWidth, newHeight));  // 调整大小
    //     cv::cvtColor(processedFrame, processedFrame, cv::COLOR_GRAY2BGR);  // 转换颜色空间

    //     // 车道线绘制（在原始方向）
    //     const int lineThickness = calc_scale;  // 线宽
    //     for (int y = 0; y < line_tracking_height; ++y) {
    //         // 计算缩放后坐标
    //         const int scaledY = y * calc_scale;
    //         const int scaledLeft = left_line[y] * calc_scale;
    //         const int scaledRight = right_line[y] * calc_scale;
    //         const int scaledMid = mid_line[y] * calc_scale;

    //         // 绘制车道标记（单点线段）
    //         cv::line(processedFrame, {scaledLeft, scaledY}, {scaledLeft, scaledY},
    //                 cv::Scalar(0, 0, 255), lineThickness);  // 左线-红色
    //         cv::line(processedFrame, {scaledRight, scaledY}, {scaledRight, scaledY},
    //                 cv::Scalar(0, 255, 0), lineThickness);   // 右线-绿色
    //         cv::line(processedFrame, {scaledMid, scaledY}, {scaledMid, scaledY},
    //                 cv::Scalar(255, 0, 0), lineThickness);   // 中线-蓝色
    //     }

    //     // 图像旋转180度
    //     cv::rotate(processedFrame, processedFrame, cv::ROTATE_180);

    //     // 居中显示处理后的图像
    //     const cv::Rect displayArea(
    //         (screenWidth - newWidth) / 2,    // x起始位置
    //         (screenHeight - newHeight) / 2,  // y起始位置
    //         newWidth,                        // 显示宽度
    //         newHeight                        // 显示高度
    //     );
    //     processedFrame.copyTo(fbImage(displayArea));  // 复制到帧缓冲区

    //     // 转换到RGB565格式并更新帧缓冲
    //     convertMatToRGB565(fbImage, fb_buffer, screenWidth, screenHeight);
    // }

    // double dest_frame_duration1;
    // { // 计算帧率
    //     double dest_fps = readDoubleFromFile(destfps_file);  // 读取目标帧率
    //     double fps = cap.get(cv::CAP_PROP_FPS);  // 获取摄像头帧率
    //     printf("Camera fps:%lf\n", fps);  // 打印摄像头帧率

    //     // 计算每帧的延迟时间（us）
    //     dest_frame_duration1 = static_cast<double>(1000000 / std::min(fps, dest_fps));
    // }

    cv::Mat gray, binary;  // 创建灰度图和二值图变量
    cvtColor(raw_frame, gray, cv::COLOR_BGR2GRAY);  // 将原始帧转换为灰度图

    // 斑马线检测处理
    // handleZebraCrossing(raw_frame);  // 调用斑马线检测函数

    // 斑马线检测核心逻辑

    // 获取当前时间
    double current_time = cv::getTickCount() / cv::getTickFrequency();

    // 检查是否在冷却期内
    bool in_cooldown = current_time - start_time < zebra_cooldown_end.load();

    static bool first_run = true;  // 首次运行标志
    if (first_run) {
        const int debug_y = gray.rows - std::max(20, gray.rows / 8);  // 调试行位置
        const int debug_x1 = gray.cols * 0.15;  // 调试列位置1
        const int debug_x2 = debug_x1 + 50;  // 调试列位置2
        printf("[调试] 跑道灰度：%d，白线灰度：%d\n", 
               gray.at<uchar>(debug_y, debug_x1),  // 打印跑道灰度值
               gray.at<uchar>(debug_y, debug_x2));  // 打印白线灰度值
        first_run = false;  // 清除首次运行标志
    }

    const int height = gray.rows;  // 灰度图高度
    const int width = gray.cols;  // 灰度图宽度
    const int scan_y = height - std::max(20, height / 2);  // 扫描行位置
    const int x_start = width * 0.1;  // 扫描起始列
    const int x_end = width * 0.9;  // 扫描结束列
    const int GRAY_THRESHOLD = 160;  // 灰度阈值
    const int MIN_TRANSITIONS = 10;  // 最小跳变次数
    const int STOP_TIME_SECONDS = 3;  // 停车时间（秒）

    if (!in_cooldown) {  // 如果不在冷却期
        int total_transitions = 0;  // 总跳变次数
        const int scan_lines = 5;  // 扫描行数
        for (int i = 0; i < scan_lines; ++i) {  // 遍历扫描行
            int current_y = scan_y - i * 5;  // 当前行位置
            bool current_white = gray.at<uchar>(current_y, x_start) > GRAY_THRESHOLD;  // 当前像素是否为白色
            int line_transitions = 0;  // 当前行跳变次数

            for (int x = x_start; x < x_end; x += 3) {  // 遍历扫描列
                bool pixel_white = gray.at<uchar>(current_y, x) > GRAY_THRESHOLD;  // 当前像素是否为白色
                if (pixel_white != current_white) {  // 如果像素颜色发生变化
                    line_transitions++;  // 增加跳变次数
                    current_white = pixel_white;  // 更新当前颜色状态
                }
            }
            total_transitions += line_transitions;  // 累加跳变次数
        }

        const bool current_zebra = (total_transitions / scan_lines >= MIN_TRANSITIONS);  // 当前是否检测到斑马线
        const bool new_zebra_detected = current_zebra && !last_zebra_detected.load();  // 是否新检测到斑马线

        if (new_zebra_detected && !is_stopped) {  // 如果新检测到斑马线且未停车
            is_stopped = true;  // 设置停车标志
            stop_start_time = cv::getTickCount() / cv::getTickFrequency();  // 记录停车开始时间

            std::lock_guard<std::mutex> lock(speedMutex);  // 锁定速度互斥锁
            target_speed = 0.0;  // 设置目标速度为0
            sign = 1;  // 设置停车标志

            printf("[%s] 检测到斑马线（跳变次数：%d），停车%d秒\n", 
                  getCurrentTime().c_str(), total_transitions / scan_lines, STOP_TIME_SECONDS);  // 打印停车信息

            int ret = wonderEchoSend(0xFF, 0x10);  // 发送语音提示
            if (ret != 0) {
                printf("语音发送失败，错误码：%d\n", ret);  // 打印错误信息
            }
        }
        last_zebra_detected = current_zebra;  // 更新上次检测结果
    }

    if (is_stopped) {  // 如果正在停车
        // const double current_time = cv::getTickCount() / cv::getTickFrequency();
        if (current_time - stop_start_time >= STOP_TIME_SECONDS) {  // 如果停车时间超过设定值
            is_stopped = false;  // 清除停车标志
            sign = 0;  // 清除停车标志

            const double new_speed = readDoubleFromFile(speed_file);  // 读取新的速度值
            {
                std::lock_guard<std::mutex> lock(speedMutex);  // 锁定速度互斥锁
                target_speed = new_speed;  // 设置目标速度
            }
            printf("[%s] 停车结束，恢复速度至%.1f\n", getCurrentTime().c_str(), new_speed);  // 打印恢复速度信息
            start_time = cv::getTickCount() / cv::getTickFrequency();  // 更新冷却开始时间
            banmaxian_num++;  // 增加斑马线计数
        } else {
            return 0;  // 如果未到停车时间，返回0
        }
    }

    // // 白线检测核心逻辑
    // // 1. 定义检测区域（底部水平带状区域）
    // cv::Rect roi(0, gray.rows - DETECTION_ROI_HEIGHT, gray.cols, DETECTION_ROI_HEIGHT);
    // cv::Mat detectionZone = gray(roi);  // 提取检测区域

    // // 2. 二值化处理
    // cv::threshold(detectionZone, binary, WHITE_LINE_THRESHOLD, 255, cv::THRESH_BINARY);  // 二值化

    // // 3. 检测白线位置（找最上方的白色像素）
    // static std::atomic<bool> whiteLineDetected{false};  // 白线检测标志
    // bool currentDetection = false;  // 当前是否检测到白线

    // for (int y = binary.rows - 1; y >= 0; --y) {  // 从底部向上扫描
    //     uchar* row = binary.ptr<uchar>(y);  // 获取当前行指针
    //     for (int x = 0; x < binary.cols; ++x) {  // 遍历当前行
    //         if (row[x] == 255) {  // 如果当前像素为白色
    //             if ((binary.rows - y) <= STOP_DISTANCE_PX) {  // 如果距离底部小于停车距离
    //                 currentDetection = true;  // 设置检测标志
    //                 break;  // 退出循环
    //             }
    //         }
    //     }
    //     if (currentDetection) break;  // 如果检测到白线，退出循环
    // }

    // // 4. 状态更新和停车控制
    // static auto stopStartTime = std::chrono::steady_clock::now();  // 停车开始时间
    // if (currentDetection && !whiteLineDetected.load()) {  // 如果检测到白线且未停车
    //     whiteLineDetected = true;  // 设置白线检测标志
    //     stopStartTime = std::chrono::steady_clock::now();  // 记录停车开始时间

    //     std::lock_guard<std::mutex> lock(speedMutex);  // 锁定速度互斥锁
    //     // target_speed = 0.0;  // 设置目标速度为0
    //     stop_sign = 1;  // 设置停车标志

    //     printf("[%s] 检测到白线，触发停车\n", getCurrentTime().c_str());  // 打印停车信息

    //     // 可选：触发语音提示
    //     // int ret = wonderEchoSend(0xFF, 0x11);
    //     // if (ret != 0) {
    //     //     printf("语音发送失败，错误码：%d\n", ret);
    //     // }
    // }

    // // 5. 停车超时恢复
    // if (whiteLineDetected.load()) {  // 如果正在停车
    //     auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
    //         std::chrono::steady_clock::now() - stopStartTime).count();  // 计算停车时间

    //     if (elapsed >= STOP_DURATION) {  // 如果停车时间超过设定值
    //         whiteLineDetected = false;  // 清除白线检测标志
    //         stop_sign = 0;  // 清除停车标志

    //         const double new_speed = readDoubleFromFile(speed_file);  // 读取新的速度值
    //         {
    //             std::lock_guard<std::mutex> lock(speedMutex);  // 锁定速度互斥锁
    //             target_speed = new_speed;  // 设置目标速度
    //         }
    //         printf("[%s] 停车结束，恢复行驶\n", getCurrentTime().c_str());  // 打印恢复行驶信息
    //     }
    // }

    // 控制逻辑
    if (readFlag(start_file)) {  // 如果启动标志被设置
        int foresee = readDoubleFromFile(foresee_file);  // 读取预览值
        if (mid_line[foresee] != 255) {  // 如果中线值不为255
            const double servo_input = mid_line[foresee / calc_scale] * calc_scale - newWidth / 2;

            ServoControl.kP = kp;
            ServoControl.kI = ki;
            ServoControl.kD = kd;
            ServoControl.targetVal = 0;
            ServoControl.measuredVal = servo_input;
            ServoControl.isOutputEnabled = true;

            double servo_duty = -ServoControl.pidCalculate();

            servo_duty = std::clamp(servo_duty, -7.0, 7.0);  // 限制舵机占空比范围
            int servo_mid = readDoubleFromFile(servo_mid_file);  // 读取舵机中值
            double servoduty_ns = (servo_duty / 100) * servo.readPeriod() + servo_mid;  // 计算舵机占空比

            servo.setDutyCycle(servoduty_ns);  // 设置舵机占空比

            const double new_speed = readDoubleFromFile(speed_file);  // 读取新的速度值
            {
                std::lock_guard<std::mutex> lock(speedMutex);  // 锁定速度互斥锁
                // target_speed = new_speed;  // 设置目标速度
            }

            // double target_speed = readDoubleFromFile(speed_file);
            if (sign == 0) {  // 如果未停车
                // 使用PID控制每个电机（示例）
                for (int i = 0; i < 2; ++i) {  // 遍历电机
                    target_speed = readDoubleFromFile(speed_file);  // 读取目标速度
                    double edcoder_speed = -motorController[i]->encoderSpeed();  // 获取编码器速度
                    double speed_error = target_speed - edcoder_speed;  // 计算速度误差

                    SpeedPID.setPID(mortor_kp, mortor_ki, mortor_kd);  // 设置电机PID参数
                    // PID控制
                    double duty_adjustment = SpeedPID.updatemortor(speed_error);  // 更新电机占空比

                    motorController[i]->updateduty(duty_adjustment);  // 设置电机占空比
                    // motorController[i]->updateduty(15);
                    // std::cout << "current_speed : " << current_speed << std::endl;
                    // std::cout << "edcoder_speed : " << edcoder_speed << std::endl;
                    // // std::cout << "last_duty : " << last_duty << std::endl;
                    // std::cout << "duty_adjustment : " << duty_adjustment << std::endl;
                }
            }
            // else
            // {
            //     target_speed = 0.0;  // 设置目标速度为0
            //     // 使用PID控制每个电机（示例）
            //     for (int i = 0; i < 2; ++i) {  // 遍历电机
            //         motorController[i]->updateduty(0);  // 设置电机占空比为0
            // //         double edcoder_speed = -motorController[i]->encoderSpeed();  // 获取编码器速度
            // //         double speed_error = target_speed - edcoder_speed;  // 计算速度误差

            // //         BrakePID.setPID(stop_kp, stop_ki, stop_kd);  // 设置刹车PID参数
            // //         // PID控制
            // //         double duty_stop = BrakePID.updatemortor(speed_error);  // 更新刹车占空比
            
            // //         motorController[i]->updateduty(duty_stop);  // 设置刹车占空比
            //     }
    // }

            // mortorEN.setValue(1);  // 设置电机使能
        }
    }

    return 0;  // 返回成功
}

// 获取当前时间的函数
std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();  // 获取当前时间
    auto in_time_t = std::chrono::system_clock::to_time_t(now);  // 转换为time_t类型
    std::stringstream ss;  // 创建字符串流
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");  // 格式化时间
    return ss.str();  // 返回格式化后的时间字符串
}