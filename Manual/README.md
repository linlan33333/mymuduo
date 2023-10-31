#### 手写Muduo网络库项目

本文件夹中的内容是重抄Muduo库核心类的总结，尽量把每一个类的作用与设计思路记录下来，方便复习。

阅读顺序如下：

Timestamp类的设计技巧（与TCP报文的“可选项”字段有关，可以先放一放）

CurrentThread类的设计技巧

Channel类

Poller基类

EPollPoller类

EventLoop类

Thread类

EventLoopThread类

EventLoopThreadPool类

InetAddress类

Socket类

Buffer类

Acceptor类

TcpConnection类

TcpServer类

最后梳理整个流程时走几个主线来阐述各个各个类之间的调用关系：

* 创建TcpServer对象时，各个子线程的EventLoop对象的创建过程
* TcpServer析构（服务器关闭）时，各个子线程的EventLoop对象是怎么退出的，子线程是怎么关闭的
* 有新连接收到时，mainLoop是如何将连接下发给子线程的