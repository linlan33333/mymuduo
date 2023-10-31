#pragma once

#include "noncopyable.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

/**
 * 这个线程池离谱在没有采用生产者消费者模型，也就没有任务队列之类的东西
 * 所以有别于传统的线程池，它怎么让线程领取任务的？直接轮询，每个线程依次领取
 * 轮到谁谁领取，所以不需要任务队列
*/
class EventLoopThreadPool : private noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);

    ~EventLoopThreadPool();

    // 设置线程池中的线程数量
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    // 如果工作在多线程中，baseLoop_默认以轮询的方式分配channel给subloop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_; }

    const std::string name() const { return name_; }
private:
    EventLoop* baseLoop_;       // 最开始用来监听新用户连接的loop
    std::string name_;
    bool started_;
    int numThreads_;            // 设置线程数量
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;     // 控制EventLoopThread对象们
    std::vector<EventLoop*> loops_;                             // 控制EventLoop对象们
};