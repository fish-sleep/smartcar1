#include "camera.h"
#include "GPIO.h"

// ==========================
// 全局变量定义
// ==========================
cv::VideoCapture cap; // 摄像头对象，抓取视频流
std::mutex speedMutex; // 保护速度变量的互斥锁
double kp = 0;         // 方向舵机PID比例系数
double ki = 0;         // 方向舵机PID积分系数
double kd = 0;         // 方向舵机PID微分系数
int screenWidth, screenHeight;    // 屏幕分辨率宽度和高度
int newWidth, newHeight;          // 自适应后的分辨率
int fb;                          // 帧缓冲设备文件描述符
uint16_t *fb_buffer;             // 指向帧缓冲区内存的指针
PwmController servo(1, 0);       // 舵机控制PWM对象
bool streamCaptureRunning = false; // 标识视频流捕获是否运行
int saved_frame_count = 0;        // 已保存的帧数计数器
std::mutex frameMutex;            // 保护pubframe的互斥锁
cv::Mat pubframe;                 // 共享视频帧

// 电机控制PID参数
double mortor_kp = 0;
double mortor_ki = 0.1;
double mortor_kd = 0;
// 停车控制PID参数
double stop_kp = 0;
double stop_ki = 0.1;
double stop_kd = 0;

int sign = 0;               // 停车标志，1：停车，0：行驶
int stop_sign = 0;          // 备用停车标识
double start_time = -15;    // 斑马线冷却时间起始标志

// ==========================
// 斑马线检测相关全局变量
// ==========================
std::atomic<int> banmaxian_num(0);      // 斑马线计数器，线程安全
std::atomic<bool> is_stopped{false};    // 是否处于停车状态
std::atomic<double> stop_start_time{0.0};  // 记录停车时的起始时间
std::atomic<bool> last_zebra_detected{false};  // 记录上次斑马线检测结果

// 斑马线冷却时间结束标识，防止频繁触发停车
std::atomic<double> zebra_cooldown_end{10.0};  

#define calc_scale 2  // 缩放比例，分辨率调整用

// 注释掉的PID控制示例
// static PIDController BrakePID(2.0, 0.5, 1.0, 0.0, INCREMENTAL, 100.0); // 刹车PID控制器
// static PIDController SpeedPID(0, 0, 0, 0, INCREMENTAL, 100.0);  // 速度环（增量式）

