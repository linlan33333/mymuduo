#include "Timestamp.h"

Timestamp::Timestamp() : microSecondsSinceEpoch_(0)
{

}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch) : microSecondsSinceEpoch_(microSecondsSinceEpoch)
{

}

// 这是一个静态方法，开箱即用
// 返回一个Timestamp临时对象方便调用其他函数比如now函数
Timestamp Timestamp::now() 
{
    return Timestamp(time(NULL));       // 调用有参构造函数返回临时对象
}

std::string Timestamp::toString() const
{
    char buf[128] = {0};    // c风格字符串
    tm* tm_time = localtime(&microSecondsSinceEpoch_);
    snprintf(buf, 128, "%4d/%02d/%02d %02d:%02d:%02d", 
        tm_time->tm_year + 1900, tm_time->tm_mon + 1, tm_time->tm_mday,
        tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec);

    return buf;
}

// #include <iostream>
// 测试这个类的代码有没有问题
// int main() {
//     std::cout << Timestamp::now().toString() << std::endl;

//     return 0;
// }
