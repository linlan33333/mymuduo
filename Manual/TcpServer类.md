## TcpServer类

#### 一、TcpServer类的作用

TcpServer类的作用是创建subLoop线程，创建Acceptor对象（创建并封装listenfd），并在这一切准备就绪时开启TCP服务器，让主线程陷入epoll_wait循环不断处理新连接。具体作用如下：

* 提供设置subLoop线程数量的接口
* 提供启动服务器的接口
* 提供四大用户回调函数接口：线程初始化完成时用户想做的操作、连接建立与断开时用户想做的操作、收到TCP报文后用户想做的操作、发送TCP报文后用户想做的操作
* 编写Acceptor中的Channel收到读事件后的回调操作：该回调操作是Acceptor::handleRead中的一部分，从而能够将四大用户回调函数交给Acceptor管理，由Acceptor传给TcpConnection对象。

#### 二、TcpServer类的组成

##### 1、TcpServer类的成员变量

TcpServer类的成员变量分为三类：

* TcpServer的属性与状态

  > 标志TcpServer是否已启动的started\_：防止TcpServer被重复启动多次。
  >
  > 服务器的名称name\_
  >
  > 服务器的IP和端口号ipPort\_

* TcpServer的下属

  > 主线程中用户提前创建好的EventLoop对象mainLoop_
  >
  > 管理所有子线程的线程池threadPool_
  >
  > 创建与管理listenfd的Acceptor对象acceptor_
  >
  > 保存所有TcpConnection对象指针的connections_
  
* TcpServer保存的四大用户回调函数

  > 线程初始化完成时用户想做的操作threadInitCallback_
  >
  > 连接建立与断开时用户想做的操作connectionCallback_
  >
  > 收到TCP报文后用户想做的操作messageCallback_
  >
  > 发送TCP报文后用户想做的操作writeCompleteCallback_

##### 2、TcpServer类的成员函数

* 构造函数：构造函数做了两件事：初始化上面的成员变量，该创建的创建，该赋值的赋值；最重要的是将监听到新连接后需要执行的业务逻辑打包成函数对象交给Acceptor。在第一部分中说过，Acceptor类在Channel收到读事件后的回调操作中，有一部分业务逻辑是定义在TcpServer类中的，因此TcpServer需要把这部分业务逻辑打包成函数对象交给Acceptor，从而能够拼成完整的回调操作。

* 析构函数：在析构函数中会将每一个TcpConnection对象拎出来断开连接，走close方式。

* setThreadNum函数：告诉threadPool_该创建多少个子线程

* start函数：启动TCP服务器的入口函数，调用后启动线程池，接着让listenfd开始listen，再注册到epol实例中，注意该函数并不会让主线程运行mainLoop的epoll_wait循环，得用户手动调用mainLoop的loop方法才行。

* newConnection函数：这就是listenfd监听到读事件后需要执行的一部分回调操作，在Acceptor::handleRead中调用，其业务逻辑如下：

  > 第一步：调用线程池的轮询算法获取一个subLoop对象，创建TcpConnection对象。
  >
  > 第二步：将用户回调函数中的connectionCallback\_、messageCallback\_、writeCompleteCallback\_赋给TcpConnection对象。同时将关闭连接的回调函数赋给TcpConnection对象，<font color=red>注意这个回调函数是TcpServer自己的成员函数removeConnection</font>，并非用户写好的回调函数。
  >
  > 第三步：让subLoop对象立刻执行回调操作TcpConnection::connectEstablished，从而将包装了connfd的Channel加到该subLoop中。至此listenfd就执行完了全部读事件回调操作，成功将connfd包装成Channel并添加到subLoop中的Poller里。
  
* removeConnection函数和removeConnectionInLoop函数

  > removeConnection函数：让subLoop立刻执行回调操作，该回调操作是removeConnectionInLoop函数
  >
  > removeConnectionInLoop函数：从成员变量connections_中移除该TcpConnection对象指针，接着让subLoop立刻执行TcpConnection::connectDestroyed函数关闭连接。

#### 三、TcpServer类的设计技巧

