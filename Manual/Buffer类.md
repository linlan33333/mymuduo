## Buffer类

Buffer类的详细介绍可以阅读[陈硕的博客](https://www.cnblogs.com/Solstice/archive/2011/04/17/2018801.html)

#### 一、Buffer类的作用

`Buffer`类是一个用于管理数据缓冲区的类，主要的作用是管理connfd的读写缓冲区。<font color=red>对于非阻塞的文件、套接字来说，都需要设计Buffer缓冲区来存放数据</font>具体作用如下：

**核心功能**

* 数据缓冲：`Buffer`类用于管理一个字节序列的数据缓冲区。这个缓冲区通常用于存储从网络套接字读取或向套接字写入的数据，以便进行数据的传输和处理，也就是下面的数据收发功能。
* 数据收发：通过将connfd的缓冲区数据读取出来并写到Buffer中实现收取fd的网络数据；通过将待发送数据从Buffer中读取出来并写到connfd的缓冲区中实现发送网络数据给fd。

**基础功能**

* 读写操作：`Buffer`类提供了读取和写入数据的方法。可以从缓冲区中读取数据，也可以将数据写入缓冲区。方便网络数据的解析与生成。
* 容量管理：`Buffer`可以动态地扩展和收缩缓冲区的容量，以适应不同大小的数据（其实底层用的vector，让它帮忙管理容量），同时`Buffer`支持数据移动，有的时候前面的prepair区域很大，可读区域其实位置很靠后，因此可以将空余空间挪动位置减少缓存碎片产生，从而更高效地利用容量。

#### 二、Buffer类的设计结构

Buffer类的缓冲区结构由三部分组成

<img src="E:/桌面工作文件/重要资料/C++项目实践/手写muduo网络库项目梳理/image/Buffer_struct.png" />

* prependable区域：方便在数据前面添加几个字节，比如说把数据A写到Buffer后还需要在前面加上1字节的数据表示数据A的长度，方便对方知道数据A多长才算完整，就可以把长度信息添加在该区域中，最后发送时再合并。

  <font color=red>本项目其实并没有用到prependable区域，只用了readable和writable区域</font>

* readable区域：读缓冲区，客户端发过来的数据都存在这里慢慢处理。

* writable区域：写缓冲区，里面的数据都需要发给客户端，vector容器初始大小为1K。

#### 三、Buffer类的组成成分

##### 1、Buffer类的成员变量

Buffer类的成员变量就是维护了一个```vector<char>```容器进行存储数据，以及readerIndex\_变量表示读数据的起始下标，writerIndex\_变量表示写数据的起始下标。

##### 2、Buffer类的成员函数

成员函数按功能分为两类：

**基础功能**

* 能立刻推导出图上各区域的长度的函数readableBytes、writableBytes、prependableBytes

* 能立刻给出读、写操作的起始位置的函数peek、beginWrite

* 读取出数据并返回

  > retrieveAllAsString函数：将可读区域的数据**全部**读取。先复制到字符串中并返回，再调用retrieve函数将缓冲区恢复成默认样子。
  >
  > retrieveAsString函数：**部分**读取可读区域中的数据。将可读区域的部分数据读取到字符串中并返回，同时调用retrieve函数更新一下下次读取数据的位置。
  >
  > retrieve函数：更新下次读取数据的位置，更新方式要看是上面哪个函数调用的它，如果是retrieveAllAsString，那么该函数会调用retrieveAll去把缓冲区恢复成默认样子。如果是retrieveAsString，那么该函数就移动readerIndex\_下标。
  >
  > retrieveAll函数：把缓冲区恢复成默认样子，当可读区域数据全部读完了才会调用它。

* 写入数据到缓冲区

  >append函数：将数据拷贝到writable区域。写之前会先调用ensureWritableBytes函数检查并执行扩容操作，然后再拷贝数据，最后再更新writerIndex\_下标，扩容操作是为核心功能中的分散写操作准备的，如果没有分散写操作这里扩容就不会触发，原因下面会说。
  >
  >ensureWritableBytes函数：由于用户每次读取一小部分数据后readerIndex\_会越来越靠后，导致prependable区域越来越大，但其实prependable区域只需要固定长度（kCheapPrepend大小）即可，就得判断多余的prependable区域和writable区域加起来够不够用。如果够用，就挪一下地方再存数据，如果还不够，不需要移动数据直接就扩容。

##### 核心功能

* 将connfd缓冲区数据直接写到Buffer中

  > 使用分散写的方式，先在栈上申请一块64K的栈内存，使用readv优先将数据读到栈内存上（如果可写区域writable比栈内存小），再将栈内存里的数据写到writable中，该扩容就扩容。
  >
  > **为什么要设计临时栈内存来倒腾一次数据？可不可以不使用栈内存？**
  >
  > **答：**不可以，假设不使用栈内存，只用writable区域存数据，那么使用read函数时，如果来了超过1K的数据，那么read函数一次性写不完数据writable区域中，返回值也是写了多少数据到writable区域中，而不是来了多少数据，导致没法触发扩容机制，也就是说Buffer缓冲区压根不知道writable区域空间不够放。那么由于LT机制，没读完的数据下次再通知去读，就得反反复复调用read函数，而反复调用系统调用很影响服务器性能的，因此就设计了足够大的临时栈内存，可以先将数据读到栈内存上，确保readv能把数据一次性读完（尽量），然后再将栈内存上的数据写到writable区域中，该扩容就扩容。这样就避免了多次调用readv导致服务器性能损耗。

* 将Buffer中的数据写到connfd中

  > 直接调用write系统调用函数写就完事了

#### 四、Buffer类的设计技巧

##### 1、使用大容量的栈内存先存储connfd的数据

这一点在核心功能讲解中已经提过了，是Muduo库的一个创新点。

##### 2、使用prependable区域方便在数据前面添加额外信息

陈硕的博客中提到，该空间可以让程序能以很低的代价在数据**前面**添加几个字节。

例如：程序以固定的4个字节表示消息的长度（即《[Muduo 网络编程示例之二：Boost.Asio 的聊天服务器](http://blog.csdn.net/Solstice/archive/2011/02/04/6172391.aspx)》中的 LengthHeaderCodec），如果要序列化一个消息，但是不知道它有多长，那么可以一直 append() 直到序列化完成，然后再在序列化数据的前面添加消息的长度，把 200 这个数 prepend 到首部。
