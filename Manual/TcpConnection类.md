## TcpConnection类

#### 一、TcpConnection类的作用

TcpConnection类的作用是将connfd打包成Channel，设置好该connfd的各个接口如写数据，读数据等作为Channel的回调操作。需要注意的是，该类仅仅提供各个操作Channel的接口给TcpServer使用，并不能分发这些Channel，也不能发起连接。具体的作用如下：

* 包装connfd：在构造函数中将connfd包装成Channel，给该Channel设置四大事件回调函数（读、写、关闭、出错），同时设置该socket保活机制。
* 连接管理：```TcpConnection```类提供管理连接状态的接口，如建立连接（包装好Channel后）、断开连接（直接调用close关闭连接）、关闭连接（调用shutdown关闭连接）。
* <font color=red>避免串话</font>：

#### 二、TcpConnection类的组成

##### 1、TcpConnection类的成员变量分为四类：

* TcpConnection对象的属性和状态

  > state_表示连接的状态，用来控制业务逻辑，如已断开连接那肯定不能发数据了
  >
  > name_就是这个TcpConnection连接的名字
  >
  > localAddr_表示当前主机的IP地址端口号
  >
  > peerAddr_表示对端的IP地址和端口号
  
* TcpConnection对象的回调函数，由TcpServer传过来，-----------------，注意这不是设置Channel读写关错四大回调函数

* TcpConnection对象的上级和下属

  > EventLoop对象的指针loop_，这个EventLoop是subLoop，由TcpServer轮询选出来并传过来的。
  >
  > Socket对象智能指针，将connfd包装成Socket，方便调用Socket提供的接口设置一些套接字属性，后面关闭连接时也能直接调用接口关闭，方便快捷。
  >
  > Channel对象智能指针，TcpConnection需要包装的Channel对象

* 收发数据的缓冲区

  > 接受数据的缓冲区inputBuffer_
  >
  > 发送数据的缓冲区outputBuffer_
  >
  > 水位线 highWaterMark\_，当outputBuffer\_中积攒的数据量超过该数值时，将触发highWaterMarkCallback\_回调函数
  >
  > highWaterMarkCallback\_回调函数，当outputBuffer\_中积攒的数据量过高时调用，但是具体的业务逻辑没见着，是别的类传过来的，并没有在TcpConnection类中定义，目前也没有在别的类中发现这一函数的定义

##### 2、TcpConnection类的成员函数

* 构造函数

  > 给TcpConnection的成员变量赋值，包括表示TcpConnection对象的属性和状态的那些变量，以及TcpConnection对象的上级和下属（没创建的现场创建）。
  >
  > 给Channel对象（connfd的Channel对象）绑定关错四大回调函数，它们都是TcpConnection类的成员函数，下面会提到。
  >
  > 设置connfd套接字属性，如保活机制，直接调用Socket对象的接口。

* connectEstablished函数

  > 该函数干了两件比较重要的事情：
  >
  > * 向poller注册channel的读事件，设置状态为已连接，同时调用建立连接、断开连接时的回调函数connectionCallback_，这是用户传过来在每次建立连接时需要调用的函数。
  > * 让Channel对象反向绑定自己，由于封装connfd的Channel调用的读写关错四大回调函数是TcpConnection的成员函数，因此得传入this指针，这就得保证Channel对应的TcpConnection对象不能先没了，就需要使用弱智能指针机制在调用TcpConnection的成员函数之前观察TcpConnection对象是否还活着。

* shutdown函数和shutdownInLoop函数

  > shutdown函数用于给EventLoop对象传入上级回调函数shutdownInLoop函数，要求EventLoop立刻关闭该连接。一般是服务器主动关闭连接时用到该函数。
  >
  > shutdownInLoop函数就是调用Socket类的shutdownWrite优雅地关闭连接。
  
* connectDestroyed函数

  > 使用close系统调用断开连接，该函数调用时机有两种：
  >
  > * 当关闭服务器时，TcpServer对象要析构，析构时就会主动把所有连接都断开，也就是把每个TcpConnection对象都调用一遍本函数
  > * 当客户端主动断开连接，Channel会检测到关闭事件，此时会调用的回调函数中就有一步需要调用该函数close掉连接。
  >
  > 该函数的主要功能是将channel从Poller中的ChannelMap中摘掉，同时检查连接的状态，对于调用时机1来说此时连接状态肯定是kConnection状态，因此需要先将Channel对象从epoll中删除，再将Channel对象从Poller中移除，但对于调用时机2来说，此时已经是kDisconnected状态了（不信看看handleClose怎么写的），Channel对象已经从epoll中删除了，因此此时只需要将Channel对象从Poller中移除就行。

