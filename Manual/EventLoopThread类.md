## EventLoopThread类

#### 一、EventLoopThread类的作用

EventLoopThread类的作用是创建与管理EventLoop对象（一定是subLoop），并在一个新线程中运行事件循环EventLoop，实现“One loop per thread”。具体作用如下：

* EventLoop的创建：`EventLoopThread`会在一个新的线程中创建一个`EventLoop`对象，该对象一定是扮演subLoop角色，因为主线程中一开始就已经创建好了mainLoop。这个`EventLoop`将在新线程中运行。
* Thread的创建：创建一个新的线程，从而在子线程中运行EventLoop的loop函数。
* 管理EventLoop对象和Thread对象的生命周期：在该对象析构时让EventLoop结束循环并退出，子线程退出。

#### 二、EventLoopThread类的组成

##### 1、EventLoopThread类的成员变量按作用和功能分为三类：

* 它的下属单位：该类所需要管理的EventLoop对象和Thread对象。
* 创建EventLoop对象时对该对象的初始化操作：通过回调函数callback_变量来保存。
* 控制线程同步的变量：如互斥锁与条件变量，用于同步startLoop函数和threadFunc函数之间的先后执行顺序。

##### 2、EvetnLoopThread类的成员函数：

* threadFunc函数：该函数作为子线程的线程函数，从一开始就被注入到Thread类的成员变量func\_中。该函数要创建EventLoop对象，执行它的loop方法。

* startLoop函数：该函数需要调用Thread的start函数从而创建子线程并运行上面的threadFunc函数。注意该函数的返回值：EventLoop类型的指针，该函数需要将创建好的EventLoop对象的指针上交给EventLoopThreadPool类进行统一管理。因此该函数需要进行进程同步：

  > 在主线程中使用条件变量让threadFunc函数先运行。
  >
  > 在子线程中把EventLoop创建出来，将EventLoop对象的指针保存在成员变量loop\_中，创建出来后notify主线程。
  >
  > 在主线程中条件变量满足后拿到成员变量loop\_的结果并返回。

#### 三、EventLoopThread类的设计技巧

