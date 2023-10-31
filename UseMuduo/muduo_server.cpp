/*
muduo网络库给用户提供了两个主要的类
TcpServer：用于编写服务器端程序的
TcpClient：用于编写客户端程序的

采用epoll + 线程池的设计
好处：能够把网络I/O的代码和业务代码区分开，用户只要专注于写业务代码即可
*/

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <string>
#include <functional>
#include <iostream>

/*基于muduo网络库开发服务器程序步骤：
1、组合TcpServer对象
2、创建EventLoop事件循环对象的指针
3、明确TcpServer构造函数需要什么参数，输出ChatServer的构造函数
4、在当前服务器类的构造函数当中，注册处理连接的回调函数setConnectionCallback，以及处理读写事件的回调函数setMessageCallback
5、设置合适的服务端线程数量，muduo库会自己分配I/O线程和worker线程
6、定义start函数来开启事件循环，也就是启动服务器
*/
// 这是个回射服务器
class ChatServer
{
public:
    // #3 明确TcpServer构造函数需要什么参数，输出ChatServer的构造函数
    ChatServer(muduo::net::EventLoop* loop,     // 事件循环
                const muduo::net::InetAddress& listenAddr,      // IP + Port
                const std::string& nameArg)     // 服务器的名字
        : _server(loop, listenAddr, nameArg),
          _loop(loop)
    {
        // 给服务器注册用户连接的创建和断开回调
        std::function<void(const muduo::net::TcpConnectionPtr&)> connectionCallBackFunc = std::bind(&ChatServer::onConnection, this, std::placeholders::_1);
        _server.setConnectionCallback(connectionCallBackFunc);

        // 给服务器注册用户读写事件回调，这次直接用临时对象传给函数
        _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置服务器端的线程数量
        _server.setThreadNum(4);
    }

    // 开启事件循环
    void start() {
        _server.start();
    }

private:

    // 专门处理用户的连接创建和断开
    void onConnection(const muduo::net::TcpConnectionPtr& conn) {
        // peerAddress()方法用于获取连接方的地址信息
        
        if (conn->connected()) {        // connected()方法用于查看连接是否断开
            std::cout << conn->peerAddress().toIpPort() << " -> " << conn->localAddress().toIpPort() << " state:上线" << std::endl;
        }
        else {
            std::cout << conn->peerAddress().toIpPort() << " -> " << conn->localAddress().toIpPort() << " state:下线" << std::endl;
            conn->shutdown();       // 该连接断开后把它的文件描述符close掉，相当于close(fd)
            //_loop->quit();        这是关闭服务器
        }
    }

    // 专门处理用户的读写事件
    void onMessage(const muduo::net::TcpConnectionPtr& conn,        // 连接
                    muduo::net::Buffer* buf,        // 缓冲区，把用户传过来的数据拷到这里面去
                    muduo::Timestamp time)          // 记录接收到数据的时间信息
    {
        std::string recvBuf = buf->retrieveAllAsString();
        std::cout << "recv data: " << recvBuf << std::endl;
        std::cout << "time: " << time.toString() << std::endl;
        conn->send(recvBuf);
    }

    muduo::net::TcpServer _server;      // #1 组合TcpServer对象，这玩意的构造函数需要传入参数，因此在ChatServer构造函数中得先把_server构造出来
    muduo::net::EventLoop* _loop;       // #2 创建EventLoop事件循环对象的指针，这个EventLoop相当于epoll
};

int main() {
    muduo::net::EventLoop loop;     // 相当于创建一个epoll实例
    muduo::net::InetAddress addr("127.0.0.1", 8000);        // 定义好服务器IP和端口
    ChatServer server(&loop, addr, "回射服务器");

    server.start();     // 相当于将listenfd通过epoll_ctl添加到epoll实例中
    loop.loop();        // 相当于以阻塞的方式调用epoll_wait等待新用户连接，以及已连接用户的读写事件等发生

    return 0;
}