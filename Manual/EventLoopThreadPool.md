## EventLoopThreadPool类

#### 一、EventLoopThreadPool类的作用

```EventLoopThreadPool```类的作用是管理和维护一组`EventLoop`对象（也就是EventLoopThread对象，因为One loop per thread），以用于多线程的事件处理。具体作用如下：

* 创建子线程：`EventLoopThreadPool`会创建一组线程，每个线程都拥有一个独立的`EventLoop`对象。
* 分发连接与负载均衡：`EventLoopThreadPool`通常用于分发连接给不同的`EventLoop`。当新的客户端连接到服务器时，`EventLoopThreadPool`会采用轮询的方式选择一个`EventLoop`来处理该连接，从而实现了负载均衡。

* 销毁子线程：```EventLoopThreadPool```通过智能指针来管理所有的```EventLoopThread```对象，确保在该类析构时连带着将所有```EventLoopThread```对象都析构掉，达到销毁子线程的目的。

#### 二、EvetnLoopThreadPool类的组成成分

##### 1、EvetnLoopThreadPool类的成员变量按作用和功能分为三类：

* 它的下属单位：主要是存放所有的EventLoopThread对象的容器和存放所有的EventLoop对象的容器。此外还有mainLoop的EventLoop指针，这是为了当用户没有创建子线程时，那么轮询选择子线程处理连接就只能选择mainLoop了，因此会返回该mainLoop指针。
* 初始化线程的相关变量：如确定创建几个线程的线程数量numThreads\_、线程名称name\_。
* 记录轮询到哪个线程的变量next_。

##### 2、EventLoopThreadPool类的成员方法如下

* 设置线程数量的setThreadNum函数：设置好后调用start函数创建线程就会根据这个线程数量来创建。
* 创建线程的start函数：该函数会根据设置好的线程数量创建这么多EventLoopThread对象，完成它们的初始化操作，同时将EventLoopThread对象和EventLoop对象的指针存放到容器中方便后续管理。
* 轮询获取EventLoop对象的getNextLoop函数：该函数会返回EventLoop对象的指针，具体是哪一个根据轮询的结果从存放所有的EventLoop对象的容器中抓取，当然，如果没有子线程，那就返回mainLoop的指针。
* 获取存放所有EvetnLoop对象的容器的函数getAllLoops：该函数返回成员变——所有的EventLoop对象的容器。

#### 三、EventLoopThreadPool类的设计技巧

##### 1、为什么Muduo库不使用生产者消费者模型而是使用轮询的方式选择线程处理连接？

轮询方式的线程选择相对简单，减少了对线程间同步和通信的复杂性。

生产者-消费者模型可能需要更复杂的队列和同步机制来管理任务的生成和消费，在面对高并发场景加锁解锁非常非常频繁，上下文切换也会非常频繁，会导致服务器效率低下。

而轮询的方式简单高效，还能很好地达到负载均衡的效果：因为当连接数成千万级时，由大数定律可知每个连接的持续时间呈正态分布，因此采用轮询算法后每个子线程上的连接持续时间也呈现相同的正态分布，数学期望都相等，平均的连接时间相等，子线程的负载程度也相等，这就完美达到了负载均衡的效果。
