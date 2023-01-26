## 话不多说，先放代码

[https://github.com/xyygudu/ThreadPool](https://github.com/xyygudu/ThreadPool)

## 多线程程序一定好吗？

不一定，要看当前程序的类型来判断，程序的类型有IO密集型和CPU密集型。

- IO密集型：涉及一些IO操作的指令，比如设备、文件、网络等，这些IO操作很容易阻塞程序，也是比较耗时的操作。
- CPU密集型：指令主要是用于计算的。

对于多核计算机来说，这两种类型的程序使用多线程都是有必要的，但是如果是**单线程环境，IO密集型程序才适合多线程**，比如某个任务处于网络阻塞状态，那就可以切换其他线程去处理其他任务；CPU密集型程序不适合多线程，因为就算是多个线程，这些计算指令依然还是这个核心来处理，除了执行指令外，还多了一些线程切换开销。

## 线程数量怎么确定？

- 线程的创建核销毁是非常"重"的操作，比如PCB的创建。
- 线程本身占用大量内存。每个线程都有自己独立的栈空间。
- 线程的上下文切换非常耗时。如果cpu大量时间花在切换线程，就没时间处理任务了。
- 大量线程同时唤醒会使得系统出现瞬时负载量过大导致宕机。

**线程数量一般都是按照当前CPU的核心数量来确定的。**

## 线程池介绍

### 线程池基本原理

为了任务得到及时的处理（所谓任务可以理解为待执行的函数），把待处理的任务都放入线程池的任务队列中，线程池的多个线程就可以从该任务队列中取出任务并执行，线程再取出任务的同时，用户也可以向任务队列中添加新的任务，就好像用户只管把要执行的任务告诉线程池，线程池内部线程处理完成后返回用户处理结果。大致原理如下图。
![image.png](https://cdn.nlark.com/yuque/0/2023/png/27222704/1674735325442-b2fadb6c-4091-4c17-b12f-78eb4c181c87.png#averageHue=%23998476&clientId=u0ee95a07-4455-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=557&id=u49939487&margin=%5Bobject%20Object%5D&name=image.png&originHeight=696&originWidth=1108&originalType=binary&ratio=1&rotation=0&showTitle=false&size=50875&status=done&style=none&taskId=uf7503347-6b66-40a1-b82e-757490cb966&title=&width=886.4)
有读者可能疑问，函数不是调用了就执行了吗，怎么还可以放入什么任务队列？是的，C++11提供了function模板，可以把函数当作一个可调用对象（可理解为一个变量）先保存起来，甚至可以把这个可调用对象存入容器中（比如queue中），然后可以再合适的时机把该调用对象取出来，然后执行。相当于把要执行的函数保存后延迟调用。

### 线程池的优势

- 服务进程启动之初，就事先在线程池重创建好了很多线程，当有任务需要执行时，就可以直接选择一个线程马上执行任务，而不必像之前一样，执行任务还需要额外时间先创建好线程。
- 任务执行完成后，不用释放线程，而是把线程归还到线程池中，便于为后续任务提供服务。

### 线程池应该具备的功能

- 线程池应该是可配置的。比如配置保留线程的数量，线程池最多能开启的线程数量等。
- 具有保留线程和动态线程。保留线程常驻在线程池中，只要线程池没有退出或者被回收，则常驻线程就不会终止；动态线程根据任务量的大小动态创建和销毁，当保留线程不能及时处理任务队列的任务时，增加动态线程，当任务队列为空，许多动态线程处于等待状态时，终止动态线程。
- 能够执行各种不一样的任务（参数不同的函数），并且能够获取执行结果
- 可获取当前线程池中线程的总个数，空闲线程的个数。
- 可以控制任务队列的任务数量，防止过多任务占用大量内存。
- 开启线程池功能的开关。关闭时确保当前任务能够执行完成，并丢弃剩下没有执行的任务，或者停止向任务队列中添加新任务，把剩余的任务执行完毕后退出。

## 线程池的实现

### 前置知识

本线程池使用C++11编写，主要使用到了以下的C++11特性，适合初学者入门。
std::function和std::bind、可变参数函数模板、package_task和future、原子类型（atomic）、条件变量、互斥锁、智能指针、lambda表达式、移动语义、完美转发

### 类的划分

线程池中共分为两个类，一个Thread类和一个ThreadPool类，Thread类主要是用于保存线程的一些基本属性、比如线程标志（是保留线程还是动态线程）、线程ID、std::thread等等，涉及到的方法主要是启动线程（start函数）；ThreadPool类主要用于管理Thread类和任务队列，包括动态添/删除线程、向任务队列存取任务等等。这两个类的头文件主要代码如下：

```cpp
class Thread : public std::enable_shared_from_this<Thread>
{

public:
    using ThreadPtr = std::shared_ptr<Thread>;
    using ThreadFunc = std::function<void(ThreadPtr)>; 
    enum class ThreadFlag { kReserved, kDynamic };

    explicit Thread(ThreadFunc func, ThreadFlag flag = ThreadFlag::kReserved);
    ~Thread() = default;
    // 获取线程id
    pid_t getThreadId() { return threadId_; }  
    void start();               // 启动线程

private:
    ThreadFlag threadFlag_;     // 线程标志
    pid_t threadId_;            // 线程id
    std::thread thread_;        // 线程
    ThreadFunc threadFunc_;     // 线程执行函数
};
```

```cpp
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
```

### 功能实现

#### ThreadPool构造函数做了什么？

构造函数主要初始化一些配置，比如保留线程的数量、线程池最大的线程数量等等。

#### 线程池启动(start)时做了什么？

线程池调用start函数后，会根据配置中给定的保留线程数量向线程容器`workThreads_`中添加保留线程，添加保留线程实际上就是创建Thread对象，同时启动这些线程，Thread类也有个start函数，这个函数正式创建`std::thread`对象，并为`std::thread`绑定要运行的回调函数（即ThreadPool类的threadFunc函数），线程池启动后，此时所有保留线程都处于运行状态，都等待任务的到来。至于为什么要用智能指针封装Thread类，见回答“Thread类为什么要用智能指针”。

```cpp
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
```

#### 子线程是怎么处理任务的？

子线程执行的是一个死循环，只有退出的线程的退出标志`quit_=true`时才会退出循环，子线程运行结束。子线程循行在ThreadPool的threadFunc函数中，该函数工作步骤如下：

1. 获取互斥锁。因为多个线程要从任务队列中取出任务，因此，任务队列`tasks_`就是临界资源，子线程访问临界资源需要加锁
2. 根据条件变量`notEmptyCV_`判断任务队列`tasks_`是否为空，只有`tasks_`为空并且线程池设置了退出标志（即`quit_=true`）时，才会将子线程自己从线程池的`workThreads_`中移除，这样做的目的是为了让线程池尽管设置了退出标志，但是只要任务队列不为空就会继续执行，换句话说，这是为了防止线程池退出时还有任务没有处理。其余情况均会从任务队列中取出任务并执行。

当子线程满足退出条件时，即`quit_ && tasks_.empty()`为`true`时，除了把自己从线程池中移除外，还有通知条件变量`exitCV_`，目的是为了让所有线程都从线程池中移除后再析构线程池对象，具体见**“**线程池怎么安全退出？**”**。
主要代码如下（以保留线程为例，动态线程只是多了一个超时回收）：

```cpp
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
                // 动态线程处理过程
            } 
        }
        task();                         // 执行任务
        std::cout << "thread(id: " << threadPtr->getThreadId() << ") is running task" << std::endl;
    }
}
```

#### 如何向线程池中提交任务？

提交任务就是把任务打包，然后添加进任务队列中。具体过程如下：

1. 打包任务，即把用户提交的任务打包成可调用对象，以便添加进任务队列。`submitTask`函数使用了可变参数模板，目的是为了为了能够处理用户传入的各种任务（即要能处理参数不固定的函数）
2. 添加任务到任务队列。首先，在把打包好的任务添加进任务队列`tasks_`之前要先获取锁，然后，通过条件变量`notFullCV_`判断任务队列中的任务是否已经达到了上限，如果达到了就处于等待在此处，直到任务队列满足条件为止（此处似乎有个小bug：如果线程池设置了退出标志，就不应该继续向任务队列中添加任务，但是此处没有判断线程池是否退出，也就是说，尽管线程池设置了退出标志，只要有任务还在向任务队列中添加，线程池就无法退出，因为线程池能够成功退出的唯一条件是`quit_=true && tasks_.empty()=true`），紧接着，`notEmptyCV_`通知其他线程任务队列不为空。
3. 增加动态线程。只要处于等待中的线程数量小于要处理的任务数量，就创建新的动态线程。

```cpp
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
        tasks_.emplace([&]() {(*task)();});
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
```

#### 线程池怎么安全退出？

线程池退出时，最好等待所有子线程都退出后再退出自己。这是因为子线程会使用主线程（线程池）的资源，比如修改`waitingThreadNum_`变量等等，如果线程池对象已经析构了，而子线程还在执行，那后果是无法想象的，为了做到这一点，我们来看看`stop`函数做了些什么。

1. 首先把`quit_`设置为`ture`，这一步非常关键，子线程在死循环中会检查`quit_`标志，当`quit_=true`时，他们自己会做相应的退出处理。
2. 然后`notEmptyCV_`通知所有子线程，这是因为有的线程因为任务队列为空，处于等待状态，无法继续执行，因此这里`notEmptyCV_.notify_all()`并不是真的任务队列不为空，而是为了唤醒处于等待中的子线程，让他们正常退出。
3. 等待所有子线程都把自己从线程池中移除。每一个子线程满足结束条件后，都会把自己从`workThreads_`中移除，只有当`workThreads_.size()==0`时，才能说明所有子线程都退出了，此时，最后一个子线程把自己从`workThreads_`中移除后，会通过`exitCV_.notify_all()`告诉线程池不必等待了，因为此时，`workThreads_`大小已经为0，线程池退出成功。

或许有读者疑问：不可以通过让workThreads_中的每一个子线程调用`join()`就好了吗，为什么要通过条件变量`exitCV_`？回答见“子线程是如何被回收的”。

```cpp
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
```

## 疑问解答

### 何时启动动态线程？

如果任务队列只要有任务就启动了动态线程似乎不太合适，因为很可能保留线程还在创建过程中，此时任务队列可能有几个任务处理不过来，当保留线程创建完毕后，很可能就足以处理任务队列中剩余的任务，所以任务队列有多少任务时启动动态线程？
答：可以通过判断任务队列中任务的数量和空闲子线程数量相比较，如果等待中的线程数量比任务队列中的数量少，则可以创建动态线程，不过似乎也存在：保留线程还在创建过程中，此时任务队列数量大于已经等待中的线程数量，导致不必要的动态线程的创建，不过似乎问题不大。这里只是为了引发读者思考。

### 子线程是如何被回收的？

子线程运行的函数执行结束后（退出了循环），何时回收线程的资源（join或者detach），何时释放线程所属的类Thread？
答：在start线程时就调用了`detach`来分离线程，虽然使用`detach`要十分慎重，但是这里没办法，`start`函数不能调用`join`，因为子线程不退出，主线程就会阻塞在`join`函数，导致`start`都执行不能结束，那还提交个毛的任务。那就不能在其他函数中调用`join`吗，比如`stop`函数中依次遍历`workThreads_`中的子线程进行`join`？不行，因为涉及到动态线程要回收，假如某个子线程等待超时了要回收，子线程会把自己从`workThreads_`中移除，后续线程池遍历`workThreads_`时，就无法找到这个刚移除的子线程了，因而无法从`workThreads_`中取出该线程，为其线程调用`join`了。所以本文在`start`函数中创建的线程调用了`detach`来回收线程。有读者又有疑问，可否在子线程在把自己从`workThreads_`中移除时顺便join或者detach？不行，因为子线程不可以join或者dtach自己，否则会出错）

### Thread类为什么要用智能指针？

答：Thread是要被添加进线程池的容器的，相比于添加Thread类到容器中，显然没有添加一个指针效率高，但是如果是添加的裸指针Thread*，那就需要手动new Thread对象，如果忘记delete会导致内存泄露，因此，采用智能指针包裹Thread，共享指针和独占指针均可

### 为什么要让所有子线程退出后再销毁线程池

因为子线程使用了主线程（线程池）的资源（如quit_变量），如果线程池先被析构了，那子线程很可能还在运行，也就是可能会访问已经被销毁的资源，导致子线程出现异常而退出。这也是我比较推荐线程要join而谨慎detach的原因。

### 如何让所有线程退出后主线程退出？

方法一：引入新的条件变量`exitCV_`。即我使用的方法：在线程池的stop函数中用一个`exitCV_`专门用于查看`worThreads_`是否为空，不为空就一直等待，这要求每个子线程在时间循环中满足结束条件时，直接把自己从`workThreads_`中移除，同时`exitCV_.notify_all();`，此时尽管子线程的`exitCV_`进行了notify，但是只要`worThreads_`不为空，就依然还会处于等待状态，直到所有子线程把自己从`workThreads_`中移除。
方法二：为每一个Thread类添加一个是否退出标志，假设是kStop。此方法在子线程退出时，并不从`workThreads_`中移除自己，而是只把自己标记为kStop状态即可退出事件循环，那什么时候join或者detach呢，那就是在线程池退出时（调用stop时）先遍历`workThreads_`，把每个线程join或者detach一下，然后再清空`workThreads_`就行。优点：不必引入条件变量`exitCV_`，可以通过join的方式退出子线程，join更安全，缺点：子线程退出，但是子线程所属Thread类没有得到释放，而且给后续动态添加子线程带来了更复杂的情况：`workThreads_`中线程处于stop状态的应该优先被启用，如果仍需要增加线程，再创建新的线程。

