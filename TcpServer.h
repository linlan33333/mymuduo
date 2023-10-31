#pragma once

/**
 * 这里不做前置声明，因为如果做前置申明，那么需要在使用muduo库时，包含这些头文件，给用户造成非常大的不方便
 * 因此这里就需要把要用到的头文件全部包含在这里，而不是在.cc文件中
*/
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

// 对外的服务器编程使用的类
class TcpServer : private noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    // 预置两个选项标记是否设置了端口复用
    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg, Option option = kNoReusePort);

    ~TcpServer();

    /**
     * 以下四个函数均是TcpServer提供给用户的接口，用户写好相关函数然后传过来
    */
    void setThreadInitCallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; }
    // 这个回调函数最终会交给Acceptor对象中的newConnectionCallback_成员变量，执行新用户的连接和断开事件的回调操作
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }


    // 设置底层subLoop的个数
    void setThreadNum(int numThreads);

    // 开启服务器监听，本质就是开启mainLoop的listen方法
    void start();
    
private:
    void newConnection(int sockfd, const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop* loop_;       // mainLoop 用户定义的EventLoop对象

    const std::string ipPort_;      // 保存服务器相关的IP地址和端口号
    const std::string name_;        // 保存服务器的名称

    std::unique_ptr<Acceptor> acceptor_;        // 运行在mainLoop， 任务就是监听新连接事件
    std::unique_ptr<EventLoopThreadPool> threadPool_;       // one loop per thread

    ConnectionCallback connectionCallback_;     // 用户自己定义的，用于处理新用户连接的回调函数，连接成功、连接建立、连接断开都会调用
    MessageCallback messageCallback_;           // 用户自己定义的，用于处理有读写消息的回调函数
    WriteCompleteCallback writeCompleteCallback_;       // 用户自己定义的，用于处理发送消息完成以后的回调

    ThreadInitCallback threadInitCallback_;     // 用于处理新线程初始化的回调函数
    std::atomic_int started_;           // 标记TcpServer是不是已经启动过了 

    int nextConnId_;                    // 因为mainLoop只在主线程中运行，不涉及其他线程操作这个nextConnId_，所以没必要用atomic类型
    ConnectionMap connections_;         // 保存所有的连接
};