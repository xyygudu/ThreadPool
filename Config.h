#pragma once
#include <chrono>

class Config
{
public:
    const unsigned int MAX_THREAD_NUM = 8;          // 最大线程数量
    const unsigned int RESERVED_THREAD_NUM = 4;     // 保留的线程数目（这些线程只在线程池销毁的时候回收）
    int64_t WAIT_SECOND = 5;                        // 非保留线程空闲等待时长（超时后，线程会被回收）
    uint MAX_TASK_NUM = 1000;                         // 任务队列中最大的任务数量

    Config() {}
    ~Config() {}
};


