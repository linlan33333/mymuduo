#pragma once

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

#include <functional>

class EventLoop;
class InetAddress;

// 这个类的作用就是将监听新用户连接的listenfd打包成channel对象然后传给mainLoop
// 它是专门为listenfd服务的
// 就好比TcpConnction专门处理subLoopLoop中的connfd一样
class Acceptor : private noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    // 把新连接的回调函数存进来
    void setNewConnectionCallback(const NewConnectionCallback& cb) {
        newConnectionCallback_ = cb;
    }

    bool listening() const { return listening_; }

    void listen();
private:
    // 这个函数专门处理新用户连接时的业务操作
    // handle的是listenfd的read事件
    void handleRead();

    EventLoop* loop_;   // 这个loop_是mainLoop，但是没有什么用，这个类里没用上
    Socket acceptSocket_;       // 包装的就是listenfd
    Channel acceptChannel_;

    NewConnectionCallback newConnectionCallback_;
    bool listening_;
};
