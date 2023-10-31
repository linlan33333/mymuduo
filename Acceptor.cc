#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

// 创建一个不阻塞的sockfd，用来监听新用户连接，也就是listenfd，交给acceptSocket_包装起来
static int createNonBlockingSocket()
{
    int sockfd = ::socket(AF_INET, SOCK_CLOEXEC | SOCK_NONBLOCK | SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_FATAL("%s:%s:%d listen socketfd create failed. errno:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }

    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonBlockingSocket())
    , acceptChannel_(loop_, acceptSocket_.fd())
    , listening_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);      // bind，前面已经创建了acceptSocket_，这里就得紧接着绑定了
    // 当TcpServer启动之后，有新用户连接时，listenfd就会接收到数据，那么就需要执行回调操作进行建立连接
    // 这里先给包装成Channel的listenfd注册回调函数
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listening_ = true;
    acceptSocket_.listen();     // listen
    acceptChannel_.enableReading();     // acceptChannel_ => Poller
}

// 该回调函数专门用于listenfd，因此只给包装了listenfd的channel对象注册
// listenfd有事件发生了，就说明有新用户连接，那么该干啥干啥
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    // 建立连接，拿到与客户端通讯的fd
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0) {
        if (newConnectionCallback_) {
            // 轮询找到subLoop，唤醒，分发当前的新客户端的Channel
            // 本质上调用的是TcpServer的newConnection函数
            newConnectionCallback_(connfd, peerAddr);
        }
        else {
            ::close(connfd);
        }
    }
    else {
        LOG_FATAL("%s:%s:%d listen socketfd create failed. errno:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        // EMFILE表示当前进程打开的文件描述符已达上限，此时服务器不能再接受客户端连接
        if (errno == EMFILE) {
            LOG_ERROR("%s:%s:%d sockfd opened too much. errno:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        }
    }
}


