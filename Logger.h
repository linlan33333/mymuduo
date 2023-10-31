#pragma once

#include <string>
#include <iostream>
#include <cstdlib>

#include "noncopyable.h"
#include "Timestamp.h"

// LOG_INFO("%s %d", arg1, arg2)
// 为了防止宏出现错误，一般都是采用do while(0)结构，而且末尾不要打分号，原因可以查阅相关资料
// 注意斜杠后面不能有空格，不然就不是一行
#define LOG_INFO(logmsgFormat, ...) \
    do \
    {  \
        Logger &logger = Logger::instance();   \
        logger.setLogLevel(LogLevel::DEBUG);   \
        char buf[1024] = {0};   \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);   \
        logger.log(buf);    \
    } while(0)

#define LOG_ERROR(logmsgFormat, ...) \
    do \
    {  \
        Logger &logger = Logger::instance();   \
        logger.setLogLevel(LogLevel::ERROR);   \
        char buf[1024] = {0};   \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);   \
        logger.log(buf);    \
    } while(0)

#define LOG_FATAL(logmsgFormat, ...) \
    do \
    {  \
        Logger &logger = Logger::instance();   \
        logger.setLogLevel(LogLevel::FATAL);   \
        char buf[1024] = {0};   \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);   \
        logger.log(buf);    \
        exit(-1);   \
    } while(0)

// 这么写的原因在于LOG_DEBUG输出的信息长，次数多，因此会有大量的IO操作影响服务器效率，因此默认不开启
#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...) \
    do \
    {  \
        Logger &logger = Logger::instance();   \
        logger.setLogLevel(LogLevel::DEBUG);   \
        char buf[1024] = {0};   \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);   \
        logger.log(buf);    \
    } while(0)            
#else
    #define LOG_DEBUG(logmsgFormat, ...)
#endif
// 定义日志的级别
// 比如INFO：打印重要的流程信息，ERROR：打印不影响软件正常运行的错误信息，FATAL：致命的系统错误，会使程序异常终止的那种
// DEBUG：打印调试信息
enum LogLevel {
    INFO,       // 普通信息
    ERROR,      // 错误信息
    FATAL,      // core信息
    DEBUG       // 调试信息
};

// 输出一个日志类
class Logger : noncopyable
{
public:
    // 获取日志唯一的实例对象
    static Logger& instance();

    // 设置日志级别
    void setLogLevel(int level);

    // 写日志的接口
    void log(std::string msg);
private:
    Logger() {}     // 把构造函数扔到private里面实现单例模式

    int logLevel_;
};