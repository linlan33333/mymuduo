#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , mutex_()
    , cond_()
    , callback_(cb)
{

}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;

    if (loop_ != nullptr) {
        loop_->quit();      // 等待loop结束循环
        thread_.join();     // 等待线程退出
    }
}

// 为什么返回EventLoop类型的指针，因为EventLoopThreadPool对象需要操控所有的EventLoop对象，所以需要
// 从这里获取这些对象的地址
EventLoop *EventLoopThread::startLoop()
{
    thread_.start();        // 启动线程执行线程函数
                            // 注意线程函数本质执行的是下面的threadFunc

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_ == nullptr) {
            // 等待线程函数创建好EventLoop对象
            cond_.wait(lock);
        }

        loop = loop_;
    }

    return loop;                   
}

// 这个方法是在thread_里面运行，也就是线程要干的业务函数
void EventLoopThread::threadFunc() 
{
    // 在线程中创建一个独立的eventLoop，也就是one loop per thread
    EventLoop loop;

    if (callback_) {
        callback_(&loop);       // 对刚创建的EventLoop对象做一些初始化操作
    }

    // 为什么要上锁，因为startLoop需要返回loop对象，现在loop对象还没创建好呢，需要进程同步
    // 这里的进程同步就是同一个类不同成员函数之间的同步
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();        // 开始一直epoll_wait

    std::unique_lock<std::mutex> lock(mutex_);      // 能到这一步说明loop调用quit函数退出循环了
                                                    // 这时线程函数需要做一些扫尾工作
    loop_ = nullptr;        // 防止其他线程用这个loop_，给loop_添加一些上级回调操作
}
