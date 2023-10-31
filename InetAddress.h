#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>
#include <string>

// 该类将设置IP和端口的相应方法封装起来
class InetAddress 
{
public:
    // 注意：sockaddr_in成员中有个sin_port，类型是in_port_t，本质是uint16_t，这里的port就是它
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");

    explicit InetAddress(const sockaddr_in& addr) : addr_(addr) {}

    // 获取IP地址
    std::string toIp() const;

    // IP地址和端口号一起获取
    std::string toIpPort() const;

    // 单单获取端口号
    uint16_t toPort() const;

    const sockaddr_in* getSockAddr() const { return &addr_; }
    void setSockAddr(const sockaddr_in& addr) { addr_ = addr; }

private:
    sockaddr_in addr_;
};
