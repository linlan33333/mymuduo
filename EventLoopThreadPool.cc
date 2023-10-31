#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "EventLoopThread.h"

#include <cstring>

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0)
{
}

// 析构函数什么也不用做
EventLoopThreadPool::~EventLoopThreadPool()
{
    // 不需要delete掉baseLoop_指针，因为EventLoop是创建在栈上的对象，自动释放，具体看
    // EventLoopThread::threadFunc()函数是怎么创建EventLoop对象的
}

void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;

    for (int i = 0; i < numThreads_; i++) {
        // 给线程起名字
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread* t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread> (t));   // 用智能指针管理EventLoopThread指针，为的是该EventLoopThreadPool类
                                                                    // 析构时也连带着把所有EventLoopThread对象都析构了
        loops_.push_back(t->startLoop());   // 这一行代码干了三样事：创建线程，线程创建并绑定一个新的EventLoop，
                                            // 最后把它添加到loops_中
    }

    // 用户没有设置线程数量，那么主线程既要监听新用户的连接，又要处理用户发来的请求
    if (numThreads_ == 0) {
        cb(baseLoop_);
    }
}

// 如果工作在多线程中，baseLoop_默认以轮询的方式分配channel给subloop
// 这种轮询方式就是雨露均沾方式。这种方式会不会出现子线程负载程度不同？
// 不会，因为当连接数成千万级时，由大数定律可知每个连接的持续时间呈正态分布，
// 因此采用轮询算法后每个子线程上的连接持续时间也呈现相同的正态分布，
// 数学期望都相等，平均的连接时间相等，子线程的负载程度也相等
// 这完美满足负载均衡的要求，要的就是这个效果
EventLoop *EventLoopThreadPool::getNextLoop()
{
    EventLoop* loop = baseLoop_;

    // 用户设置了线程数量，有其他的EventLoop
    if (!loops_.empty()) {
        loop = loops_[next_];
        ++next_;    // 轮询一次后就轮到下一个
        if (next_ >= loops_.size()) {
            next_ = 0;
        }
    }

    // 如果没有设置线程数量，那么channel只能都分配给baseLoop_了
    return loop;
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty()) {   // 没有设置线程数量的结果
        return std::vector<EventLoop*> (1, baseLoop_);
    }
    else {
        return loops_;
    }
}
