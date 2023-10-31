#pragma once

#include "noncopyable.h"

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>

class Thread : private noncopyable
{
public:
    using ThreadFunc = std::function<void()>;       // 同样，为什么认为线程的业务函数都是没有返回值且不接收参数的函数类型
                                                    // 因为参数会通过bind绑定，这样就不需要传入参数了

    explicit Thread(ThreadFunc func, const std::string& name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }

    pid_t tid() const { return tid_; }

    const std::string& name() const { return name_; }

    static int numCreated() { return numCreated_; }

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;       // 指向线程对象
    pid_t tid_;             // 线程id，这是相当于用top命令查看的线程id，而不是pthread_tid
    ThreadFunc func_;       // 线程函数对象
    std::string name_;      // 线程的名字，打印信息时候用的

    static std::atomic_int numCreated_;       // 记录总共有多少个线程
};