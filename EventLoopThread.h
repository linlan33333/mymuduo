#pragma once

#include "noncopyable.h"
#include "Thread.h"

#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <string>

class EventLoop;

// 一个EventLoopThread对象包含一个Thread类和一个EventLoop类
// 因此叫one loop per thread
// mainLoop直接在主线程中就创建了，这里的EventLoop都是subLoop
class EventLoopThread : private noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(), const std::string& name = std::string());

    ~EventLoopThread();

    EventLoop* startLoop();

private:
    void threadFunc();

    EventLoop* loop_;
    bool exiting_;  // 这个变量貌似没用
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;       // 用于创建subLoop对象后对该对象做一些初始化操作
};