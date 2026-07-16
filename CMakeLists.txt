# 包含交叉编译工具链
include(cross.cmake)

# 设置CMake的最低版本要求
cmake_minimum_required(VERSION 3.5.0)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pthread -Wall")  # 对于 C++ 编译器
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall")              # 对于 C 编译器

# 定义项目名称和版本，并指定使用C和C++语言
project(smartcar_demo VERSION 0.1.0 LANGUAGES C CXX)

# 设置OpenCV的安装路径
set(OpenCV_DIR /home/y6/桌面/opencv/build)

# 查找OpenCV库，确保安装了所需的依赖
find_package(OpenCV REQUIRED)

# 包含OpenCV的头文件路径
include_directories(${OpenCV_INCLUDE_DIRS})
message(STATUS "OpenCV Include Directories: ${OpenCV_INCLUDE_DIRS}")

# 包含项目的自定义库路径
include_directories(lib)

# 查找源文件所在的目录
aux_source_directory(src DIR_SRCS)

# 将 src 目录下的源文件编译为静态库
add_library(common_lib STATIC ${DIR_SRCS})

# 添加子目录
add_subdirectory(main)   # 主程序

# add_subdirectory(demo1)  # demo1
# add_subdirectory(framebuffer_demo)
# add_subdirectory(opencv_demo1)
# add_subdirectory(opencv_demo2)
# add_subdirectory(opencv_demo3)
# add_subdirectory(encoder_demo)
# add_subdirectory(jy62_demo)
# add_subdirectory(key_demo)
# add_subdirectory(wonderEcho_demo)
# add_subdirectory(udp_receive)
# add_subdirectory(image_test)