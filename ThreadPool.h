#pragma once

#include <iostream>
#include <functional>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <list>
#include <queue>
#include <future>
#include <atomic>

#include "Config.h"
#include "Thread.h"

class ThreadPool
{
public:
    using Task = std::function<void()>;                     // 定义任务回调函数
    using Seconds = std::chrono::seconds;
    using ThreadPoolLock = std::unique_lock<std::mutex>;
    using ThreadPtr = std::shared_ptr<Thread>;              // 线程指针类型
    using ThreadFlag = Thread::ThreadFlag;

    explicit ThreadPool();
    ~ThreadPool();

    void start();                                           // 开启线程池
    void stop();                                            // 停止线程池
    template<typename Func, typename... Args>               // 给线程池提交任务
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>;
    
    int getWatingThreadNum();
    size_t getThreadNum();

private:
    void addThread(ThreadFlag flag = ThreadFlag::kReserved);// 向线程池中添加一个新线程
    void removeThread(pid_t id);
    void threadFunc(ThreadPtr threadPtr);                   // 线程执行函数

    std::list<ThreadPtr> workThreads_;                      // 存放工作线程，使用智能指针是为了能够自动释放Thread
    std::mutex threadMutex_;                                // 工作线程互斥锁
    std::queue<Task> tasks_;                                // 任务队列
    std::mutex taskMutex_;                                  // 互斥锁（从队列取出/添加任务用到）
    std::condition_variable notFullCV_;                     // 任务队列不满
    std::condition_variable notEmptyCV_;                    // 任务队列不空
    std::condition_variable exitCV_;                        // 没有任务时方可线程池退出

    // std::atomic_int taskNum_;                            // 任务队列中未处理的任务数量
    std::atomic_int waitingThreadNum_;                      // 处于等待中的线程数量
    std::atomic_uint curThreadNum_;                         // 当前线程数量

    std::atomic_bool quit_;                                 // 标识是否退出线程池

    Config config_;                                         // 存储线程池配置
};

// 函数模板的定义一般写在头文件
// 打包任务，放入任务队列里面
template <typename Func, typename... Args>
inline auto ThreadPool::submitTask(Func &&func, Args &&...args) -> std::future<decltype(func(args...))>
{
    using ReturnType = decltype(func(args...));
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
    std::future<ReturnType> result = task->get_future();
    size_t taskSize = 0;
    {  
        // 获取锁
        ThreadPoolLock lock(taskMutex_);
        notFullCV_.wait(lock, [this]()->bool { 
            bool notfull = false;
            if (tasks_.size() < (size_t)config_.MAX_TASK_NUM) notfull = true;
            else std::cout << "task queue is full..." << std::endl;
            return notfull;
        });
        // 如果任务队列不满，则把任务添加进队列
        tasks_.emplace([task]() {(*task)();});
        taskSize = tasks_.size();
        notEmptyCV_.notify_one();
    }

    // 根据任务队列中任务的数量以及空闲线程数量决定要不要新增线程
    if ((size_t)waitingThreadNum_ < taskSize && curThreadNum_ < config_.MAX_THREAD_NUM)
    {
        addThread(ThreadFlag::kDynamic);
    }
    return result;
}
