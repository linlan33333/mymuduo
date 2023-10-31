#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    // 注意线程调用了join函数就不能调用detach函数
    if (started_ && !joined_) {
        thread_->detach();
    }
}

// 一个线程对象，记录的就是一个新线程的详细信息
void Thread::start()
{
    started_ = true;
    sem_t sem;      // 定义一个信号量，让线程函数获取到pid之后才能让主线程继续往下执行
    sem_init(&sem, false, 0);
    // 开启线程，执行线程函数
    thread_ = std::make_shared<std::thread>([&] () {
        // 获取当前的tid
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        // 执行业务函数，其实是EventLoopThread里面的threadFunc函数
        func_();
    });

    // 这里必须等待获取上面新创建的线程的tid值
    // 因为线程对象调用start()函数后，认为就可以获取该线程的所有详细信息了，但如果
    // tid_还没有赋值，线程函数还没运行，那么就访问不了该线程对象的tid_，不符合逻辑
    sem_wait(&sem);
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty()) {
        char buf[32] = {0};

        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}
