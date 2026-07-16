# demo1
add_executable(encoder_demo encoder_demo.cpp)

# 链接 OpenCV 库
target_link_libraries(encoder_demo common_lib ${OpenCV_LIBS})