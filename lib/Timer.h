#ifndef TIMER_H
#define TIMER_H

#include <thread>
#include <chrono>
#include <atomic>
#include <functional>
#include <iostream>

class Timer
{
public:
    Timer(int interval_ms, std::function<void()> task);
    ~Timer();

    void start();
    void stop();

private:
    void run();

    int interval; // Interval in milliseconds
    std::function<void()> task;
    std::atomic<bool> running;
    std::thread worker;
};

#endif // TIMER_H
