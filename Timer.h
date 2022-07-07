#pragma once

#include <chrono>
#include <iostream>
#include <string>

using namespace std::chrono;

class Timer
{
public:
    Timer(std::string_view name)
        : TimerName(name), Start(steady_clock::now())
    {}

    void Stop()
    {
        auto End = steady_clock::now();
        duration<double> elapsed_seconds = End - Start;
        std::cout << TimerName << " time consume: " << elapsed_seconds.count() << "s.\n";
    }

    double Timing()
    {
        auto End = steady_clock::now();
        duration<double> elapsed_seconds = End - Start;
        return elapsed_seconds.count();
    }

private:
    std::string TimerName;
    time_point<steady_clock> Start;
};