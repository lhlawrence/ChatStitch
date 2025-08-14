#include <iostream>
#include <stdlib.h>
#include <string>
#include <chrono>
#include <vector>

class Timer
{
private:
    std::chrono::_V2::system_clock::time_point now;
    std::chrono::_V2::system_clock::time_point last_time;
    std::chrono::nanoseconds dt;
    int dtUs;

    std::vector<std::string> names;
    std::vector<int> times;

public:
    Timer(){
        last_time = std::chrono::high_resolution_clock::now();
    }

    ~Timer() {}

    void reset(){
        now = std::chrono::high_resolution_clock::now();
        last_time = now;
        times.clear();
        names.clear();
    }

    void update(std::string name){
        now = std::chrono::high_resolution_clock::now();
        dt = now - last_time;
        last_time = now;
        dtUs = std::chrono::duration_cast<std::chrono::microseconds>(dt).count();  // 微秒
        times.push_back(dtUs);
        names.push_back(name);
    }


    int print(std::string text){
        printf("[ %s ] ", text.c_str());
        int totalUs = 0;
        for (int i=0; i<times.size(); i++)
        {
            printf(" %s: dt = %.3f ms, ", names[i].c_str(), static_cast<float>(times[i])/1000.0);
            totalUs += times[i];
        }
        printf(" total = %.3f ms\n", static_cast<float>(totalUs)/1000.0);
        reset();
        return totalUs;
    }
};