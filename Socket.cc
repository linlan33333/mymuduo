#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>

Socket::~Socket()
{
    ::close(sockfd_);
}

// 就是bind函数干的事
void Socket::bindAddress(const InetAddress &localaddr)
{
    if (::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in))) {
        LOG_FATAL("bind sockfd:%d failed", sockfd_);
    }
}

void Socket::listen()
{
    if (::listen(sockfd_, 1024)) {
        LOG_FATAL("listen sockfd:%d fail", sockfd_);
    }
}

// peeraddr是个输出参数，把客户端的信息绑到这上面，然后调用者就拿到了客户端的信息
// 返回值是建立连接的fd
int Socket::accept(InetAddress *peeraddr)
{
    sockaddr_in addr;   // 记录客户端的端口，ip等信息，待会注入到peeraddr中
    socklen_t len = sizeof(sockaddr_in);
    bzero(&addr, sizeof addr);
    /**
     * 注意connfd需要设置为非阻塞
     * 可以用accept4函数，相比于accept函数多一个参数，用来设置socketfd的标志
    */
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);

    if (connfd >= 0) {
        peeraddr->setSockAddr(addr);    // 把信息注入到peeraddr中
    }
    return connfd;
}

// 单纯关闭写端，需要使用shutdown函数进行精确操作
void Socket::shutdownWrite()
{
    if (::shutdown(sockfd_, SHUT_WR) < 0) {
        LOG_ERROR("Socket::shutdownWrite error");
    }
}

// 即可用于listenfd，也可用于connfd，用于listenfd标识以后所有建立连接的connfd都被设置这个选项
void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}

// 监听的listenfd专用
void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

// 监听的listenfd专用
void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}

// 连接的connfd专用
void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}