// ===============================
// 斑马线检测函数，判断是否检测到斑马线
// ===============================
bool detectZebraCrossing(const cv::Mat& inputImage) {
    // 1. 定义斑马线颜色及形态学参数
    const cv::Scalar lowerWhite(0, 0, 200);    // HSV白色下限
    const cv::Scalar upperWhite(180, 30, 255); // HSV白色上限
    const int MIN_STRIPE_COUNT = 3;            // 最少白条数量
    const int MIN_AREA = 50;                   // 白条最小面积
    const float MIN_ASPECT_RATIO = 3.0f;       // 白条最小长宽比（宽比高）
    const int MIN_WIDTH = 30;                  // 白条最小宽度
    const float SPACING_VARIANCE = 0.3f;       // 条纹间距最大容许变化率

    // 2. 转换输入图像到HSV空间，便于筛选白色区域
    cv::Mat hsvImage;
    cvtColor(inputImage, hsvImage, cv::COLOR_BGR2HSV);

    // 3. 创建白色掩膜，滤出白色条纹
    cv::Mat whiteMask;
    inRange(hsvImage, lowerWhite, upperWhite, whiteMask);

    // 4. 形态学闭操作，增强水平连通的白色条纹
    cv::Mat kernelH = getStructuringElement(cv::MORPH_RECT, cv::Size(15, 1));
    morphologyEx(whiteMask, whiteMask, cv::MORPH_CLOSE, kernelH);

    // 5. 轮廓检测，得到每条白条轮廓
    std::vector<std::vector<cv::Point>> contours;
    findContours(whiteMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // 6. 遍历轮廓筛选符合斑马线条纹特性的轮廓
    int stripeCount = 0;
    std::vector<cv::Rect> stripeRects;

    for (const auto& contour : contours) {
        double area = contourArea(contour);  // 计算轮廓面积
        if (area < MIN_AREA) continue;       // 面积太小忽略

        cv::Rect rect = boundingRect(contour);   // 获得包围框
        float aspectRatio = (float)rect.width / rect.height;  // 计算长宽比

        // 判断是否为长条形（宽度远大于高度且宽度大于最小值）
        if (aspectRatio > MIN_ASPECT_RATIO && rect.width > MIN_WIDTH) {
            stripeCount++;
            stripeRects.push_back(rect);
        }
    }

    // 7. 条纹数达到阈值时，判断条纹间距是否均匀
    if (stripeCount >= MIN_STRIPE_COUNT) {
        // 按Y坐标升序排序条纹
        std::sort(stripeRects.begin(), stripeRects.end(),
                  [](const cv::Rect& a, const cv::Rect& b) { return a.y < b.y; });

        // 计算条纹间距平均值
        float avgGap = 0;
        for (size_t i = 1; i < stripeRects.size(); ++i) {
            avgGap += (stripeRects[i].y - stripeRects[i - 1].y);
        }
        avgGap /= (stripeRects.size() - 1);

        // 检查条纹间距变化是否超过容许范围
        bool isZebra = true;
        for (size_t i = 1; i < stripeRects.size(); ++i) {
            float gap = stripeRects[i].y - stripeRects[i - 1].y;
            if (fabs(gap - avgGap) > avgGap * SPACING_VARIANCE) {
                isZebra = false;  // 条纹间距不均匀，不是斑马线
                break;
            }
        }

        return isZebra;  // 返回是否是斑马线
    }

    return false;  // 条纹不够，返回false
}

// ===============================
// 斑马线检测后的处理逻辑
// ===============================
void handleZebraCrossing(const cv::Mat& frame) {
    // 1. 检测当前帧是否存在斑马线
    bool current_zebra = detectZebraCrossing(frame);
    // 是否新检测到斑马线（上次没检测到，这帧检测到了）
    bool new_zebra_detected = current_zebra && !last_zebra_detected.load();
    const int STOP_TIME_SECONDS = 3;  // 停车时间，单位秒

    // 2. 新检测到斑马线且车辆未停车时执行停车逻辑
    if (new_zebra_detected && !is_stopped) {
        is_stopped = true;  // 标记为停车状态
        stop_start_time = cv::getTickCount() / cv::getTickFrequency();  // 记录停车开始时间

        std::lock_guard<std::mutex> lock(speedMutex);
        target_speed = 0.0;   // 设置目标速度为0，停车
        sign = 1;             // 设置停车标志

        printf("[%s] 检测到斑马线，停车%d秒\n",
               getCurrentTime().c_str(), STOP_TIME_SECONDS);

        // 触发语音播报提示（可选）
        int ret = wonderEchoSend(0xFF, 0x10);
        if (ret != 0) {
            printf("语音发送失败，错误码：%d\n", ret);
        }
    }

    // 3. 更新上次的斑马线检测结果
    last_zebra_detected = current_zebra;

    // 4. 如果处于停车状态并超过停车时间则恢复行驶
    if (is_stopped) {
        const double current_time = cv::getTickCount() / cv::getTickFrequency();
        if (current_time - stop_start_time >= STOP_TIME_SECONDS) {
            is_stopped = false;   // 清除停车状态
            sign = 0;             // 清除停车标志
            banmaxian_num++;      // 增加斑马线计数

            const double new_speed = readDoubleFromFile(speed_file); 
            {
                std::lock_guard<std::mutex> lock(speedMutex);
                target_speed = new_speed;  // 恢复目标速度
            }
            printf("[%s] 停车结束，恢复速度至%.1f\n",
                   getCurrentTime().c_str(), new_speed);
        }
    }
}

// ===============================
// 摄像头初始化
// ===============================
int CameraInit(uint8_t camera_id, double dest_fps, int width, int height) {
    // 设置舵机周期和中间占空比，启动使能
    servo.setPeriod(3000000);
    servo.setDutyCycle(1500000);
    servo.enable();

    // 打开帧缓冲设备
    fb = open("/dev/fb0", O_RDWR);
    if (fb == -1) {
        std::cerr << "无法打开帧缓冲区设备" << std::endl;
        return -1;
    }

    // 获取帧缓冲信息
    struct fb_var_screeninfo vinfo;
    if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        std::cerr << "无法获取帧缓冲区信息" << std::endl;
        close(fb);
        return -1;
    }

    screenWidth = vinfo.xres;       // 屏幕宽度
    screenHeight = vinfo.yres;      // 屏幕高度
    size_t fb_size = vinfo.yres_virtual * vinfo.xres_virtual * vinfo.bits_per_pixel / 8;

    // 映射帧缓冲区到内存
    fb_buffer = (uint16_t *)mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
    if (fb_buffer == MAP_FAILED) {
        std::cerr << "无法映射帧缓冲区到内存" << std::endl;
        close(fb);
        return -1;
    }

    // 打开摄像头设备
    cap.open(camera_id);
    if (!cap.isOpened()) {
        printf("无法打开摄像头\n");
        munmap(fb_buffer, fb_size);
        close(fb);
        return -1;
    }

    // 设置摄像头分辨率
    cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    // 设置摄像头编码格式为MJPG
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    // 关闭自动曝光，手动调节曝光
    cap.set(cv::CAP_PROP_AUTO_EXPOSURE, -1);

    int cameraWidth = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    int cameraHeight = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    printf("摄像头分辨率: %d x %d\n", cameraWidth, cameraHeight);

    // 根据屏幕大小适配缩放比例
    double widthRatio = static_cast<double>(screenWidth) / cameraWidth;
    double heightRatio = static_cast<double>(screenHeight) / cameraHeight;
    double scale = std::min(widthRatio, heightRatio);

    newWidth = static_cast<int>(cameraWidth * scale);
    newHeight = static_cast<int>(cameraHeight * scale);
    printf("自适应分辨率: %d x %d\n", newWidth, newHeight);

    double fps = cap.get(cv::CAP_PROP_FPS);
    printf("Camera fps:%lf\n", fps);

    line_tracking_width = newWidth / calc_scale;
    line_tracking_height = newHeight / calc_scale;

    // 返回每帧时长，单位ms
    return static_cast<int>(1000.0 / std::min(fps, dest_fps));
}

