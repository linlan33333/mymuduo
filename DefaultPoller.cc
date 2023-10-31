#include "Poller.h"
#include "EPollPoller.h"

#include <stdlib.h>

// 为什么不用策略模式或者原型模式，因为这里的newDefaultPoller方法要求直接就要返回一个子类的实例对象
Poller* Poller::newDefaultPoller(EventLoop* loop)
{
    if (getenv("MUDUO_USE_POLL")) {
        return nullptr;     // 生成poll实例
    }
    else {
        return new EPollPoller(loop);     // 生成epoll实例
    }
}
