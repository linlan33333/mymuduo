## EPollPoller类

#### 一、EPollPoller类的作用

EPollPoller类继承自Poller类，完成Poller基类所设置的任务，除此之外EPollPoller还需要及时的更新Channel对象实际所发生的事件。

#### 二、EPollPoller类的组成成分

##### 1、EPollPoller的成员变量上看有两个

* 创建的epoll实例所对应的文件描述符
* epoll实例需要监听的epoll_event数组

##### 2、EPollPoller的成员函数从作用和功能上看分为三类：

* 监听并将发生事件的channel上交到EventLoop的函数poll和fillActiveChannels，其中poll函数负责监听哪些Channel对象发生事件，而fillActiveChannels函数负责将发生事件的Channel对象集中起来交给EventLoop对象。

* 负责修改Channel所关心的事件的updateChannel函数，它需要根据Channel对象的状态先判断在没在epoll实例中，再根据不同的情况选择:

  (1) 将Channel对象添加到Poller对象的ChannelMap中，再注册到epoll实例中。

  (2) Channel对象已经在ChannelMap中了，则修改epoll实例中所关心的事件。

  (3) 从epoll实例中删除该channel对象。

> 这里先挖一个坑：什么时候Channel对象会设置为对任何事件不感兴趣，只需要从epoll实例中移除但不需要从Poller中的ChannelMap中移除？

* 负责从Poller中的ChannelMap中删除Channel对象，一般认为此时用户连接已经彻底断开。

#### 三、EPollPoller类中的设计技巧

##### 1、充分利用了epoll_event.data字段绑定channel对象

如果针对socket的fd设计了一个包装类，如Channel类，那么将fd注册到epoll实例的同时，可以将包装类的指针存放在epoll_event.data字段：

```C++
epoll_event中的data字段类型：
typedef union epoll_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_6 u64;
}
```

注意data字段是个union，可以选择一种一种类型进行赋值，在EPollPoller中选择指针类型，存储socketfd所属的Channel对象的指针。

```C++
epoll_event.data.ptr = channelPtr;
```





