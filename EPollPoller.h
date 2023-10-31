#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <vector>
#include <sys/epoll.h>

class Channel;

/**
 * epoll的使用
 * epoll_create
 * epoll_ctl    add/mod/del
 * epoll_wait
*/
class EPollPoller : public Poller
{
public:
    // 干着epoll_create的事情
    EPollPoller(EventLoop* loop);
    ~EPollPoller() override;

    // 重写基类Poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;
private:
    static const int kInitEventListSize = 16;       // 给EventList初始化的长度

    // 填写活跃的连接，被updateChannel和removeChannel所调用
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    // 更新channel对象，其实就是更新fd实际发生的事件revents_，被updateChannel和removeChannel所调用
    void update(int operation, Channel* Channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;       // 通过epoll_create创建的epoll实例文件描述符存储在这
    EventList events_;  // epoll_wait监听到发生事件的socketfd就存在这个列表中，当然这里得是加强版的socketfd——Channel对象
};