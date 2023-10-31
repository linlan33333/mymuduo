#pragma once

#include <iostream>
#include <string>
#include <time.h>

/**
 * 时间类，作用是获取当前的系统时间，注意看它是如何实现开箱即用的
 * 这里面的几个成员函数是Timestamp类的核心函数
*/
class Timestamp
{
public:
    Timestamp();

    explicit Timestamp(int64_t microSecondsSinceEpoch);

    static Timestamp now();

    std::string toString() const;
private:
    int64_t microSecondsSinceEpoch_;
};