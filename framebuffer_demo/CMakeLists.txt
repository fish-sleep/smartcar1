# demo1
add_executable(framebuffer_demo framebuffer_demo.cpp)

# 链接 OpenCV 库
target_link_libraries(framebuffer_demo common_lib ${OpenCV_LIBS})