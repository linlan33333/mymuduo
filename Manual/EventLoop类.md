## EventLoop类

#### 一、EventLoop类的作用

EventLoop类创建的对象分为两种角色，一个是主线程中的EventLoop对象，称为mainLoop，只负责监听新用户连接。另一个子线程中的EventLoop，这个子线程专门来处理已连接用户的请求与响应，它的EventLoop对象称为subLoop。EventLoop类就是Reactor模型中的反应堆。

不管是mainLoop还是subLoop，EventLoop到底担任哪个角色由它的上级决定（如TcpServer类中的EventLoop、EventLoopThread类中的EventLoop），EventLoop只能机械地执行Channel的回调函数，以及上级交给他的回调函数，这就是成员变量pendingFunctors_的由来，里面的回调函数都是上级交给他的。

因此EventLoop类的作用需要按角色来看：

>**作为mainLoop：**
>
>**接受连接**：它的loop循环函数不断地监听新用户连接，也就是循环监听listenfd，通过Poller的epoll_wait实现。
>
>**执行Channel回调**：执行新连接建立时的回调函数，具体操作已经注册给Channel对象了。<font color=red>有哪些操作将在其他类中说明</font>
>
>**执行上级回调**：执行上级交给它的回调函数，注意由于wakeupFd机制，上级的回调不需要等到epoll_wait监听到新连接后再调用，可以<font color=red>随时、直接、立即执行</font>上级回调。如上级要求subLoop立即关闭循环（quit函数）等操作，后续在它的上级类也会提到其他操作。

>**作为subLoop：**
>
>**监听客户端连接数据**：它的loop循环函数不断地监听已建立的客户端连接有没有发送数据过来，通过Poller的epoll_wait实现。
>
>**执行Channel回调**：说明同上
>
>**执行上级回调**：说明同上

#### 二、EventLoop类的组成

##### 1、EventLoop类的成员变量按功能和作用分为三部分：

* 它的下属单位：比如所有发生事件的Channel集合activateChannel\_、所属的Poller对象poller\_。
* 控制行为的变量：如looping\_来控制开启循环、quit\_来控制关闭循环、callingPendingFunctors_控制是否立即执行上级回调函数等。
* 配合实现立即执行上级回调操作机制的变量：如wakeupFd\_、wakeupChannel\_等，该机制的实现方式将在第三部分说明。

##### 2、EventLoop类的成员函数按功能和作用分为三部分：

* 控制EventLoop对象开启或关闭的函数：既loop函数和quit函数

* 配合实现立即执行上级回调操作机制的函数：

  >wakeup()：解除当前线程的阻塞状态，使其能够往下执行上级回调操作
  >
  >handleRead()：向wakeupFd\_中写入数据，从而让epoll_wait解除阻塞
  >
  >runInLoop()：让该EventLoop对象立即执行上级的回调函数
  >
  >queueInLoop()：把上级回调函数放入队列中，唤醒loop所在的线程，执行回调函数
  >
  >isInLoopThread()：判断该EventLoop对象是否在创建出该对象的线程中
  >
  >doPendingFunctors()：执行回调
  
* 替Channel传话，帮助在epoll实例中修改Channel对象的函数：updateChannel函数和removeChannel函数，以及判断Channel是否在epoll实例中的hasChannel函数，这些本质上调用的还是Poller中的相应函数。

#### 三、EventLoop类中的设计技巧

##### 1、使用Linux中的eventfd来通知其他线程执行任务

Muduo库是如何实现立即执行上级（主反应堆线程）交给其他EventLoop对象（子反应堆线程）的回调操作的：

* 第一步：给每一个EventLoop对象创建一个eventfd，用来实现线程间的事件通知与线程同步，将其注册到EventLoop对象所管理的Poller实例中。

  >eventfd是Linux内核提供的一种机制，用于实现进程间或线程间的事件通知和同步。它是通过文件描述符的方式来进行事件通知的，可以用于多种并发编程场景，用途如下：
  >
  >* 事件通知： `eventfd`允许一个进程或线程向另一个进程或线程发送事件通知，通知对方发生了某种特定的事件。这可以用于协调多个并发执行的任务，以便它们在特定事件发生时执行相应的操作。
  >* 线程同步： `eventfd`可用作一种轻量级的同步机制，用于等待某个条件的发生。一个线程可以等待一个`eventfd`上的事件，而另一个线程可以在条件满足时向该`eventfd`写入事件通知。
  >* 线程池管理：在多线程编程中，`eventfd`可用于管理线程池中的线程。主线程可以使用`eventfd`通知工作线程执行任务，而工作线程可以使用它来报告任务的完成状态，<font color=red>Muduo库便是用了这么个方法</font>。
  >* 事件循环：在事件驱动编程中，`eventfd`可用于实现事件循环，使应用程序能够等待和处理不同类型的事件，例如网络事件、文件IO事件等。

* 第二步：在EventLoop对象中的loop循环函数中，循环监听eventfd和其他socketfd有没有事件发生。如果主反应堆线程有什么回调操作需要子反应堆执行，比如让子反应堆往epoll实例中添加一个socketfd、让子反应堆quit等，就往子反应堆线程的EventLoop对象的eventfd随便写数据，让子反应堆线程从EventLoop对象的epoll_wait函数中解除阻塞，接着往下循环执行。

* 第三步：在EventLoop对象中的loop循环函数中，循环执行属于socketfd的回调（也就是Channel的回调）以及属于eventfd的回调（也就是上级回调）。

  >可不可以在其他的函数中执行eventfd的回调（也就是上级回调）？
  >
  >不可以，以quit函数举例，如果在其他的函数中执行上级回调即quit函数，会因为线程可能阻塞在epoll_wait函数上（也就是那个loop循环）根本没法执行这些回调操作，也就导致无法即使调用quit函数销毁EventLoop对象。

这样就实现了有socketfd事件发生就执行Channel的回调，有eventfd事件发生就执行上级回调两不误。

##### 2、如何解决需要读vector容器中的数据，但vector容器可能随时有新数据写入时可能造成的漏读新数据的问题

EventLoop中将上级交给它的回调函数以vector容器保存，但执行这些回调操作的同时，有可能有新的回调操作过来，那么如何避免新来的回调操作被遗漏执行的问题？

EventLoop通过这样的流程来解决：

* 读vector时先上锁，不让其他线程写入数据。但为了避免让其他写线程等待时间过长，可以使用swap函数，把现有的vector数据转移到新的地方再慢慢消化，立即把锁释放，免得耽误其他写线程。
* 设置一个状态标志callingPendingFunctors_，当正在读取vector时该标志为true，不在时为false，那么其他写线程写入数据时检查该标志，如果该标志为true那么就调用wakeup函数避免EventLoop在下次循环时阻塞在epoll_wait上从而继续读取新来的回调函数，

