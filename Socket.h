#pragma once

#include "noncopyable.h"

class InetAddress;

/**
 * 该类专门用于包装监听新用户连接的listenfd，把listenfd该有的功能包装起来
 * 用于打包新用户连接的fd，将其打包成Channel，然后分发给那些subLoop对象
*/
class Socket : private noncopyable
{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    {}

    ~Socket();

    int fd() const { return sockfd_; }

    void bindAddress(const InetAddress& localaddr);
    void listen();
    int accept(InetAddress* peeraddr);

    void shutdownWrite();

    // 对数据不进行TCP缓冲，直接发送
    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);

private:
    const int sockfd_;      // 这个socketfd_既可以是listenfd也可以是connfd，得看它的上级（Acceptor或TcpConnection类）怎么创建的
};