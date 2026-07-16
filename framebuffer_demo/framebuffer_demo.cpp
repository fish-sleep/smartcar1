#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <cstring>
#include <cmath>
#include <iostream>

using namespace std;

// Framebuffer 设备
const char *fb_device = "/dev/fb0";

// 屏幕分辨率
const int screen_width = 160;
const int screen_height = 128;

// RGB565 颜色宏
#define RGB565(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))

// 8x8 位图字体（26 个字母 + 10 个数字）
const unsigned char font[36][8] = {
    // A-Z
    {0b00111000, 0b01000100, 0b01000100, 0b01111100, 0b01000100, 0b01000100, 0b01000100, 0b00000000}, // A
    {0b01111000, 0b01000100, 0b01000100, 0b01111000, 0b01000100, 0b01000100, 0b01111000, 0b00000000}, // B
    {0b00111100, 0b01000010, 0b01000000, 0b01000000, 0b01000000, 0b01000010, 0b00111100, 0b00000000}, // C
    {0b01111000, 0b01000100, 0b01000100, 0b01000100, 0b01000100, 0b01000100, 0b01111000, 0b00000000}, // D
    {0b01111110, 0b01000000, 0b01000000, 0b01111000, 0b01000000, 0b01000000, 0b01111110, 0b00000000}, // E
    {0b01111110, 0b01000000, 0b01000000, 0b01111000, 0b01000000, 0b01000000, 0b01000000, 0b00000000}, // F
    {0b00111100, 0b01000010, 0b01000000, 0b01001110, 0b01000010, 0b01000010, 0b00111100, 0b00000000}, // G
    {0b01000100, 0b01000100, 0b01000100, 0b01111100, 0b01000100, 0b01000100, 0b01000100, 0b00000000}, // H
    {0b00111000, 0b00010000, 0b00010000, 0b00010000, 0b00010000, 0b00010000, 0b00111000, 0b00000000}, // I
    {0b00011110, 0b00001000, 0b00001000, 0b00001000, 0b00001000, 0b01001000, 0b00110000, 0b00000000}, // J
    {0b01000100, 0b01001000, 0b01010000, 0b01100000, 0b01010000, 0b01001000, 0b01000100, 0b00000000}, // K
    {0b01000000, 0b01000000, 0b01000000, 0b01000000, 0b01000000, 0b01000000, 0b01111110, 0b00000000}, // L
    {0b01000100, 0b01101100, 0b01010100, 0b01010100, 0b01000100, 0b01000100, 0b01000100, 0b00000000}, // M
    {0b01000100, 0b01100100, 0b01010100, 0b01001100, 0b01000100, 0b01000100, 0b01000100, 0b00000000}, // N
    {0b00111000, 0b01000100, 0b01000100, 0b01000100, 0b01000100, 0b01000100, 0b00111000, 0b00000000}, // O
    {0b01111000, 0b01000100, 0b01000100, 0b01111000, 0b01000000, 0b01000000, 0b01000000, 0b00000000}, // P
    {0b00111000, 0b01000100, 0b01000100, 0b01000100, 0b01010100, 0b01001000, 0b00110100, 0b00000000}, // Q
    {0b01111000, 0b01000100, 0b01000100, 0b01111000, 0b01010000, 0b01001000, 0b01000100, 0b00000000}, // R
    {0b00111100, 0b01000010, 0b01000000, 0b00111000, 0b00000100, 0b01000010, 0b00111100, 0b00000000}, // S
    {0b01111100, 0b00010000, 0b00010000, 0b00010000, 0b00010000, 0b00010000, 0b00010000, 0b00000000}, // T
    {0b01000100, 0b01000100, 0b01000100, 0b01000100, 0b01000100, 0b01000100, 0b00111000, 0b00000000}, // U
    {0b01000100, 0b01000100, 0b01000100, 0b01000100, 0b01000100, 0b00101000, 0b00010000, 0b00000000}, // V
    {0b01000100, 0b01000100, 0b01000100, 0b01010100, 0b01010100, 0b01101100, 0b01000100, 0b00000000}, // W
    {0b01000100, 0b01000100, 0b00101000, 0b00010000, 0b00101000, 0b01000100, 0b01000100, 0b00000000}, // X
    {0b01000100, 0b01000100, 0b00101000, 0b00010000, 0b00010000, 0b00010000, 0b00010000, 0b00000000}, // Y
    {0b01111100, 0b00000100, 0b00001000, 0b00010000, 0b00100000, 0b01000000, 0b01111100, 0b00000000}, // Z
    // 0-9
    {0b00111000, 0b01000100, 0b01001100, 0b01010100, 0b01100100, 0b01000100, 0b00111000, 0b00000000}, // 0
    {0b00010000, 0b00110000, 0b00010000, 0b00010000, 0b00010000, 0b00010000, 0b00111000, 0b00000000}, // 1
    {0b00111000, 0b01000100, 0b00000100, 0b00001000, 0b00010000, 0b00100000, 0b01111100, 0b00000000}, // 2
    {0b00111000, 0b01000100, 0b00000100, 0b00011000, 0b00000100, 0b01000100, 0b00111000, 0b00000000}, // 3
    {0b00001000, 0b00011000, 0b00101000, 0b01001000, 0b01111100, 0b00001000, 0b00001000, 0b00000000}, // 4
    {0b01111100, 0b01000000, 0b01111000, 0b00000100, 0b00000100, 0b01000100, 0b00111000, 0b00000000}, // 5
    {0b00111000, 0b01000100, 0b01000000, 0b01111000, 0b01000100, 0b01000100, 0b00111000, 0b00000000}, // 6
    {0b01111100, 0b00000100, 0b00001000, 0b00010000, 0b00100000, 0b00100000, 0b00100000, 0b00000000}, // 7
    {0b00111000, 0b01000100, 0b01000100, 0b00111000, 0b01000100, 0b01000100, 0b00111000, 0b00000000}, // 8
    {0b00111000, 0b01000100, 0b01000100, 0b00111100, 0b00000100, 0b01000100, 0b00111000, 0b00000000}  // 9
};

