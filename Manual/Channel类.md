## Channel类的设计

#### 一、Channel类的作用

Channel类的作用是将socket文件描述符的事件（如套接字可读、可写等）与用户定义的事件处理函数关联起来，以便在发生特定事件时执行用户定义的回调操作。具体包括：

* 事件管理：Channel对象需要管理一个文件描述符（通常是套接字）以及需要监视的事件，包括可读事件（有数据可读）、可写事件（可以写入数据）、错误事件等。
* 事件处理：Channel类管理用户注册的回调函数，以便在事件发生时调用相应的回调函数。
* 事件分发：前面两个功能都是管理资源，这里的事件分发指的是调用相应事件的回调函数。`Channel`对象通常与事件循环（EventLoop）结合使用，事件循环负责等待事件的发生并分发事件给相应的`Channel`对象。一旦事件发生，`Channel`对象的回调函数将被调用，执行相应的操作。

#### 二、Channel类的组成成分

##### 1、Channel类的成员变量按功能和作用分为三部分：

* 它的上级单位，比如指向EventLoop的指针，指向TcpConnection对象的弱指针。
* 记录的fd和事件，如fd感兴趣的事件、fd身上实际发生的事件。
* 各个回调函数，如读事件回调函数、关闭连接回调函数等。

##### 2、Channel类的成员函数按功能和作用分为三部分：

* 设置它的成员变量，如它感兴趣的事件（也就是设置epoll_event）和各类事件发生时的回调函数。
* 获取它的相关信息，如获取它上级的EventLoop的指针，它管理的文件描述符fd，它注册了哪些感兴趣的事件，实际发生了哪些事件等。
* 定义它的业务函数：

> 1、执行发生事件的回调函数，也就是handleEvent函数和handleEventWithGuard函数，它俩配套使用
>
> 注意：Channel的回调函数是谁给的？如果这个Channel封装的是listenfd，那么由Acceptor类封装并赋好回调函数，如果这个Channel封装的是connectionfd，那么由TcpConnection类封装并赋好回调函数。
>
> 2、将自己从EPollPoller中删除的函数remove

#### 三、Channel类中的设计技巧

##### 1、在多线程访问共享对象时，使用weak_ptr检查对象资源是否存在

subReactor中有subLoop，用于处理已连接用户的请求。当它管理的channel对象有新的epoll事件发生时，就需要执行相应的回调函数，但这些回调函数本质上都是TcpConnection类的成员函数，因此调用时需要保证TcpConnection对象还活着。如何检查TcpConnection对象活没活着，就需要weak_ptr提权机制来检查。

这就是handleEvent函数的写法：先检查TcpConnection对象还活着没，再调用回调函数，进而调用TcpConnection对象的成员函数。

##### 2、操作位图时如何去掉某一位

disableReading函数的作用是去掉events的写事件，而events实际上是个位图，比如```EPOLLIN | EPOLLOUT | EPOLLPRI```，如何从中抠掉```EPOLLIN```，那就是先对```EPOLLIN```取反，再与上events即可。