* handleClose函数

  > Channel的四大回调函数之一（包装connfd的Channel），当检测到与客户端<font color=red>断开连接时调用</font>，干了如下两件事：
  >
  > * 修改连接的状态为kDisconnected，将Channel对象从epoll实例中删除
  >
  > * 创建该TcpConnection对象的shared_ptr智能指针，让该TcpConnection对象的引用计数加一，防止下面第四步中TcpServer::connections_容器删除该对象的智能指针导致引用计数为0从而析构掉该对象。现在该TcpConnection对象的引用计数为2。
  >
  > * 调用用户设置的建立连接、断开连接时需要进行的回调操作connectionCallback_
  >
  > * 调用TcpServer::removeConnection函数，将上面第二步中的智能指针传进去，该函数本质上调用了TcpServer::removeConnectionInLoop函数：
  >
  >   > * 从TcpServer::connections_容器中删掉该TcpConnection的智能指针，这导致TcpConnection对象的引用计数减一，现在该TcpConnection对象的引用计数为1，唯一用到该对象的指针是传进该函数的conn指针，看上面第二步。
  >   > * 让TcpConnection对象所在的subLoop对象立即执行回调函数TcpConnection::connectDestroyed，该函数将Channel对象从Poller中的ChannelMap中去除掉
  >
  > * handleClose方法调用结束，TcpConnection对象的shared_ptr智能指针析构，此时引用计数为0，析构该TcpConnection对象
  >
  > * TcpConnection对象析构使得它的成员变量Channel对象析构，Socket对象析构，而Socket对象析构使得调用close关闭connfd，连接被彻底释放
  >
  > 解释一下如何判断是否断开连接：当我方收到对方发送FIN报文时，就会触发EPOLLHUP事件，因此有以下两种情况：
  >
  > * 对方发送FIN报文，没有携带数据。
  > * 对方发送的FIN报文中携带了剩余数据。
  >
  > 因此需要对这两种情况区分：
  >
  > 第一种情况下会触发EPOLLHUP事件但不会触发EPOLLIN事件，因为没有数据需要读，此时走handleClose流程即可。
  >
  > 第二种情况下会在触发EPOLLHUP事件的同时触发EPOLLIN事件，需要读取数据，此时Channel会调用handleRead来处理数据而不是handleClose关闭连接，因此后续数据处理完后如果要发送给对方，那么由于对方已经关闭写端，导致发送失败进而关闭套接字，断开连接。

* handleRead函数

  > Channel的四大回调函数之一（包装connfd的Channel），当检测到来数据时会通过Buffer::readFd函数将数据读到Buffer中，接着调用用户传来的回调函数将数据传进去（注意Tcp服务器的用户一般是应用层服务器，他们需要拿到这些数据然后解析成应用层协议，接着去做一些业务操作）。当然，用户断开连接和连接出错等情况都会考虑进去。

* handleWrite函数

  > Channel的四大回调函数之一（包装connfd的Channel），当检测到connfd的写缓冲区没满，可以写数据时会触发写事件，通知此时可以写数据到connfd中。该函数会将outputBuffer\_中的<font color=red>没写完的数据</font>写到connfd中，为什么说是没写完的数据请看下面的调用逻辑。注意接下来用了一个小技巧：当outputBuffer\_中没有数据需要写时就设置Channel不关注写事件，避免不必要的写事件触发。那么什么时候会重新关注写事件，请看sendInLoop函数。
  >
  > 该函数是如何让Channel写数据的呢？它的<font color=red>调用逻辑</font>如下：
  >
  > * 第一步：起初没有给Channel注册对写事件感兴趣，用户编写了业务函数onMessage最终传给了TcpConnection的messageCallback\_，在业务函数onMessage中用户调用了TcpConnection::send函数发送数据。到这一刻为止是触发不了Channel的写事件回调函数，也就是该handleWrite函数的。
  > * 第二步：TcpConnection::send函数调用了sendInLoop函数，该函数才是开始发送数据的函数，当数据没有发完，就给Channel注册写事件，等Channel触发写事件执行回调也就是handleWrite函数时继续发送没发完数据。不断循环
  > * 第三步：当handleWrite终于把数据都发完时，删除该Channel的写事件，相当于回到了第一步。后续有新数据要写时让send函数写，没写完再让handleWrite函数继续写。这就是为什么说handleWrite的作用是将outputBuffer\_中的没写完的数据写到connfd中。

* handleError函数

  > Channel的四大回调函数之一（包装connfd的Channel），当检测到错误时调用，做的事其实就是在日志中打印错误号。

* send函数和sendInLoop函数

  > <font color=red>send函数是暴露给用户的接口，用户有什么数据需要发送时就在给TcpServer的messageCallback回调函数中直接调用即可，把要发的数据传进去</font>，作用是当有数据需要发送时，该函数本质上调用了sendInLoop函数进行发送数据。
  >
  > sendInLoop函数负责将要写的数据写入connfd的缓冲区，让内核发给客户端，如果这一趟将数据全部发送完了，就不需要给Channel注册写事件了；但如果这一趟数据没能发送完，那么会将剩余的数据保存到缓冲区中，然后给Channel注册写事件，当写事件触发时会调用handleWrite函数，把<font color=red>剩余的数据</font>写入connfd中发送过去，如果还没写完就继续等下次触发写事件，直到数据全部发完，handleWrite会删除写事件，避免不必要的写事件触发。

#### 三、TcpConnection的设计技巧

