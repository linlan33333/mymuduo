#include "TcpConnection.h"
#include "Logger.h"
#include "EventLoop.h"
#include "Channel.h"
#include "Socket.h"

#include <functional>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>

// 服务于TcpConnection构造函数的
// 为什么要设置成静态的？因为TcpServer.cc中也定义了一次，不设为静态函数的话名称会起冲突
static EventLoop* CheckLoopNotNull(EventLoop* loop) {
    if (loop == nullptr) {
        LOG_FATAL("%s:%s:%d TcpConnection is null \n", __FILE__, __FUNCTION__, __LINE__);
    }

    return loop;
}

// 该对象在TcpServer::newConnection中被创建
TcpConnection::TcpConnection(EventLoop *loop, const std::string nameArg, int sockfd, const InetAddress &localAddr, const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)     // 本机的IP地址端口号 
    , peerAddr_(peerAddr)       // 对端的IP地址端口号
    , highWaterMark_(1024 * 1024 * 64)  // 大小为64M
{
    // 下面给channel设置相应的回调函数，poller给channel通知感兴趣的事件发生了，channel会回调相应的操作函数
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1)
    );

    channel_->setWriteEventCallback(
        std::bind(&TcpConnection::handleWrite, this)
    );

    channel_->setCloseEventCallback(
        std::bind(&TcpConnection::handleClose, this)
    );

    channel_->setErrorEventCallback(
        std::bind(&TcpConnection::handleError, this)
    );

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);    // 启动tcp的socket的保活机制
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", name_.c_str(), channel_->fd(), (int)state_);
}

void TcpConnection::shutdown()
{
    if (state_ == StateE::kConnected) {
        setState(StateE::kDisconnecting);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, shared_from_this())
        );
    }
}

// 该函数被TcpServer中的newConnection调用
void TcpConnection::connectEstablished()
{
    setState(StateE::kConnected);
    channel_->tie(shared_from_this());  // 让channel对象绑定自己
    channel_->enableReading();      // 向poller注册channel的读事件

    // 新连接建立，得调用这个回调函数，这是用户写好传过来的
    connectionCallback_(shared_from_this());
}

// 断开连接，最终调用close来关闭连接，而不是shutdown
// 该函数什么时候调用？
// （1）先看TcpConnection::handleClose什么时候调用
// TcpConnection::handleClose调用后本质上调用了TcpServer的removeConnection方法
// 而removeConnection方法调用了本函数
// （2）当TcpServer对象要析构，析构时就会主动把所有连接都断开，
// 也就是把每个TcpConnection对象都调用一遍本函数
void TcpConnection::connectDestroyed()
{
    if (state_ == StateE::kConnected) {     // 对于调用时机2来说此时连接状态肯定是kConnected，因此会走这个if判断
                                            // 但对于调用时机1来说，此时连接状态已经设为kDisconnected了，不信看看handleClose函数
                                            // 是怎么写的，因此不会走这个if判断，而是直接remove掉Channel
        setState(kDisconnected);
        channel_->disableAll();     // 把channel从epoll实例中摘掉
        connectionCallback_(shared_from_this());
    }

    channel_->remove();  // 把channel从poller中的ChannelMap中删除掉
}

// Channel上有数据可读时调用的回调函数
void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) {
        // 已建立连接的用户，由可读事件发生了，调用用户传入的回调操作onMessage
        // shared_from_this()用于获取当前对象的智能指针，相当于shared_ptr<TcpConnection> ptr(this)，但不能这么写，因为会重复创建多个控制块
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0) {  // 客户端断开连接
        handleClose();
    }
    else {  // 出错了
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

/**
 * 该函数的作用是将send函数没发完的数据发送出去，为什么这么说请看笔记
*/
void TcpConnection::handleWrite()
{
    if (channel_->isWriting()) {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if (n > 0) {
            // 更新一下发送数据的起始位置
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0) {
                /**
                 * 当数据全部发送完成时就设置不关注写事件，避免不必要的写事件触发
                 * 那什么时候会设置回关注写事件呢？当需要发送数据，也就是调用sendInLoop函数
                 * 一次性没能把数据发送完，那么
                */
                channel_->disableWriting(); 
                if (writeCompleteCallback_) {
                    // 让subLoop赶紧去执行用户定义好的writeCompleteCallback_
                    // 其实这个writeCompleteCalllback_干的事就是调用send函数发送TCP报文（请看使用示例）
                    // 因此发送数据之前需要
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this())
                    );
                }
                if (state_ == StateE::kDisconnecting) {  // 如果该连接正在关闭
                    // 说明此时在发送数据的时候有人调用了shutdown函数，但shutdown函数
                    // 发现还在发送数据，于是赶紧停掉，这里就需要在发送完数据后继续调用shutdown函数关闭连接
                    shutdownInLoop();
                }
            }
        }
        else {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else {
        // 这个fd已经关闭写端了
        LOG_ERROR("TcpConnection::handleWrite TcpConnection fd=%d is down, no more writing\n", channel_->fd());
    }
}

