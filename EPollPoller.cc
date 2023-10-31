#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <strings.h>
#include <iostream>

const int kNew = -1;    // 表示一个Channel还没有添加到Epoll里面
const int kAdded = 1;    // 表示一个Channel已经添加到Epoll里面
const int kDeleted = 2;    // 表示一个Channel已经从Epoll中删除掉了

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))        // epoll_create1干什么的看笔记
    , events_(kInitEventListSize)
{
    // epoll实例创建失败，致命错误，打印一下
    if (epollfd_ < 0) {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

// 作用相当于epoll_wait，由EventLoop调用
// 需要通过epoll_wait把发生事件的channel填到activeChannels中去，交给EventLoop，并返回发生epoll_wait的时刻
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 实际上应该用LOG_DEBUG输出日志更为合理，因为LOG_DEBUG默认不开启，避免了大量IO操作拖慢epoll性能
    LOG_DEBUG("func=%s => fd total count:%lu\n", __FUNCTION__, channels_.size());

    // 解释一下第二个参数是什么意思：epoll_wait中第二个参数要求传入一个普通数组的首地址，但是events_是一个vector，
    // vector底层是一个数组，只要把该数组的第一个元素的地址传给epoll_wait即可。
    // *events_.begin()表示取第一个地址，再加上取地址符即可，这是一个重要技巧，C++调用Linux系统调用时经常这么做，也可以用C++11的events_.data()
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    // 由于errno是全局的变量，不同线程跑poll函数时都会改这个errno，所以先用一个局部变量保存一下当前值
    int saveErr = errno;

    Timestamp now(Timestamp::now());

    if (numEvents > 0) {
        LOG_DEBUG("%d events happened\n", numEvents);
        // 把发生事件的channel都填到EventLoop给的activeChannels中
        fillActiveChannels(numEvents, activeChannels);
        // 如果events_的空间不够了，也就是epoll_wait填到events_中填不下，需要扩容操作
        // 因为Muduo采用LT模式，events_空间不够了epoll_wait会把填不下的事件留到下一次调用epoll_wait再填
        if (numEvents == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    }
    // 如果epoll_wait等都等超时了，那说明肯定numEvents为0
    else if (numEvents == 0) {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else {
        // saveErr如果是EINTR，说明被中断了再回来，那么应该继续运行
        // 如果是其他错误，那说明真出问题了，应该打印一下
        if (saveErr != EINTR) {
            errno = saveErr;
            LOG_ERROR("EPollerPoller::poll() error!");
        }
    }

    return now;
}

// 该函数负责把发生事件的channel填到EventLoop给的容器中，上交给EventLoop处理
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; i++) {
        // 顺藤摸瓜找到发生事件的fd归属的channel
        // Channel对象指针转化成功了
        // 目前events_[i]没问题，有内容
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        // channel->testHaveChannel();
        // 个人倾向于events_数组中的data.ptr指向的Channel对象早就被销毁或者释放了，然后这里调用fd函数访问了
        // 已经被释放的对象，因此报错信息中重复出现Channel的指针地址是0x555500000005，很可能是一个无效地址
        // 这个bug需要理解一遍整个业务流程才能知道错在哪
        // 2023/10/8记录：查看日志信息可以发现，总共创建了5个socket，其中有三个是属于三个subLoop线程的，那么为什么会有两个socket在mainLoop中？
        // 也就是说为什么主线程会创建两个listenfd？
        // 2023/10/9记录：查找到错误原因了：在update函数中给event_data是union类型，赋值只能赋一种，如果既给ptr字段赋值为Channel对象指针，又给fd字段设置为
        // Channel的fd的话，会覆盖掉ptr字段的值，导致后面怎么找也找不到Channel对象
        std::cout << channel->fd() << std::endl;
        channel->set_revents(events_[i].events);

        // EventLoop就拿到了它的Poller给它返回的所有发生事件的channel列表了
        activeChannels->push_back(channel);
    }
}

// 由Channel中的update方法发起调用，向epoll实例中更新Channel所关注的事件
/**
 *                 EventLoop
 *  ChannelList                  Poller
 *                            ChannelMap<fd, Channel*>      epollfd
*/
void EPollPoller::updateChannel(Channel *channel)
{
    /**
     * channel更新所关注的事件有三种情况：
     * 第一种：channel是新创建的，从来没关注过事件，自然也就不在epoll实例中
     * 第二种：channel之前关注过某事件，现在要更改，也就是说该channel已经在epoll实例中
     * 第三种：channel之前对任何事件都不感兴趣，已经从epoll实例中摘除了（events_），但是没有从channels_中删除（因为从channels_中删除的话肯定会置为kNew）
    */
    const int index = channel->index();
    LOG_INFO("function name=%s fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    // 根据channel里面的index_的值进行操作
    // 针对1、3两种情况的处理方式
    if (index == kNew || index == kDeleted) {
        if (index == kNew) {
            int fd = channel->fd();
            channels_[fd] = channel;
        }

        // 修改index_
        channel->set_index(kAdded);
        // 注意channel没添加过这里往epoll里面添加一下，为什么kDeleted的Channel不需要在channels_中添加一下呢？
        // 起始哟版index不会为kDeleted，因为从epoll实例上摘掉Channel后紧接着就会从channels_中删掉该channel，没有机会处于kDeleted状态
        update(EPOLL_CTL_ADD, channel);
    }
    // 针对第二种情况的处理方式
    else {
        int fd = channel->fd();
        // 如果某个channel对任何事件都不感兴趣，表示需要重epoll实例中删除掉该channel
        // 逻辑上的删除，不修改channels_(就是那个ChannelMap)
        // 真正从ChannelMap中删除是通过removeChannel函数去删，该函数由Channel的remove函数调用
        if (channel->isNoneEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        // 否则就去epoll中修改epoll_event事件
        else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 由Channel中的remove方法发起调用，功能是从poller的ChannelMap中去除Channel
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("function name=%s => fd=%d\n", __FUNCTION__, fd);

    // 别忘了如果该channel注册在epoll实例上也要顺便移除
    int index = channel->index();
    if (index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }
    // 把channel的index_设置为没有被添加到epoll中
    channel->set_index(kNew);
}

// 往epoll中修改，并没有在ChannelMap中修改
// 根据选择的操作进行添加文件描述符、修改文件描述符或删除文件描述符
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    bzero(&event, sizeof event);
    
    int fd = channel->fd();

    event.events = channel->events();
    // 将ptr指针指向channel对象，这样当epoll_wait通知哪些fd收到数据时可以顺藤摸瓜找到对应的channel对象
    event.data.ptr = channel;
    std::cout << "EPollPoller::update中channel对象被绑定到event.data.ptr中" << std::endl;
    std::cout << "该channel对象的fd为" << channel->fd() << std::endl;
    /********
     * 注意event.data是个union，只能选一个字段使用，前面已经用了ptr字段了，这里再用fd字段就会覆盖掉ptr字段
     * 导致后面找不到channel对象的地址
    *********/
    ////////////////////// event.data.fd = fd;

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else {
            LOG_FATAL("epoll_ctl mod/add error:%d\n", errno);
        }
    }
}