// 绘制一个像素
void draw_pixel(ushort *fb_data, int x, int y, ushort color)
{
    if (x >= 0 && x < screen_width && y >= 0 && y < screen_height)
    {
        fb_data[y * screen_width + x] = color;
    }
}

// 绘制一个矩形
void draw_rect(ushort *fb_data, int x, int y, int width, int height, ushort color)
{
    for (int i = x; i < x + width; i++)
    {
        for (int j = y; j < y + height; j++)
        {
            draw_pixel(fb_data, i, j, color);
        }
    }
}

// 绘制一个圆形
void draw_circle(ushort *fb_data, int center_x, int center_y, int radius, ushort color)
{
    for (int y = -radius; y <= radius; y++)
    {
        for (int x = -radius; x <= radius; x++)
        {
            if (x * x + y * y <= radius * radius)
            {
                draw_pixel(fb_data, center_x + x, center_y + y, color);
            }
        }
    }
}

// 绘制一个字符
void draw_char(ushort *fb_data, int x, int y, char c, ushort color)
{
    int index = -1;
    if (c >= 'A' && c <= 'Z')
    {
        index = c - 'A'; // A-Z 对应 0-25
    }
    if (c >= 'a' && c <= 'z')
    {
        index = c - 'a'; // A-Z 对应 0-25
    }
    else if (c >= '0' && c <= '9')
    {
        index = c - '0' + 26; // 0-9 对应 26-35
    }

    if (index >= 0 && index < 36)
    {
        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                if (font[index][i] & (1 << (7 - j)))
                {
                    draw_pixel(fb_data, x + j, y + i, color);
                }
            }
        }
    }
}

// 绘制字符串
void draw_string(ushort *fb_data, int x, int y, const char *str, ushort color)
{
    while (*str)
    {
        draw_char(fb_data, x, y, *str, color);
        x += 8; // 每个字符占 8 像素宽度
        str++;
    }
}

int main()
{
    // 打开 Framebuffer 设备
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

    // 清空屏幕（填充黑色）
    memset(fb_data, 0, fb_size);

    // 绘制渐变背景
    for (int y = 0; y < screen_height; y++)
    {
        for (int x = 0; x < screen_width; x++)
        {
            ushort color = RGB565(x, y, (x + y) / 2);
            draw_pixel(fb_data, x, y, color);
        }
    }

    // 绘制一个矩形
    draw_rect(fb_data, 20, 20, 50, 30, RGB565(255, 0, 0)); // 红色矩形

    // 绘制一个圆形
    draw_circle(fb_data, 100, 80, 20, RGB565(0, 255, 0)); // 绿色圆形

    // 绘制一段文字
    draw_string(fb_data, 10, 100, "Hello World", RGB565(255, 255, 255)); // 白色文字

    // 释放资源
    munmap(fb_data, fb_size);
    close(fb_fd);

    return 0;
}