## Acceptor类

#### 一、Acceptor类的作用

Acceptor类的作用是将监听新用户连接的listenfd打包成Channel对象，用于接受客户端连接请求，设置好Channel的读事件回调函数（将新连接分配给相应的`TcpConnection`对象），从而构建服务器端的监听套接字。具体功能如下：

* 监听套接字的创建：`Acceptor`负责创建并配置服务器端的监听套接字。这个监听套接字用于等待客户端的连接请求，将其封装成Channel对象注册到Poller中，同时给Channel对象设置读写关错四大回调函数（这里只需要设置读事件回调函数）
* 接受连接：`Acceptor`类的主要任务是调用`accept`函数来接受客户端连接。当有客户端连接请求到达时，`accept`会创建并封装一个新的套接字，表示与客户端建立的连接。
* 新连接的管理：`Acceptor`会将新的客户端连接封装为`TcpConnection`对象，并将其交由`EventLoop`中的某个线程处理。由`TcpConnection`对象负责管理连接的状态、事件处理、数据收发等。
* 监听套接字的析构：当关闭服务器时会将监听套接字从epoll实例中删除，对应的Channel对象等也将被析构。

#### 二、Acceptor类的组成

##### 1、Acceptor类的成员变量

* Acceptor需要创建的Socket对象（本质上就是listenfd），以及需要包装的Channel对象。
* 保存监听到新连接的回调函数newConnectionCallback\_，该回调函数本质上是TcpServer的newConnection函数，下面会提到该函数干了什么。
* 记录listenfd是否开始监听的标识listen\_

##### 2、Acceptor类的成员函数

* 构造函数：负责创建listenfd，并将其包装成Channel对象（包括设置读事件的回调函数，也就是下面介绍的handleRead函数），设置套接字属性（如端口复用等），绑定IP地址和端口号（也就是bind操作）。注意给Channel对象设置读事件的回调函数是handleRead函数，而handleRead函数又调用了TcpServer的newConnection函数。

* 析构函数：从epoll实例中摘掉listenfd，同时从Poller中移除listenfd的Channel。

* listen函数：为TcpServer类提供的接口，就是listen操作开启监听，同时挂载到epoll实例中。

* handleRead函数：该函数将被绑定到Channel的读事件发生时的回调函数上，作用如下

  > 当监听到新连接时，调用accept系统调用创建connfd，接着调用newConnectionCallback\_函数（本质就是TcpServer的newConnection函数）使用轮询算法取一个subLoop出来，将connfd包装成TcpConnection对象，设置好各种回调函数，然后将其挂载到subLoop中的epoll实例上。具体过程将在TcpServer类中介绍该函数。

#### 三、Acceptor类的设计技巧