// poller检测到客户端断开连接时调用channel的closeCallback回调函数
// 而closeCallback调用的其实是该函数
void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(StateE::kDisconnected);
    channel_->disableAll();     // 让channel从epoll实例上摘掉
                                // 让channel从EPollPoller中删除是closeCallback_干的事

    TcpConnectionPtr connPtr(shared_from_this());   // 让该TcpConnection对象的引用计数+1，使得原本在TcpServer::connections_
                                                    // 容器中的TcpConnection指针不再唯一，这样哪怕connections_容器中删掉了该
                                                    // TcpConnection对象指针也不会导致引用计数为0从而析构掉该对象，导致出问题
    if (connectionCallback_) {
        connectionCallback_(connPtr);   // 执行回调,该回调在连接关闭时也要调用
    }
    if (closeCallback_) {
        closeCallback_(connPtr);    // 这是关闭连接的回调，本质调用的是上面的connectionDestroyed函数
    }
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        err = errno;
    }
    else {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d", name_.c_str(), err);
}

void TcpConnection::send(const std::string& buf)
{
    if (state_ == kConnected) {
        // 一般都是在当前loop所在的线程中执行，这里的判断其实没多大必要
        if (loop_ -> isInLoopThread()) {
            // 为了能在两种情况下都能发送数据，才多设计了个sendInLoop，不然直接send里面就把业务逻辑做完了
            sendInLoop(buf.c_str(), sizeof buf);
        }
        else {
            loop_->runInLoop(
                std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size())
            );
        }
    }
}

/**
 * 要考虑写的过快而内核发送数据较慢（就是对端接收太慢）的情况
*/
void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0;  // 已发送的数据
    ssize_t remaining = len;    // 没发送完的数据
    bool faultError = false;    // 记录是否产生错误

    // 都断开连接了肯定不能发送数据了
    if (state_ == StateE::kDisconnected) {
        LOG_ERROR("TcpConnection::sendInLoop disconnected, give up writing");
        return;
    }

    // 表示Channel_之前需要发送的数据已经发送完了，而且缓冲区没有待发送的数据
    // 因为在handleWrite中如果把要发的数据全部发送出去了的话是正好符合
    // !channel_->isWriting() && outputBuffer_.readableBytes() == 0
    // 这个条件的
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0) {  // 发送成功
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_) {     // 数据一次性发送完了
            // 既然在这里数据一次性发送完了，就不用再给channel注册epollout事件了
            // 与下面的数据没发送完（比如对端接收速度慢导致）的情况对应
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this())
                );
            }
        }
        else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                LOG_ERROR("TcpConnection::sendInLoop send error");

                // 对端socket连接重置了
                if (errno == EPIPE || errno == ECONNRESET) {
                    faultError = true;
                }
            }

        }
    }

    // 说明当前这一次write，并没有把数据全部发送出去，剩余的数据需要保存到缓冲区中，然后给channel
    // 注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的socket（也就是channel）调用writeCallback_回调方法
    // 也就是调用TcpConnection::handleWrite方法，把发送缓冲区中的数据继续发送出去，如果还没发送完，再注册epollout事件。。。不断循环调用这个回调函数
    // 直至把缓冲区中的全部数据都发完
    if (!faultError && remaining > 0) {  
        // 目前发送缓冲区的剩余的待发送数据的长度   
        size_t oldLen = outputBuffer_.readableBytes();
        /**
         * 由于这个回调函数highWaterMarkCallback_并没有定义
         * 所以这个if分支不会触发
        */
        if (oldLen + remaining >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMarkCallback_) {
                loop_->queueInLoop(
                    std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining)
                );
        }
        // 因为有部分没发送完，需要保存回缓冲区中
        outputBuffer_.append((char*)data + nwrote, remaining);
        if (!channel_->isWriting()) {
            channel_->enableWriting();  // 这里一定要注册channel的写事件，否则poller不会给channel通知epollout事件，
                                        // 因为数据全部发送完时，会设置Channel不关注epollout事件（看handleRead），避免不必要的写事件反复触发
                                        // 也就不会再来调用这个回调函数，剩下的数据就没机会发出去了
        }
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting()) {   // 说明此时outputBuffer中的数据已经全部发送完成了
        socket_->shutdownWrite();   // 关闭写端，触发EPOLLHUP情况，回看Channel的handleEventWithGuard方法
    }
}
