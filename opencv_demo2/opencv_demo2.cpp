/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2025-01-07 06:26:01
 * @LastEditors: Ilikara 3435193369@qq.com
 * @LastEditTime: 2025-01-17 15:41:43
 * @FilePath: /2k300_smartcar/opencv_demo2/opencv_demo2.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <opencv2/opencv.hpp>
#include <cmath>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <algorithm> // for sort

using namespace cv;
using namespace std;

// 定义立方体的顶点
vector<Point3f> vertices = {
    {-1, -1, -1},
    {1, -1, -1},
    {1, 1, -1},
    {-1, 1, -1},
    {-1, -1, 1},
    {1, -1, 1},
    {1, 1, 1},
    {-1, 1, 1}};

// 定义立方体的面
vector<vector<int>> faces = {
    {0, 1, 2, 3}, // 前面
    {4, 5, 6, 7}, // 后面
    {0, 1, 5, 4}, // 底面
    {2, 3, 7, 6}, // 顶面
    {0, 3, 7, 4}, // 左面
    {1, 2, 6, 5}  // 右面
};

// 定义每个面的颜色
vector<Scalar> faceColors = {
    Scalar(255, 0, 0),   // 红色
    Scalar(0, 255, 0),   // 绿色
    Scalar(0, 0, 255),   // 蓝色
    Scalar(255, 255, 0), // 黄色
    Scalar(255, 0, 255), // 紫色
    Scalar(0, 255, 255)  // 青色
};

// 将3D点投影到2D平面（透视投影）
Point2f projectPoint(Point3f point, Mat rotationMatrix, float scale, Point2f offset, float fov)
{
    Mat pointMat = (Mat_<float>(3, 1) << point.x, point.y, point.z);
    Mat rotatedPoint = rotationMatrix * pointMat;

    // 透视投影
    float z = rotatedPoint.at<float>(2, 0);
    float x = rotatedPoint.at<float>(0, 0) / (z / fov + 1) * scale + offset.x;
    float y = rotatedPoint.at<float>(1, 0) / (z / fov + 1) * scale + offset.y;

    return Point2f(x, y);
}

// 将 RGB888 转换为 RGB565
ushort rgb888_to_rgb565(const Vec3b &color)
{
    return ((color[2] >> 3) << 11) | ((color[1] >> 2) << 5) | (color[0] >> 3);
}

// 计算面的中心深度
float calculateFaceDepth(const vector<Point3f> &faceVertices, Mat rotationMatrix)
{
    float depth = 0;
    for (const auto &vertex : faceVertices)
    {
        Mat pointMat = (Mat_<float>(3, 1) << vertex.x, vertex.y, vertex.z);
        Mat rotatedPoint = rotationMatrix * pointMat;
        depth += rotatedPoint.at<float>(2, 0); // Z 轴深度
    }
    return depth / faceVertices.size(); // 返回平均深度
}

int main()
{
    // Framebuffer 设备
    const char *fb_device = "/dev/fb0";
    int fb_fd = open(fb_device, O_RDWR);
    if (fb_fd == -1)
    {
        cerr << "Error: Cannot open framebuffer device" << endl;
        return -1;
    }

    // 获取 Framebuffer 信息
    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo))
    {
        cerr << "Error: Cannot get framebuffer information" << endl;
        close(fb_fd);
        return -1;
    }

    // 检查 Framebuffer 是否支持 RGB565
    if (vinfo.bits_per_pixel != 16)
    {
        cerr << "Error: Framebuffer is not RGB565 format" << endl;
        close(fb_fd);
        return -1;
    }

    // 映射 Framebuffer 到内存
    size_t fb_size = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    ushort *fb_data = (ushort *)mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_data == MAP_FAILED)
    {
        cerr << "Error: Failed to map framebuffer to memory" << endl;
        close(fb_fd);
        return -1;
    }

    // 创建 OpenCV 图像（160x128，RGB888）
    Mat image(128, 160, CV_8UC3, Scalar(0, 0, 0));

    float angle = 0;
    float scale = 50;       // 缩放因子
    Point2f offset(80, 64); // 图像中心点
    float fov = 500;        // 透视投影的视野

    while (true)
    {
        // 清空图像
        image.setTo(Scalar(0, 0, 0));

        // 计算旋转矩阵
        Mat rotationMatrixX = (Mat_<float>(3, 3) << 1, 0, 0,
                               0, cos(angle), -sin(angle),
                               0, sin(angle), cos(angle));

        Mat rotationMatrixY = (Mat_<float>(3, 3) << cos(angle), 0, sin(angle),
                               0, 1, 0,
                               -sin(angle), 0, cos(angle));

        Mat rotationMatrixZ = (Mat_<float>(3, 3) << cos(angle), -sin(angle), 0,
                               sin(angle), cos(angle), 0,
                               0, 0, 1);

        Mat rotationMatrix = rotationMatrixZ * rotationMatrixY * rotationMatrixX;

        // 计算每个面的深度并排序
        vector<pair<float, int>> faceDepths; // {深度, 面索引}
        for (size_t i = 0; i < faces.size(); i++)
        {
            vector<Point3f> faceVertices;
            for (int vertexIndex : faces[i])
            {
                faceVertices.push_back(vertices[vertexIndex]);
            }
            float depth = calculateFaceDepth(faceVertices, rotationMatrix);
            faceDepths.push_back({depth, i});
        }

        // 按深度从远到近排序
        sort(faceDepths.begin(), faceDepths.end(), [](const pair<float, int> &a, const pair<float, int> &b)
             {
                 return a.first > b.first; // 深度大的先绘制
             });

        // 按排序后的顺序绘制面
        for (const auto &faceDepth : faceDepths)
        {
            int faceIndex = faceDepth.second;
            vector<Point2f> facePointsFloat;
            for (int vertexIndex : faces[faceIndex])
            {
                Point3f vertex = vertices[vertexIndex];
                Point2f projectedPoint = projectPoint(vertex, rotationMatrix, scale, offset, fov);
                facePointsFloat.push_back(projectedPoint);
            }

            // 将 Point2f 转换为 Point（CV_32S 类型）
            vector<Point> facePoints;
            for (const auto &pt : facePointsFloat)
            {
                facePoints.push_back(Point(cvRound(pt.x), cvRound(pt.y)));
            }

            // 填充面
            fillConvexPoly(image, facePoints, faceColors[faceIndex]);

            // 绘制边
            for (size_t j = 0; j < facePoints.size(); j++)
            {
                line(image, facePoints[j], facePoints[(j + 1) % facePoints.size()], Scalar(0, 0, 0), 2);
            }
        }

        // 将 OpenCV 图像（RGB888）转换为 RGB565 并写入 Framebuffer
        for (int y = 0; y < image.rows; y++)
        {
            for (int x = 0; x < image.cols; x++)
            {
                Vec3b color = image.at<Vec3b>(y, x);
                fb_data[y * vinfo.xres + x] = rgb888_to_rgb565(color);
            }
        }

        // 更新角度
        angle += 0.02;

        // 等待一段时间
        usleep(1000000 / 50);
    }

    // 释放资源
    munmap(fb_data, fb_size);
    close(fb_fd);

    return 0;
}