// ===============================
// 摄像头资源释放
// ===============================
void cameraDeInit(void) {
    cap.release();  // 释放摄像头

    struct fb_var_screeninfo vinfo;
    if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        std::cerr << "无法获取帧缓冲区信息" << std::endl;
    } else {
        size_t fb_size = vinfo.yres_virtual * vinfo.xres_virtual * vinfo.bits_per_pixel / 8;
        munmap(fb_buffer, fb_size);  // 解除内存映射
    }

    close(fb);  // 关闭帧缓冲设备
}

// ===============================
// 保存图像文件
// ===============================
bool saveCameraImage(const cv::Mat& frame, const std::string &directory) {
    if (frame.empty()) {
        std::cerr << "Save Error: Frame is empty." << std::endl;
        return false;  // 空图像不保存
    }

    // 拼接文件名路径，例如 ./image/image_00001.jpg
    std::ostringstream filename;
    filename << directory << "/image_" << std::setw(5) << std::setfill('0') << saved_frame_count << ".jpg";
    saved_frame_count++;  // 计数器递增
    return cv::imwrite(filename.str(), frame);  // 直接保存图像
}

// ===============================
// 视频流捕获线程函数
// ===============================
void streamCapture(void) {
    cv::Mat frame;
    while (streamCaptureRunning) {
        cap.read(frame);  // 从摄像头读取帧
        frameMutex.lock();
        pubframe = frame;  // 更新共享帧
        frameMutex.unlock();
    }
}

// ===============================
// 摄像头处理主函数（含PID控制）
// ===============================
PIDController ServoControl(1.0, 0.0, 2.0, 0.0, POSITION, 1250000);  // 舵机PID控制器
// PIDController MotorControl(0, 0, 0, 0, INCREMENTAL, 1250000);  // 电机PID定义（注释）

