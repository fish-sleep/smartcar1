# demo1
add_executable(demo1 demo1.cpp)

# 链接 OpenCV 库
target_link_libraries(demo1 common_lib ${OpenCV_LIBS})