## InetAddress类

#### 一、InetAddress类的作用

`InetAddress`类用于封装和操作网络地址信息（也就是sockaddr_in结构体），并提供了方便的接口来表示和操作这些信息。作用如下：

* IP地址和端口设置：该类提供接口设置好sockaddr_in结构体的端口信息与IP地址。
* IP地址与端口获取：该类提供接口来方便获取sockaddr_in结构体中的IP地址和端口信息。

#### 二、InetAddress类的组成成分

InetAddress类的组成成分简单，具体可看源码：

成员变量addr_为sockaddr_in结构体类型，用来保存网络地址信息

成员方法包含设置addr\_的接口，从addr\_中获取相关数据的接口（如IP地址、端口号），以及获取addr_变量的接口。