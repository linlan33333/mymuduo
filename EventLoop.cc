#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

// 防止一个线程创建多个EventLoop，因为这是全局变量，当某个线程第一次创建EventLoop对象之后，该指针就
// 不为空。只要检查这个全局变量是否为空就知道该线程有没有创建过EventLoop对象
/**
 * 为什么要加上__thread修饰，因为这是一种线程局部存储机制。加上__thread修饰的变量在每个线程内只有一份实例，各线程之间
 * 的值互不干扰，但如果采用static全局变量，值的变化所有线程都能修改都能看到，就起不到防止一个线程创建多个EventLoop的效果
*/
__thread EventLoop* t_loopInThisThread = nullptr;

// 定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000;

// 创建wakeupfd，用来notify唤醒subReactor处理新来的channel，注意这个wakeupfd也需要用channel包装一下
// 因此成员变量中的wakeupChannel_就是这么来的
int createEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        LOG_FATAL("eventfd error: %d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread != nullptr) {
        LOG_FATAL("Another EventLoop %p exists in this thread%d 该线程已经创建其他的EventLoop对象了 \n", 
                t_loopInThisThread, threadId_);
    }
    else {
        t_loopInThisThread = this;
    }

    // 设置wakeupChannel感兴趣的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个eventloop都将监听wakeupchannel的EPOLLIN读事件了
    wakeupChannel_->enableReading();
}

// 对于subLoop来说，当主线程退出，会让子线程都退出，由于subLoop是创建在子线程中的，子线程退出时会
// 调用这里的析构函数让EventLoop消失
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping\n", this);

    // 循环不断地进行epoll_wait
    while (!quit_) {
        activeChannels_.clear();
        // poll方法监听两类fd，一种是客户端的fd，另一种是wakeupFd
        // 一开始创建subReactor时，里面还没有要监听的客户端fd，因此poll函数必然阻塞，得让mainReactor往wakeupFd_中写数据
        // 才能让poll函数解除阻塞，才能有机会调用doPendingFunctors函数
        // 而mainLoop就用不到wakeupFd，因为没有新用户连接的话阻塞在那就行了
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel* channel : activeChannels_) {
            // Poller监听哪些channel发生事件了，然后上报给EventLoop，通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作，这些回调操作都是它的上级交给他完成的
        // 这些回调操作要求随时、直接、立即执行，因此为了防止线程阻塞在上面的poll函数那一步，需要通过wakeupfd_来解除阻塞，从而能立即执行上级回调操作
        /**
         * mainReactor会事先往subReactor中的EventLoop对象中注册一个回调函数，该EventLoop被唤醒之后才会执行这里的回调函数
         * 比如mainReactor建立了新的客户端连接，需要把新连接的channel注册到subReactor中，
         * 这里的doPendingFunctors就是负责把新连接的channel注册到epoll中去的
        */
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop loping. \n", this);
    looping_ = false;
}

// 退出事件循环
// 在subLoop中，该函数一定是在doPendingFunctors()中调用的，在mainLoop中该函数由EventLoopThread的析构函数调用
//                                  mainloop
//          subloop 1                subloop 2                subloop 3
// 注意这个EventLoop函数是各种loop共用同一套代码，为什么有可能subloop1发现应该在subloop2运行quit函数？
void EventLoop::quit()
{
    quit_ = true;

    /**
     * 如果是自己的线程EventLoop退出，那么该函数能被调用说明已经执行到doPendingFunctors()了
     * 此时下一次while循环就会不满足条件而结束
     * 但是如果是其他的线程中的EventLoop要退出，那么其他线程有可能卡在poll函数中（卡在epoll_wait），
     * 此时就需要唤醒它继续往下走，从而在下一次while循环时因不满足条件而结束
    */
    if (!isInLoopThread()) {
        wakeup();
    }
}

// 在当前loop中执行回调函数cb，谁会调用该函数呢？
// 是它的上级，比如TcpServer要求它调用Acceptor::listen函数
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) {       // 在当前的loop线程中执行回调函数cb
                                  // 待会研究一下什么时候会直接在当前线程中执行回调函数
        cb();
    }
    else {          // 在非当前的loop线程中执行回调函数cb，就需要唤醒loop所在的线程，执行cb
        queueInLoop(cb);
    }
}

// 把回调函数cb放入队列中，唤醒loop所在的线程，执行回调函数cb
void EventLoop::queueInLoop(Functor cb)
{
    // 临界区，出了临界区自动释放锁
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的需要执行上面回调操作的loop的线程
    // 为什么要|| callingPendingFunctors_：
    // 因为如果当前线程依次执行回调函数列表中的函数，由于执行时swap了一下（看doPendingFunctors函数）
    // 导致该eventLoop对象并不知道有新的回调函数添加到pendingFunctors_中了，因此执行完doPendingFunctors函数后回到
    // 下一次while循环（看loop函数）就有可能阻塞住，因此需要再唤醒一下让它执行新来的回调函数
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();       // 唤醒loop所在线程
    }
}

// 用来唤醒loop所在的线程的     
// 向wakeupfd_写一个数据，wakeupChannel就会发生读事件，就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}

// 代Channel向Poller询问
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

// 代Channel向Poller询问
void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

// 代Channel向Poller询问
bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        LOG_ERROR("EventLoop::handleRead() reads %ld bytes instead of 8", n);
    }
}

// 执行回调
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    // 临界区
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 把pendingFunctors_和funtors交换，从而避免pendingFunctors_被上锁导致mainloop无法向pendingFunctors_里面添加回调函数
        functors.swap(pendingFunctors_);
    }

    for (const Functor& functor : functors) {
        functor();      // 执行当前loop需要执行的回调操作
    }

    callingPendingFunctors_ = false;
}
