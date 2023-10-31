## Muduo库运行流程

这里将沿着几条主线任务讲述Muduo库完成这些任务经历了哪些操作，以echo回射服务器为例，描述：

* 服务器启动运行
* 服务器监听到新连接$\longrightarrow$建立连接$\longrightarrow$接受数据$\longrightarrow$发送数据$\longrightarrow$关闭连接
* 服务器关闭运行

#### 一、服务器启动运行

用户自己需要定义一个服务器类EchoServer，包含成员变量EventLoop指针和TcpServer对象，同时设置好用户四大回调操作以及子线程数量。注意四大回调操作中，用户可以获取TcpConnection对象指针，从而使用TcpConnection对象的部分接口。

**主线程**

* 早早创建好mainLoop对象，设置好IP地址和端口后实例化服务器类。

  > 实例化服务器类TcpServer时Muduo库流程如下：
  >
  > * TcpServer类实例化时记录下mainLoop对象，IP地址端口号。
  >
  > * TcpServer类实例化时创建Acceptor对象：
  >
  >   > - Acceptor类实例化时创建listenfd套接字，同时封装成Socket对象<font color=red>（这一步进行listen操作）</font>
  >   >
  >   > * Acceptor类实例化时将listenfd套接字包装成Channel对象
  >   > * Acceptor类实例化时通过Socket对象的接口设置listenfd的各个属性（如端口复用、允许多套接字监听统一端口等）
  >   > * Acceptor类实例化时通过Socket对象的接口为listenfd绑定端口<font color=red>（这一步进行bind操作）</font>
  >   > * Acceptor类实例化时为Channel对象设置读事件回调函数（此时的回调函数是残缺的，不完整的）
  >
  > * TcpServer类实例化时创建EventLoopThreadPool对象。
  >
  > * TcpServer类实例化时完善Acceptor类为Channel对象设置的读事件回调函数

* 调用TcpServer类的start函数，启动TcpServer服务器（同时传入用户四大回调操作之一——线程初始化回调操作）

  > 启动TcpServer服务器流程如下：
  >
  > * 启动EventLoopThreadPool线程池<font color=red>（这一步创建出子线程运行epoll_wait循环）</font>：
  >
  >   > * 按照提前设置好的线程数量依次创建EventLoopThread线程对象：
  >   >
  >   >   > EventLoopThread线程对象创建Thread对象，创建Thread对象的同时设置好它的线程函数
  >   >
  >   > * 调用startLoop方法让Thread线程对象启动线程执行线程函数：
  >   >
  >   >   > * 创建EventLoop对象，执行用户的线程初始化回调操作
  >   >   > * 启动EventLoop对象的loop函数让该子线程陷入epoll_wait循环中<font color=red>（此时是空循环，没有任何connfd）</font>
  >   >
  >   > * 将EventLoop对象添加到管理容器中管理
  >
  > * 让mainLoop立刻执行如下回调操作：
  >
  >   > * 通过Socket对象的接口运行listen函数开始监听<font color=red>（这一步进行listen操作）</font>
  >   >
  >   > * 将Channel挂载到Poller中<font color=red>（注意此时还没有进行epoll_wait循环）</font>：
  >   >
  >   >   > * 将Channel添加到Poller的管理容器中
  >   >   > * 将listenfd添加到epoll实例中<font color=red>（这一步进行epoll_ctl操作添加listenfd）</font>

* 调用EventLoop的loop函数开启loop循环：

  > 让主线程进入epoll_wait状态<font color=red>（循环监听listenfd）</font>

至此，服务器启动完毕，能够正常监听并建立新连接。

#### 二、服务器监听到新连接$\longrightarrow$建立连接$\longrightarrow$接受数据$\longrightarrow$发送数据$\longrightarrow$关闭连接

主线程此时处于loop循环中（在```EventLoop::loop```函数中打转），当监听到新连接时，依次执行以下步骤：

* 调用EPollPoller中的poll函数将包装listenfd的Channel放入EventLoop中的activeChannels_容器中。<font color=red>（这一步完成监听到新连接）</font>执行细节如下：

  > * EPollPoller::poll函数运行epoll_wait，获知有多少个文件描述符发生事件（实际只有一个listenfd），接着调用成员函数fillActiveChannels将Channel填写到EventLoop中的activeChannels_容器中。
  > * fillActiveChannels函数从events\_容器中依次取出epoll_event结构体，根据结构体中的Channel指针找到对应的Channel，将其添加到activeChannels\_容器中。注意：events_容器是交给epoll实例存放发生事件的epoll_event结构体的数组。
  > * 检查events_容器是否被完全填充满了，如果被填充满，说明可能还有epoll_event没放进去，需要扩容

