'''
Author: ilikara 3435193369@qq.com
Date: 2025-03-11 03:00:45
LastEditors: Ilikara 3435193369@qq.com
LastEditTime: 2025-03-11 17:04:09
FilePath: /2k300_smartcar/udp_receive/slider.py
Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
'''
import tkinter as tk
import socket
import struct

# 配置参数
TARGET_IP = "192.168.43.238"  # 目标板卡IP
TARGET_PORT = 8888  # 目标端口


class UdpSliderApp:
    def __init__(self, master):
        self.master = master
        master.title("Dual Slider Control")

        # 创建UDP socket
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        # 滑块1
        self.slider1_frame = tk.Frame(master)
        self.slider1_frame.pack(pady=10)
        self.label1 = tk.Label(self.slider1_frame, text="Angle: 0.00")
        self.label1.pack(side=tk.LEFT)
        self.slider1 = tk.Scale(
            self.slider1_frame,
            from_=-7.0, to=7.0,
            resolution=0.01,
            orient=tk.HORIZONTAL,
            length=300,
            command=lambda v: self.send_values()
        )
        self.slider1.pack(side=tk.LEFT)

        # 滑块2
        self.slider2_frame = tk.Frame(master)
        self.slider2_frame.pack(pady=10)
        self.label2 = tk.Label(self.slider2_frame, text="Speed: 0.00")
        self.label2.pack(side=tk.LEFT)
        self.slider2 = tk.Scale(
            self.slider2_frame,
            from_=-20.0, to=20.0,  # 示例范围可调
            resolution=0.1,
            orient=tk.HORIZONTAL,
            length=300,
            command=lambda v: self.send_values()
        )
        self.slider2.pack(side=tk.LEFT)

        # 退出按钮
        self.exit_btn = tk.Button(
            master,
            text="Exit",
            command=master.quit,
            bg="#ff4444",
            fg="white"
        )
        self.exit_btn.pack(pady=20)

        self.reset_btn = tk.Button(
            self.slider2_frame,
            text="Reset to 0",
            command=self.reset_slider2,
            bg="#ff6666",
            fg="white",
            width=8
        )
        self.reset_btn.pack(side=tk.LEFT, padx=10)

    def send_values(self):
        """打包并发送两个浮点数"""
        try:
            val1 = float(self.slider1.get())
            val2 = float(self.slider2.get())

            # 更新标签
            self.label1.config(text=f"Angle: {val1:.2f}")
            self.label2.config(text=f"Speed: {val2:.2f}")

            # 打包为网络字节序的两个float
            data = struct.pack('!ff', val1, val2)  # !表示网络字节序

            # 发送UDP数据
            self.sock.sendto(data, (TARGET_IP, TARGET_PORT))
        except Exception as e:
            print(f"发送错误: {str(e)}")

    def reset_slider2(self):
        self.slider2.set(0.0)  # 设置滑块物理位置
        self.label2.config(text="Slider2: 0.00")  # 直接更新显示
        self.send_values()  # 手动触发发送


if __name__ == "__main__":
    root = tk.Tk()
    app = UdpSliderApp(root)
    root.mainloop()
    app.sock.close()
