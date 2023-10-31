## Poller基类

#### 一、Poller类的作用

Poller类的作用是监视和等待socket文件描述符（也就是监听Channel对象）上的事件，然后通知事件循环（EventLoop对象）来处理这些事件。具体包括：

* 事件监视：负责监视一组Channel对象上发生的事件，这些事件包括可读事件、可写事件、错误事件等。
* 事件等待：等待这些Channel对象上的事件发生。
* 事件通知：当一个或多个文件描述符上的事件发生时，`Poller`将事件信息传递给事件循环（EventLoop对象）。事件循环会根据事件的类型来执行相应的回调函数，执行用户定义的业务逻辑。这样实现了事件驱动的编程模型。

Poller类就是Reactor模型中的Demultiplex事件分发器。

#### 二、Poller类的组成成分

##### 1、Poller类的成员变量按功能和作用来看只有一类：它的上司EventLoop指针和下属ChannelMap集合

> EventLoop指针用于找到负责人来上报事件，ChannelMap集合用于管理它要监听的Channel对象们。

##### 2、Poller类的成员函数按功能和作用来看分为两类：

* 担当epoll相关操作的功能函数：包括poll函数（相当于执行epoll_wait）、updateChannel（相当于执行epoll_ctl中的EPOLL_CTL_MOD功能）、removeChannel（相当于执行epoll_ctl中的EPOLL_CTL_DEL功能）。
* 查看Channel对象是否存在的函数hasChannel。

#### 三、Poller类中的设计技巧

##### 1、基类想要设计一个成员函数能直接创建子类对象并返回，可以设计一个工厂子类来实现该成员函数

为了能在Poller中直接获取子类的实例对象，如EpollPoller、PollPoller，同时满足两个条件：

* 子类的类型可以扩充，而且满足开闭原则，不改动Poller类的代码。
* 父类不能包含子类的头文件，否则这么做有违依赖倒置原则、单一职责原则。
* 能根据不同平台的特性返回最适合的子类对象，如Windows平台返回WSPoll对象、Linux平台返回EPollPoller对象。

**为此，Poller类设计了子类DefaultPoller，并且定义了一个静态成员函数用以返回最适合当前平台的Poller子类对象**，函数签名如下：

```static Poller* newDefaultPoller(EventLoop* loop);```

该函数在子类DefaultPoller中实现，之所以不在Poller中实现，原因在于：

* 子类有哪些类型Poller是不知道的，更何况子类未来还能扩充，因此该函数不好在Poller中实现。
* 子类可以包含其他的子类头文件，但父类Poller这么做有违依赖倒置原则、单一职责原则，而且会产生循环依赖导致编译出错。

因此该函数设计成静态即方便Poller对象调用，也能隔离父类和子类的依赖关系，非常巧妙。

