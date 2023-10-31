#include "Poller.h"

Poller::Poller(EventLoop *loop)
    : loop_(loop)
{
}

bool Poller::hasChannel(Channel *channel) const
{
    // 为什么不写成这种形式，因为要判断两个条件
    // if (channels_.find(channel->fd()) != channels_.end()) {
    //     return true;
    // }
    auto iter = channels_.find(channel->fd());
    // 要判断两个条件
    return iter != channels_.end() && iter->second == channel;
}

