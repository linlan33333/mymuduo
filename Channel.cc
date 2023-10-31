#include "Channel.h"
#include "EventLoop.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;     // EPOLLPRI触发则表示有紧急数据到来，比如TCP套接字的带外数据或套接字错误
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)        // 这里初始化为-1与EPollPoller.cc中的kNew相对应，表示该channel对象还没有添加到epoll实例中
    , tied_(false)
{
}

Channel::~Channel()
{
    std::cout << "该Channel对象被析构了, fd为" << fd() << std::endl;
}

// fd得到poller通知后，处理相应的事件的
void Channel::handleEvent(Timestamp recieveTime)
{
    // 如果是subLoop要求执行相应的回调，那么回调函数本质是TcpConnection中的方法，
    // 就需要检测TcpConnection对象是否还存在
    if (tied_) {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard != nullptr) {
            handleEventWithGuard(recieveTime);
        }
    }
    // 如果是mainLoop要求执行相应的回调，那么回调函数实际上是用户创建mainLoop时给的，
    // 那直接调用就行，具体看UseMuduo中的mymuduo_server.cpp文件
    else {
        handleEventWithGuard(recieveTime);
    }
}

// channel的tie方法在什么时候调用过？TcpConnection绑定channel的时候会调用
// 也让channel绑定TcpConnection对象
void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_ = obj;     // weak_ptr得用shared_ptr初始化
    tied_ = true;
}

// 在channel所属的EventLoop中，把当前的channel删除掉
void Channel::remove()
{
    // 通过channel所属的EventLoop，调用poller的相应方法，最终调用的是EPollPoller中的removeChannel方法，来摘掉该channel对象
    loop_->removeChannel(this);
}

// 当改变channel感兴趣的事件后，需要通过该方法在polleri里面更改fd相应的事件epoll_ctl
void Channel::update()
{
    // 通过channel所属的EventLoop，调用poller的相应方法，最终调用的是EPollPoller中的updateChannel方法，来注册fd的events_事件
    loop_->updateChannel(this);
}

// 根据poller通知的channel发生的具体事件，有channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents: %d\n", revents_);
    // 这是关闭了写端，是我们服务器主动断开了连接之后，
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {

        if (closeCallback_) {
            closeCallback_();   // 本质上调用的是TcpConnection的handleClose方法
                                // 而TcpConnection的handleClose方法本质上调用的是TcpServer的removeConnection方法
        }
    }

    // 这是出错了
    if (revents_ & EPOLLERR) {
        if (errorCallback_) {
            errorCallback_();
        }
    }

    // 发生可读事件
    if (revents_ & (EPOLLIN | EPOLLPRI)) {
        if (readCallback_) {
            readCallback_(receiveTime);
        }
    }

    // 发生可写事件
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) {
            writeCallback_(); 
        }
    }
}

const void Channel::testHaveChannel()
{
    std::cout << "可以调用Channel对象的testHaveChannel函数" << std::endl;
}
