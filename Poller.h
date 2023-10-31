#pragma once

#include <vector>
#include <unordered_map>

#include "noncopyable.h"
#include "Timestamp.h"
#include "Channel.h"

// 同样这两个类在代码中只需要用作指针类型，因此只需要前置声明即可不需要包含相应的头文件
class Channel;
class EventLoop;

// muduo库中多路事件分发器的核心IO复用模块，担任epoll的职责
// 注意这是纯虚类，不能实例化对象
class Poller : private noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    // 纯虚函数，给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;     // 相当于启动了epoll_wait()
    virtual void updateChannel(Channel* channel) = 0;       // 相当于自动epoll_ctl执行EPOLL_CTL_MOD操作
    virtual void removeChannel(Channel* channel) = 0;       // 相当于自动epoll_ctl执行EPOLL_CTL_DEL操作

    // 判断某个Channel对象是否在当前Poller当中
    bool hasChannel(Channel* channel) const;

    // EventLoop可以通过该接口获取默认的IO复用具体实现，类似单例模式中的getInstance
    /**
     * 注意：该方法并不在Poller.cc中实现，因为有哪些子类会继承Poller不知道，所以不能在Poller.cc中包含子类的头文件
     * 所以可以单独创建一个叫DefaultPoller的文件，在该文件中实现该方法，这是引入子类的头文件就合情合理
     * 为什么不使用抽象工厂模式或者原型模式？  后续看它怎么用，看看能不能用设计模式替换掉
    */
    static Poller* newDefaultPoller(EventLoop* loop);
protected:
    // 它的键是channel管理的文件描述符，值是那个channel的指针，也就是提供了fd到Channel*的映射关系
    using ChannelMap = std::unordered_map<int, Channel*>;
    // 这里面装的是在epoll中注册过的channel，那个EventLoop里面的ChannelList装的是ChannelMap中发生事件的Channel，是包含关系
    ChannelMap channels_;
private:
    EventLoop* loop_;       // 定义Poller所著的事件循环EventLoop，和channel一样都是EventLoop的子组件
};