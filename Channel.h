#pragma once

#include <functional>
#include <memory>
#include <iostream>

#include "noncopyable.h"
#include "Timestamp.h"
#include "Logger.h"

// 前向声明，不需要在这里包含头文件，以减少编译器依赖，见《Linux多线程服务器编程》P402 前向声明
class EventLoop;
/**
 * 理清楚EventLoop，Channel和Poller之间的关系 《= Reactor模型上对饮Demultiplex事件分发器
 * Channel理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN、EPOLLOUT事件
 * 还绑定了poller返回的具体事件，也就是epoll检测到到底在这个socketfd上发生了什么事件
*/
class Channel : private noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    // 处理事件函数，fd得到poller通知以后，调用相应的回调方法
    void handleEvent(Timestamp recieveTime);

    // 设置回调函数对象，因为回调函数对象都是私有变量，需要set方法
    // 本质上是TcpConnection调用并赋值的，这里的cb其实就是TcpConnection中的handleWrite、handleRead等方法
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteEventCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseEventCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorEventCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 防止当channel对象被手动释放掉之后，channel还在执行回调操作
    void tie(const std::shared_ptr<void>& obj);

    // one loop per thread
    EventLoop* ownerLoop() { return loop_; }
    int fd() const { return fd_; }
    int events() const { return events_; }

    // 设置fd实际上发生的事情
    void set_revents(int revts) { 
        revents_ = revts; 
    }

    // 检测当前channel中的fd有没有注册感兴趣的事件，用来查看Channel注册了哪些事件
    bool isNoneEvent() const { return events_ == kNoneEvent; }      // 对任何事件都不感兴趣，表示可以从epoll实例中摘掉该channel了
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReadingg() const { return events_ & kReadEvent; }

    // 可以理解为修改epoll_event，设置fd相应的事件状态，然后让epoll_ctl更新一下（没放到epoll实例的顺便放进去）
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }     // 学学如何操作位图去掉某一位
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    int index() { return index_; }
    void set_index(int idx) {index_ = idx;}

    // 将自己从EPollPoller中的ChannelMap中删除
    void remove();

    // 测试一下该Channel对象还活没活着，这个是自己用来debug用的，不是项目里有的
    static const void testHaveChannel();

private:
    void update();

    // handleEvent()方法调用的就是这个函数
    void handleEventWithGuard(Timestamp receiveTime);

    // 表明当前的fd有没有对某个事件感兴趣
    // 为什么不直接用EPOLLIN、EPOLLOUT，因为这个网络库支持poll和epoll，所以它们把poll和epoll的事件统一起来
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_;   // 事件循环，注意委托的关系
    const int fd_;      // 要记录的socketfd，poller监听的对象
    int events_;        // 注册fd感兴趣的事件
    int revents_;       // 在fd身上实际发生的事件
    int index_;         // 表示该channel是否添加到epoll实例中，配合EPollPoller.cc中的kNew、kAdded等使用

    std::weak_ptr<void> tie_;   // 系着TcpConnection对象，由于channel对象调用的回调操作都是TcpConnection中的成员函数
                                // 注意，TcpConnection对象是给到用户手里的，因此用户可能会把该对象析构掉
                                // 为了确保调用那些成员函数时，绑定的TcpConnection对象还存在，就需要用一根弱智能指针指向TCPConnection对象
                                // 从而观察对应的TcpConnection还活着不 
    bool tied_;         // 表示是否还系着TcpConnection对象

    // 因为channel通道里面可以获知fd_最终发生的具体的事件revents_，因此它要根据revents_调用相应事件的回调
    ReadEventCallback readCallback_;    // 本质上是TcpConnection::handleRead方法
    EventCallback writeCallback_;   // 本质上是TcpConnection::handleWrite方法
    EventCallback closeCallback_;   // 本质上是TcpConnection::handleClose方法
    EventCallback errorCallback_;   // 本质上是TcpConnection::handleError方法
};
