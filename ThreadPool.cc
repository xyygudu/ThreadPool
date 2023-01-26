
#include "ThreadPool.h"


ThreadPool::ThreadPool()
    : config_(Config())
{
    // taskNum_.store(0);
    waitingThreadNum_.store(0);
    curThreadNum_.store(0);
    quit_.store(false);
}

ThreadPool::~ThreadPool() 
{
    if (!quit_)
    {
        stop();
    }
}

void ThreadPool::start()
{
    quit_ = false;
    size_t reservedThreadNum = config_.RESERVED_THREAD_NUM;
    std::cout << "init thread num: " << reservedThreadNum << std::endl;
    while (reservedThreadNum--)
    {
        addThread(); // 默认创建保留线程(threadFlag_=kReserved)
    }
}

void ThreadPool::stop()
{
    // 线程池退出时，线程只有两种状态，要么wait要么run，所以要把处于wait状态的线程唤醒
    quit_ = true;
    // 唤醒所有处于kWaiting的线程
    ThreadPoolLock lock(taskMutex_);
    notEmptyCV_.notify_all();     
    // 等待所有线程退出并移出所有线程
    exitCV_.wait(lock, [&]()->bool { return workThreads_.size() == 0; });
    std::cout << "all thread remove from workThreads_ successfully" << std::endl;  
}

void ThreadPool::addThread(ThreadFlag flag)
{
    // 默认创建保留线程(threadFlag_=kReserved)
    ThreadPtr newThreadPtr(new Thread(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1), flag));
    newThreadPtr->start();                  // 启动线程，线程自动执行threadFunc
    // 如果不把newThreadPtr添加到workThread_，函数运行结束后，newThreadPtr的引用计数为0，资源会被释放
    ThreadPoolLock lock(threadMutex_);
    workThreads_.emplace_back(newThreadPtr);
    curThreadNum_++;
}

void ThreadPool::removeThread(pid_t id)
{
    ThreadPoolLock lock(threadMutex_);
    for (auto iter = workThreads_.begin(); iter != workThreads_.end(); ++iter)
    {
        if ((*iter)->getThreadId() == id) 
        {
            workThreads_.erase(iter);
            curThreadNum_--;
            // std::cout << "subthread remove itself: workThreads_.size(): " << workThreads_.size() << std::endl;
            return;
        }
    }
}

void ThreadPool::removeAllThreads()
{
    ThreadPoolLock lock(threadMutex_);
    // for (auto iter = workThreads_.begin(); iter != workThreads_.end(); ++iter)
    // {
    //     if ((*iter)->joinable()) { (*iter)->join(); }
    //     else { (*iter)->detach(); }
    // }
    workThreads_.clear();
    curThreadNum_.store(0);
}

int ThreadPool::getWatingThreadNum()
{
    return waitingThreadNum_.load();
}

size_t ThreadPool::getThreadNum()
{
    return curThreadNum_;
}

void ThreadPool::threadFunc(ThreadPtr threadPtr)
{
    for(;;)
    {
        Task task;
        {
            ThreadPoolLock lock(taskMutex_);
            std::cout << "tid:" << threadPtr->getThreadId() << " try to get one task..." << std::endl;
            waitingThreadNum_++;
            // 只要任务队列不为空或者线程池要退出，就不阻塞在wait
            if (threadPtr->getThreadFlag() == Thread::ThreadFlag::kReserved)
            {
                notEmptyCV_.wait(lock, [this]() { return quit_ || !tasks_.empty(); });
                waitingThreadNum_--;
                // 只有当线程池quit_=true并且没有任务要处理的情况下，才会真正退出线程池
                if (quit_ && tasks_.empty())
                {
                    removeThread(threadPtr->getThreadId());
                    exitCV_.notify_all();
                    std::cout << "Reserved thread<" << threadPtr->getThreadId() << "> exit" << std::endl;
                    return;
                }
                else{
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
            }
            else
            {
                notEmptyCV_.wait_for(lock, Seconds(config_.WAIT_SECOND), [this]() { return quit_ || !tasks_.empty(); });
                waitingThreadNum_--;
                if (!quit_ && tasks_.empty())           // 为空说明一定是动态线程超时了
                {
                    removeThread(threadPtr->getThreadId()); 
                    exitCV_.notify_all();
                    std::cout << "Dynamic thread<" << threadPtr->getThreadId() << "> exit" << std::endl;
                    return;
                }
                else if (quit_ && tasks_.empty())       // 线程池要退出，并且任务已经执行完毕，则可以正常退出
                {
                    removeThread(threadPtr->getThreadId()); 
                    exitCV_.notify_all();
                    std::cout << "Dynamic thread<" << threadPtr->getThreadId() << "> exit" << std::endl;
                    return;
                }
                else 
                {
                    task = std::move(tasks_.front());
                    tasks_.pop();
                    notFullCV_.notify_all();
                }
            } 
        }
        task();                         // 执行任务
        std::cout << "thread(id: " << threadPtr->getThreadId() << ") is running task" << std::endl;
    }
}
