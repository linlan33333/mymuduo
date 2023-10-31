#include "InetAddress.h"

#include <strings.h>

InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    // 记得IP地址要转成网络字节序
    inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr.s_addr);
}

std::string InetAddress::toIp() const
{
    char buf[16];
    // IP地址要转成字符串
    inet_ntop(AF_INET, &addr_.sin_addr.s_addr, buf, sizeof(buf));
    return buf;
}

std::string InetAddress::toIpPort() const
{
    // std::string ip = toIp();
    // std::string port = std::to_string(toPort());
    // return ip + ':' + port;

    // 学学人家是如何将字符串和数字拼接到一起打印的
    char buf[64] = {0};
    inet_ntop(AF_INET, &addr_.sin_addr.s_addr, buf, sizeof(buf));
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    // sprintf的作用是将一个格式化的字符串输出到一个目的字符串中，而printf是将一个格式化的字符串输出到屏幕
    // 这里是将端口号输出到buf字符串后面
    sprintf(buf + end, ":%u", port);  
    return buf;
}

uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}

// 测试
// #include <iostream>
// int main() {
//     InetAddress addr(8000);
//     std::cout << addr.toIpPort() << std::endl;

//     return 0;
// }
