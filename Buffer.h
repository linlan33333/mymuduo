#pragma once

#include <vector>
#include <string>
#include <algorithm>

/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode
/**
 * 网络库底层的缓冲区类型，这张图解释了buffer的样子，一个buffer缓冲区分成三部分
 * 用于防止Tcp粘包问题，要写的数据太大socket文件缓冲区装不下问题等等
 * 
 * 以后的项目如果需要使用缓冲区，那么都可以借鉴这个Buffer类的设计
*/
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;      // 这是前面prependable bytes部分的长度
    static const size_t kInitialSize = 1024;    // 这是后面readable + writable部分的缓冲区的长度

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + kInitialSize)         // 初始化buffer_的长度
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {
    }

    // 可读部分的缓冲区长度，就是readable部分的大小
    size_t readableBytes() const {
        return writerIndex_ - readerIndex_;
    }

    // 可写部分的缓冲区长度
    size_t writableBytes() const {
        return buffer_.size() - readerIndex_;
    }

    // prependable部分的大小
    size_t prependableBytes() const {
        return readerIndex_;
    }

    // 返回可以读的地方的起始地址
    char* peek() {
        return begin() + readerIndex_;
    }

    // 返回缓冲区中可读数据的起始地址
    const char* peek() const {
        return begin() + readerIndex_;
    }

    char* beginWrite() {
        return begin() + writerIndex_;
    }

    // 返回可以写的地方的起始地址
    const char* beginWrite() const {
        return begin() + writerIndex_;
    }

    // 更新一下下次读取的起始位置
    void retrieve(size_t len) {
        // 只读了一部分，缓冲区还有数据没读完，一般retrieveAsString函数会走这
        if (len < readableBytes()) {
            // 更新下一次读取的起始位置
            readerIndex_ += len;
        }
        // 全部读取，一般retrieveAllAsString函数会走这
        else {
            // 缓冲区全读完了，就把缓冲区复位
            retrieveAll();
        }
    }

    // 数据读完了，就得把缓冲区恢复成默认的样子
    void retrieveAll() {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    // 把onMessage函数上报的Buffer数据，转成string类型的数据返回
    std::string retrieveAllAsString() {
        // 引用可读取长度的数据
        return retrieveAsString(readableBytes());
    }

    std::string retrieveAsString(size_t len) {
        std::string result(peek(), len);
        retrieve(len);      // 上面一句把缓冲区中可读的数据已经读取出来，这里肯定要最缓冲区进行复位操作
        return result;
    }

    // 扩容函数
    void ensureWritableBytes(size_t len) {
        if (writableBytes() < len) {
            makeSpace(len);
        }
    }

    // 把[data, data + len]区间上的数据写到writable缓冲区中
    void append(const char* data, size_t len) {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    // 从fd的缓冲区将数据直接读到buffer中，因此该函数封装在buffer中
    ssize_t readFd(int fd, int* saveErrno);

    // 通过fd发送数据，原创函数，Muduo库中没有这个函数
    ssize_t writeFd(int fd, int* saveErrno);

private:
    // 返回vector底层数组的起始地址，这么做是因为一些系统调用要传数组的指针作为参数
    char* begin() { return buffer_.data(); }
    const char* begin() const { return buffer_.data(); }

    // 扩容函数
    void makeSpace(size_t len) {
        // 扩容之前先看空余下来的空间够不够用
        // 如果用户读了一部分readable区域的数据，那么readable就有一部分空间空闲，和之前的prependable空间和一起正好大小是readerIndex_
        // 其实就是读着读着readerIndex_一直往后移，导致prependable空间越来越大，prependable空间不需要那么大，就需要挪动一下数据重新利用
        // 此时看空闲空间writableBytes() + prependableBytes() - kCheapPrepend够不够用，不够用再扩容
        if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
            buffer_.resize(writerIndex_ + len);
        }
        else {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                    begin() + writerIndex_,
                    begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    // 底层让vector直接管理内存，方便省事
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};