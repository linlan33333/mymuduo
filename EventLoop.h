#pragma once

#include <iostream>
#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

// 只用到了它们的类型做指针，因此不必包含头文件
class Channel;
class Poller;

// 事件循环类，主要包含了两个大模块  Channel  Poller(epoll的抽象)
class EventLoop : private noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环，循环执行epoll_wait，然后根据Channel发生的事情让Channel调用相应的回调操作
    void loop();
    // 退出事件循环
    void quit();

    Timestamp pollReturnTime() const {
        return pollReturnTime_;
    }

    // 在当前loop中执行回调函数cb
    void runInLoop(Functor cb);
    // 把cb放入队列中，唤醒loop所在的线程，执行回调函数cb
    void queueInLoop(Functor cb);

    // mainReactor用来唤醒subReactor的，也就是loop所在的线程，具体请看课件上的Muduo整体模型
    void wakeup();

    // 调用的是poller中的相应方法，相当于替Channel传话
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* Channel);

    // 判断该EventLoop对象是否在创建出该对象的线程中
    bool isInLoopThread() const {
        return threadId_ == CurrentThread::tid(); 
    }

private:
    // wake up
    void handleRead();
    // 执行回调
    void doPendingFunctors();

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;      // 原子操作，通过CAS实现，标识开启loop循环，由其他线程调用相关函数控制事件循环的开启、关闭
    std::atomic_bool quit_;         // 标识退出loop循环，由其他线程调用相关函数控制事件循环的开启、关闭
    const pid_t threadId_;          // 记录当前loop所在的线程的id 
    Timestamp pollReturnTime_;      // poller返回发生事件的channels的时间点，其实就是EPollPoller中的poll方法返回的结果
    std::unique_ptr<Poller> poller_;    // EventLoop所管理的Poller对象

    int wakeupFd_;      // 主要作用，当mainLoop获取第一个新用户的channel，通过轮询算法选择一个subReactor，通过该成员唤醒subloop处理channel
                        // 通过Linux的系统调用eventfd创建，可以关注一下怎么用的以及创建过程
    std::unique_ptr<Channel> wakeupChannel_;    // wakeupFd_也需要用Channel包装一下，该变量就是存储包装后的Channel的

    ChannelList activeChannels_;    // 存储所有发生事件的channel
    
    std::atomic_bool callingPendingFunctors_;   // 标识当前loop是否有需要执行的回调操作，干嘛用的看queueInLoop函数
    std::vector<Functor> pendingFunctors_;      // 存储loop需要执行的所有的回调函数，这些回调函数都是上级交给他的，比如TcpServer，比如EventLoopThread
    std::mutex mutex_;                          // 互斥锁，用来保护上面vector容器的线程安全操作
};