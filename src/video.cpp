#include "video.h"

Video::Video(const std::string &filename, double fps) 
    : timer(static_cast<int>(1000 / fps), std::bind(&Video::streamCapture, this)), cap(filename)
{
    streamCapture();
}

Video::~Video(void)
{
    timer.stop();
}

void Video::streamCapture(void) 
{
    frameMutex.lock();
    cap.read(frame);
    frameMutex.unlock();
}
    