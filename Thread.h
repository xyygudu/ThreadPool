#pragma once

#include <iostream>
#include <thread>
#include <memory>
#include <functional>


class Thread : public std::enable_shared_from_this<Thread>
{

public:
    using ThreadPtr = std::shared_ptr<Thread>;
    using ThreadFunc = std::function<void(ThreadPtr)>; 
    enum class ThreadFlag { kReserved, kDynamic };

    explicit Thread(ThreadFunc func, ThreadFlag flag = ThreadFlag::kReserved);
    ~Thread() = default;

    // 线程标志
    ThreadFlag getThreadFlag() { return threadFlag_; };
    void setThreadState(ThreadFlag flag) { threadFlag_ = flag; };

    // 获取线程id
    pid_t getThreadId() { return threadId_; }
    
    void start();               // 启动线程
    void join();
    bool joinable();
    void detach();

private:
    ThreadFlag threadFlag_;     // 线程标志
    pid_t threadId_;            // 线程id
    std::thread thread_;        // 线程
    ThreadFunc threadFunc_;     // 线程执行函数
};