int CameraHandler(void) {
    cv::Mat resizedFrame;
    static std::atomic<bool> is_stopped{false};              // 斑马线停车状态标志
    static std::atomic<double> stop_start_time{0.0};          // 斑马线停车开始时间
    static std::atomic<bool> last_zebra_detected{false};      // 上一帧斑马线检测结果

    // 帧率计算变量
    static double last_time = cv::getTickCount() / cv::getTickFrequency();
    static int frame_count = 0;
    static double fps = 0.0;

    frameMutex.lock();
    raw_frame = pubframe.clone();  // 复制保存当前帧
    frameMutex.unlock();

    if (raw_frame.empty()) {
        printf("无法捕获图像\n");
        return -1;  // 捕获失败返回错误
    }

    // 帧率计算，每10帧计算一次
    frame_count++;
    if (frame_count >= 10) {
        double current_time = cv::getTickCount() / cv::getTickFrequency();
        fps = frame_count / (current_time - last_time);
        printf("当前帧率: %.2f FPS\n", fps);
        frame_count = 0;
        last_time = current_time;
    }

    // 保存图像
    if (readFlag(saveImg_file)) {
        if (saveCameraImage(raw_frame, std::string("./image"))) {
            printf("图像%d已保存\n", saved_frame_count);
        } else {
            printf("图像保存失败\n");
            return -1;
        }
    }

    { image_main(); }  // 自定义图像处理主函数

    // ------------------------
    // 以下注释部分为显示处理结果，已保留原注释
    // ------------------------
    /*
    if (readFlag(showImg_file)) {
        // 创建全黑背景的帧缓冲图像
        cv::Mat fbImage(screenHeight, screenWidth, CV_8UC3, cv::Scalar(0, 0, 0));
        // 尺寸调整和颜色空间转换
        cv::Mat processedFrame;
        cv::resize(track, processedFrame, cv::Size(newWidth, newHeight));
        cv::cvtColor(processedFrame, processedFrame, cv::COLOR_GRAY2BGR);
        // 车道线绘制（在原始方向）
        const int lineThickness = calc_scale;
        for (int y = 0; y < line_tracking_height; ++y) {
            // 计算缩放后坐标
            const int scaledY = y * calc_scale;
            const int scaledLeft = left_line[y] * calc_scale;
            const int scaledRight = right_line[y] * calc_scale;
            const int scaledMid = mid_line[y] * calc_scale;
            // 绘制车道标记（单点线段）
            cv::line(processedFrame, {scaledLeft, scaledY}, {scaledLeft, scaledY},
                    cv::Scalar(0, 0, 255), lineThickness);  // 左线-红色
            cv::line(processedFrame, {scaledRight, scaledY}, {scaledRight, scaledY},
                    cv::Scalar(0, 255, 0), lineThickness);   // 右线-绿色
            cv::line(processedFrame, {scaledMid, scaledY}, {scaledMid, scaledY},
                    cv::Scalar(255, 0, 0), lineThickness);   // 中线-蓝色
        }
        // 图像旋转180度
        cv::rotate(processedFrame, processedFrame, cv::ROTATE_180);
        // 居中显示处理后的图像
        const cv::Rect displayArea(
            (screenWidth - newWidth) / 2,    // x起始位置
            (screenHeight - newHeight) / 2,  // y起始位置
            newWidth,                        // 显示宽度
            newHeight                        // 显示高度
        );
        processedFrame.copyTo(fbImage(displayArea));
        // 转换到RGB565格式并更新帧缓冲
        convertMatToRGB565(fbImage, fb_buffer, screenWidth, screenHeight);
    }
    */

    // ------------------------
    // 以下注释为帧率计算示例，保留原注释
    // ------------------------
    /*
    double dest_frame_duration1;
    {
        double dest_fps = readDoubleFromFile(destfps_file);
        double fps = cap.get(cv::CAP_PROP_FPS);
        printf("Camera fps:%lf\n", fps);
        dest_frame_duration1 = static_cast<double>(1000000 / std::min(fps, dest_fps));
    }
    */

    // 斑马线检测核心逻辑实现
    cv::Mat gray, binary;
    cvtColor(raw_frame, gray, cv::COLOR_BGR2GRAY);  // 转换成灰度图

    // 斑马线停车检测逻辑
    // handleZebraCrossing(raw_frame);  // 已被注释

    // 取当前时间，用于冷却判断
    double current_time = cv::getTickCount() / cv::getTickFrequency();
    bool in_cooldown = current_time - start_time < zebra_cooldown_end.load();  // 判断是否在冷却时间内

    static bool first_run = true;
    if (first_run) {
        const int debug_y = gray.rows - std::max(20, gray.rows / 8);
        const int debug_x1 = gray.cols * 0.15;
        const int debug_x2 = debug_x1 + 50;
        printf("[调试] 跑道灰度：%d，白线灰度：%d\n",
               gray.at<uchar>(debug_y, debug_x1),
               gray.at<uchar>(debug_y, debug_x2));
        first_run = false;
    }

    // 斑马线检测 - 通过灰度图扫描多行，统计跳变次数
    const int height = gray.rows;
    const int width = gray.cols;
    const int scan_y = height - std::max(20, height / 2);
    const int x_start = width * 0.1;
    const int x_end = width * 0.9;
    const int GRAY_THRESHOLD = 160;
    const int MIN_TRANSITIONS = 10;
    const int STOP_TIME_SECONDS = 3;

    if (!in_cooldown)
    {
        int total_transitions = 0;
        const int scan_lines = 5;  // 扫描的连续行数
        for (int i = 0; i < scan_lines; ++i) 
        {
            int current_y = scan_y - i * 5;
            bool current_white = gray.at<uchar>(current_y, x_start) > GRAY_THRESHOLD;
            int line_transitions = 0;

            for (int x = x_start; x < x_end; x += 3) {
                bool pixel_white = gray.at<uchar>(current_y, x) > GRAY_THRESHOLD;
                if (pixel_white != current_white) {
                    line_transitions++;
                    current_white = pixel_white;
                }
            }
            total_transitions += line_transitions;
        }

        const bool current_zebra = (total_transitions / scan_lines >= MIN_TRANSITIONS);  // 是否检测到斑马线
        const bool new_zebra_detected = current_zebra && !last_zebra_detected.load();

        if (new_zebra_detected && !is_stopped) {
            is_stopped = true;
            stop_start_time = cv::getTickCount() / cv::getTickFrequency();

            std::lock_guard<std::mutex> lock(speedMutex);
            target_speed = 0.0;
            sign = 1;
            printf("[%s] 检测到斑马线（跳变次数：%d），停车%d秒\n",
                   getCurrentTime().c_str(), total_transitions / scan_lines, STOP_TIME_SECONDS);

            int ret = wonderEchoSend(0xFF, 0x10);
            if (ret != 0) {
                printf("语音发送失败，错误码：%d\n", ret);
            }
        }
        last_zebra_detected = current_zebra;
    }

    if (is_stopped) {
        if (current_time - stop_start_time >= STOP_TIME_SECONDS) {
            is_stopped = false;
            sign = 0;
            const double new_speed = readDoubleFromFile(speed_file);
            {
                std::lock_guard<std::mutex> lock(speedMutex);
                target_speed = new_speed;
            }
            printf("[%s] 停车结束，恢复速度至%.1f\n", getCurrentTime().c_str(), new_speed);
            start_time = cv::getTickCount() / cv::getTickFrequency();
            banmaxian_num++;
        } else {
            return 0;
        }
    }

    // ------------------------
    // 注释部分：白线检测核心逻辑，保留原注释
    // ------------------------
    /*
    // 1. 定义检测区域（底部水平带状区域）
    cv::Rect roi(0, gray.rows - DETECTION_ROI_HEIGHT, gray.cols, DETECTION_ROI_HEIGHT);
    cv::Mat detectionZone = gray(roi);

    // 2. 二值化处理
    cv::threshold(detectionZone, binary, WHITE_LINE_THRESHOLD, 255, cv::THRESH_BINARY);

    // 3. 检测白线位置（找最上方的白色像素）
    static std::atomic<bool> whiteLineDetected{false};
    bool currentDetection = false;

    for (int y = binary.rows - 1; y >= 0; --y) 
    {
        uchar* row = binary.ptr<uchar>(y);
        for (int x = 0; x < binary.cols; ++x) 
        {
            if (row[x] == 255) {
                if ((binary.rows - y) <= STOP_DISTANCE_PX) 
                {
                    currentDetection = true;
                    break;
                }
            }
        }
        if (currentDetection) break;
    }

    // 4. 状态更新和停车控制
    static auto stopStartTime = std::chrono::steady_clock::now();
    if (currentDetection && !whiteLineDetected.load()) 
    {
        whiteLineDetected = true;
        stopStartTime = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(speedMutex);
        // target_speed = 0.0;
        stop_sign = 1;  // 设置停车标志

        printf("[%s] 检测到白线，触发停车\n", getCurrentTime().c_str());

        // 可选：触发语音提示
        // int ret = wonderEchoSend(0xFF, 0x11);
        // if (ret != 0) {
        //     printf("语音发送失败，错误码：%d\n", ret);
        // }
    }

    // 5. 停车超时恢复
    if (whiteLineDetected.load()) 
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - stopStartTime).count();

        if (elapsed >= STOP_DURATION) 
        {
            whiteLineDetected = false;
            stop_sign = 0;  // 清除停车标志

            const double new_speed = readDoubleFromFile(speed_file);
            {
                std::lock_guard<std::mutex> lock(speedMutex);
                target_speed = new_speed;
            }
            printf("[%s] 停车结束，恢复行驶\n", getCurrentTime().c_str());
        }
     }
    */


    // ------------------------
    // 控制逻辑，根据读取的控制文件启动车辆控制
    // ------------------------
    if (readFlag(start_file)) {
        int foresee = readDoubleFromFile(foresee_file);
        if (mid_line[foresee] != 255) {
            ServoControl.setPID(kp, ki, kd);

            const double servo_input = mid_line[foresee / calc_scale] * calc_scale - newWidth / 2;

            // double servoduty = -ServoControl.updateservo(servo_input);
            // double servo_duty = Turn_Out_Filter(servoduty);

            double servo_duty = -ServoControl.updateservo(servo_input);

            servo_duty = std::clamp(servo_duty, -7.0, 7.0);
            int servo_mid = readDoubleFromFile(servo_mid_file);
            double servoduty_ns = (servo_duty / 100) * servo.readPeriod() + servo_mid;

            servo.setDutyCycle(servoduty_ns);

            const double new_speed = readDoubleFromFile(speed_file);
            {
                std::lock_guard<std::mutex> lock(speedMutex);
                // target_speed = new_speed;
            }

            if (sign == 0) {
                // 使用PID控制每个电机（示例）
                for (int i = 0; i < 2; ++i) {
                    target_speed = readDoubleFromFile(speed_file);
                    double edcoder_speed = -motorController[i]->encoderSpeed();
                    double speed_error = target_speed - edcoder_speed;

                    SpeedPID.setPID(mortor_kp, mortor_ki, mortor_kd);
                    // PID控制
                    double duty_adjustment = SpeedPID.updatemortor(speed_error);

                    motorController[i]->updateduty(duty_adjustment);

                    // motorController[i]->updateduty(15);
                    // std::cout << "current_speed : " << current_speed << std::endl;
                    // std::cout << "edcoder_speed : " << edcoder_speed << std::endl;
                    // std::cout << "duty_adjustment : " << duty_adjustment << std::endl;
                }
            }
            // else {
            //     target_speed = 0.0;
            //     for (int i = 0; i < 2; ++i) {
            //         motorController[i]->updateduty(0);
            //     }
            // }

            // mortorEN.setValue(1);

        }
    }

    return 0;
}

// ===============================
// 获取当前时间字符串，例如“2023-11-11 12:34:56”
// ===============================
std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
    return ss.str();
}