* 遍历activeChannels_容器，依次取出Channel调用相应的读事件回调函数。<font color=red>（这一步完成建立连接）</font>执行细节如下：

  > 调用listenfd的Channel的读事件回调函数，本质上是Acceptor::handleRead函数，具体流程如下：
  >
  > * 通过Socket对象的接口调用accept函数，返回connfd<font color=red>（这一步进行accept操作）</font>
  >
  > * 调用TcpServer::newConnection函数完成剩余回调操作：
  >
  >   > * 调用EventLoopThreadPool::getNextLoop函数获取一个subLoop对象指针
  >   >
  >   > * 为connfd创建TcpConnection对象，并将其添加到connections_方便后续管理。注意此时TcpConnection对象创建的同时会设置Channel的四大回调函数以及套接字属性。
  >   >
  >   > * 将TcpConnection对象加入connections_容器中管理。
  >   >
  >   > * 为用户的三个回调操作交给TcpConnection对象，后续会调用。同时设置TcpConnection对象关闭连接时的回调操作。
  >   >
  >   > * 让subLoop立刻执行回调操作TcpConnection::connectEstablished函数：
  >   >
  >   >   > * 向poller添加Channel对象到管理Channel的容器中，像epoll实例中注册connfd，设置TcpConnection对象的状态为已连接，同时调用建立连接、断开连接时的回调函数connectionCallback_，这是用户传过来在每次建立连接时需要调用的函数。<font color=red>（这一步进行epoll_ctl操作添加connfd）</font>
  >   >   > * 让Channel对象反向绑定自己，由于封装connfd的Channel调用的读写关错四大回调函数是TcpConnection的成员函数，因此得传入this指针，这就得保证Channel对应的TcpConnection对象不能先没了，就需要使用弱智能指针机制在调用TcpConnection的成员函数之前观察TcpConnection对象是否还活着。

将视线移到子线程中来，当子线程接受到数据时，依次执行以下步骤：

* 调用EPollPoller中的poll函数将包装connfd的Channel放入EventLoop中的activeChannels_容器中。<font color=red>（这一步完成监听到客户端数据）</font>执行细节同上。

* 遍历activeChannels_容器，依次取出Channel调用相应的读事件回调函数。执行细节如下：

  > 调用connfd的Channel的读事件回调函数，本质上是TcpConnection::handleRead函数，具体流程如下：
  >
  > * 调用Buffer中的readFd函数将到来的数据读到inputBuffer_中。
  > * 调用用户传入的回调操作之一——接收数据回调函数onMessage，做相应的业务。在echo回射服务器中回调操作是读取inputBuffer_中的数据，然后发回去。

* 上面说到onMessage回调函数中会将数据发回去。<font color=red>（这一步完成给客户端发送数据）</font>其发送数据过程如下：

  > onMessage回调函数调用TcpConnection::send接口发送数据给客户端，其过程如下：
  >
  > * send函数调用sendInLoop函数发送数据，sendInLoop函数执行过程如下：
  >
  >   > * 判断写缓冲区outputBuffer_中有没有上次没发完剩的数据，如果没有剩的数据，那么调用write系统调用给客户端发数据。发完后判断一下这一次有没有把数据一次性发完：
  >   >
  >   >   - 如果一次性发完了那没事了，只需要调用用户的回调操作——发送数据回调函数（如果有的话），做相应的业务。
  >   >
  >   >   - 如果没有一次性发完，需要先将没发送完的数据存放到outputBuffer_中，接着需要向epoll注册connfd写事件，当监听到写事件发生时，调用Channel的写事件回调函数TcpConnection::handleWrite方法继续把剩余数据发送出去。其函数逻辑如下：
  >   >
  >   >     > * 调用Buffer中的writeFd函数将outputBuffer_中剩下的数据发送出去
  >   >     > * 如果这一次能将剩下的数据全部发完，那么在epoll实例中设置connfd对读事件不感兴趣，从而避免触发不必要的读事件。接着调用用户的回调操作——发送数据回调函数（如果有的话）
  >   >     > * 如果还不能将剩下的数据全部发完，那么发到哪算哪，剩下的在下次读事件触发后继续发
  >   >
  >   > * 如果有上次剩余的数据没法完，那么把这次要发的数据添加到outputBuffer_中，交给Channel的写事件回调函数去发送，流程同上。
  
