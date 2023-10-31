#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer下属的mainLoop当获取到新用户连接时，需要
 * 1、把connfd封装成Channel
 * 2、给Channel注册回调函数等操作
 * 3、然后放到subLoop中
 * 这些事情都外包给TcpConnection来做，这就是这个类的作用
 * 此外这个类还要管理连接的状态
 * 就好比Acceptor专门处理mainLoop中的listenfd一样
 * 至于这个TcpConnection对象背后的Channel对象被注册到哪个subLoop上由TcpServer来选，见TcpServer::newConnection
*/
class TcpConnection : private noncopyable,
                      public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop* loop, const std::string nameArg,
                    int sockfd, const InetAddress& localAddr,
                    const InetAddress& peerAddr);

    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }

    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == StateE::kConnected; }

    // 关闭连接
    void shutdown();

    // 发送数据给客户端，这里只考虑发送Json字符串格式的数据，其他格式暂不考虑
    // 底层调用sendInLoop
    void send(const std::string& buf);

    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }
    void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) { 
        highWaterMarkCallback_ = cb; 
        highWaterMark_ = highWaterMark;
    }

    // 创建连接时调用的函数，由TcpServer调用
    void connectEstablished();
    // 连接销毁时调用的函数，由TcpServer调用
    void connectDestroyed();
private:

    // 这个枚举表示连接的状态
    // 用来控制成员函数的业务逻辑，比如都断开连接了肯定不能发送数据
    enum StateE {
        kDisconnected, 
        kConnecting, 
        kConnected, 
        kDisconnecting
    };
    void setState(StateE state) { state_ = state; }

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    // 其实它才是真正发送数据的函数
    void sendInLoop(const void* data, size_t len);
    void shutdownInLoop();

    EventLoop* loop_;   // 一定是某个subLoop对象，不可能是mainLoop，TcpServer选好的subLoop传过来的
    const std::string name_;    // 名字
    std::atomic_int state_;     // 连接的状态，取值范围就是枚举StateE
    bool reading_;      // 本项目核心代码中没有用上，可以忽略

    // 这里和Acceptor类似，Acceptor专门处理mainLoop的listenfd
    // TcpConnection就专门处理subLoop的connfd
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;  // 强智能指针指向channel对象，channel对象那头也会用弱智能指针指向该对象
                                        // 从而观察该对象还在不在，防止channel调用回调函数时该对象没了调用失败
                                        // 比如epoll返回了发生事件的epoll_event们，此时还没有放到EventLoop的activeChannels_中
                                        // 就先调用上级回调把里面的某个connfd（也就是包装后的TcpConnection）给析构了
                                        // 结果从activeChannels_中取Channel调用相应的回调操作时发现TcpConnection没了，回调函数调用失败

    const InetAddress localAddr_;   // 记录当前主机的IP地址端口号
    const InetAddress peerAddr_;    // 记录对端的IP地址款口号

    ConnectionCallback connectionCallback_;     // TcpServer传过来的，用于处理新用户连接的回调函数
    MessageCallback messageCallback_;           // TcpServer传过来的，用于处理有读写消息的回调函数
    WriteCompleteCallback writeCompleteCallback_;       // TcpServer传过来的，用于处理发送消息完成以后的回调
    CloseCallback closeCallback_;               // TcpServer传过来的，用于处理连接关闭的回调函数，本质上是TcpServer的removeConnection方法
    HighWaterMarkCallback highWaterMarkCallback_;   // 高水位线回调函数，怀疑用于TCP拥塞控制的，该项目没貌似用上

    size_t highWaterMark_;      // 值会设为64M，超过64M就到水位线了，防止我们发送数据太快而对方接收太慢

    Buffer inputBuffer_;        // Channel的接收数据缓冲区
    Buffer outputBuffer_;       // Channel的发送数据缓冲区
};