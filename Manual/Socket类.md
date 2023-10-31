## Socket类

#### 一、Socket类的作用

Socket类的作用是封装套接字的操作，不管是用于监听的listenfd还是用于连接的connfd都要被封装成这个类，因此这个类中的成员方法需要涵盖这两种套接字的相应操作。具体作用如下：

* 套接字创建：在构造函数中将传来的socketfd存到成员变量中，即封装socketfd成Socket对象。
* 套接字配置：提供一系列接口来配置套接字属性，比如端口复用SO_REUSEPORT、连接保活SO_KEEPALIVE等。
* 地址绑定：针对listenfd提供地址绑定接口，其实就是封装bind。
* 建立连接：针对listenfd提供连接建立接口，其实就是封装accept。
* 关闭连接：针对connfd提供断开连接接口，通过shutdown关闭写端的方式优雅地断开连接，相当于服务器主动提出TCP断开第一次分手，作用和好处自行查阅。

#### 二、Socket类的组成

Socket类的成员变量和成员函数构成简单：

成员变量只有一个：需要封装的socketfd

成员函数有多个：即上述提到的那些接口。

#### 三、Socket类的设计技巧

