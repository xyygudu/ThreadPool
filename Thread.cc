#include <sys/syscall.h>
#include <unistd.h>

#include "ThreadPool.h"
#include "Thread.h"

Thread::Thread(ThreadFunc func, ThreadFlag flag)
    : threadFlag_(flag)
    , threadId_(0)
    , threadFunc_(std::move(func))
{

}

void Thread::start()
{
    thread_ = std::thread([this]() {
        threadId_ = static_cast<pid_t>(::syscall(SYS_gettid));
        threadFunc_(shared_from_this());
    });
    detach();
}


void Thread::join()
{
    thread_.join();
}

bool Thread::joinable()
{
    return thread_.joinable();
}

void Thread::detach()
{
    thread_.detach();
}