* 接着onMessage回调函数会关闭该连接，直接调用TcpConnection::shutdown函数即可。其执行过程如下：

  > * 先判断连接的状态是否是断开状态，如果没断开，则设置TcpConnection状态为断开状态
  >
  > * 接着让subLoop立刻执行TcpConnection::shutdownInLoop回调函数：本质上调用了Socket类提供的shutdownWrite接口关闭写端，进行前两次挥手操作。<font color=red>（这一步进行shutdown操作）</font>
  >
  > * 当对方也断开连接（完成四次挥手的剩下两次挥手操作，也就是发送FIN报文）时，如果发送的FIN报文不携带数据，那么将触发EPOLLHUP事件但不触发EPOLLIN事件，此时epoll_wait检测到事件，按照之前的流程，需要放入activeChannels\_容器中上报给subLoop对象，subLoop对象取出Channel调用它相应的关闭回调函数TcpConnection::handleClose方法关闭连接，其函数逻辑如下：
  >
  >   > * 修改连接的状态为kDisconnected，将Channel对象从epoll实例中删除
  >   >
  >   > * 创建该TcpConnection对象的shared_ptr智能指针，让该TcpConnection对象的引用计数加一，防止下面第四步中TcpServer::connections_容器删除该对象的智能指针导致引用计数为0从而析构掉该对象。现在该TcpConnection对象的引用计数为2。
  >   >
  >   > * 调用用户设置的建立连接、断开连接时需要进行的回调操作connectionCallback_
  >   >
  >   > * 调用TcpServer::removeConnection函数，将上面第二步中的智能指针传进去，该函数本质上调用了TcpServer::removeConnectionInLoop函数：
  >   >
  >   >   > * 从TcpServer::connections_容器中删掉该TcpConnection的智能指针，这导致TcpConnection对象的引用计数减一，现在该TcpConnection对象的引用计数为1，唯一用到该对象的指针是传进该函数的conn指针，看上面第二步。
  >   >   > * 让TcpConnection对象所在的subLoop对象立即执行回调函数TcpConnection::connectDestroyed，该函数将Channel对象从Poller中的ChannelMap中去除掉
  >   >
  >   > * handleClose方法调用结束，TcpConnection对象的shared_ptr智能指针析构，此时引用计数为0，析构该TcpConnection对象
  >
  >   如果发送的FIN报文携带数据，那么将触发EPOLLIN事件的读事件回调操作，关闭该连接就交给keepalive机制去关闭
  >
  > * TcpConnection对象析构使得它的成员变量Channel对象析构，Socket对象析构，而Socket对象析构使得调用close关闭connfd，连接被彻底释放

至此，该连接已关闭。

#### 三、服务器关闭运行

服务器关闭连接，主线程会先析构服务器类EchoServer，再析构EventLoop对象，析构服务器类EchoServer的过程如下所示：

* 析构EchoServer中的TcpServer对象，调用TcpServer类的析构函数：

  > * 依次取出TcpConnection对象，在TcpConnection对象所在的subLoop中立刻执行TcpConnection::connectDestroyed回调函数：
  >
  >   > * 将连接状态设为kDisconnection，将Channel对象从epoll实例中去掉，调用用户设置的建立连接、断开连接时需要进行的回调操作connectionCallback_
  >   > * 将Channel从Poller中的ChannelMap中去除掉
  >
  > * 每结束一次循环，该TcpConnection对象的智能指针引用计数从1变为0，使得该对象被析构，从而析构Channel对象和Socket对象，而Socket对象析构使得调用close关闭connfd，连接被彻底释放
  >
  > * 接下来析构EventLoopThreadPool对象，线程池对象的析构又会带动线程和subLoop对象的析构，而subLoop对象析构会关闭wakeupFd以及析构EPollPoller对象，进而关闭epoll实例，扫尾工作完成后子线程全部不复存在，资源全部得以释放。

接着析构mainLoop对象，mainLoop对象析构会关闭wakeupFd，析构EPollPoller对象，从而关闭epoll实例等等，后续不再赘述。

