#include "Timer.h"

Timer::Timer(int interval_ms, std::function<void()> task)
    : interval(interval_ms), task(task), running(false) {}

Timer::~Timer()
{
    stop(); // Ensure the timer is stopped in the destructor
}

void Timer::start()
{
    running = true;
    worker = std::thread(&Timer::run, this);
}

void Timer::stop()
{
    running = false;
    if (worker.joinable())
    {
        worker.join();
    }
}

void Timer::run()
{
    while (running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        if (running)
        {
            std::thread(task).detach();
        }
    }
}
