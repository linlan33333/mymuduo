#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/**
 * 从fd上读取数据， Poller工作在LT模式，因此数据肯定不会丢失
 * Buffer缓冲区是由大小的，但是tcp是流式数据，不清楚最终数据会传多大
 * 这里就需要使用分散写和集中读的方法
*/
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char extrabuf[65536] = {0};     // 这是栈上的内存空间,buffer读缓冲区不够就分散写写到这里面来
                                    // 而且函数结束该内存自动回收

    struct iovec vec[2];

    const size_t writable = writableBytes();    // 这是Buffer底层缓冲区剩余的可写空间大小 
    vec[0].iov_base = begin() + writerIndex_;   // 这一块写到buffer缓冲区中
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;                 // 这一块写到上面的extrabuf中
    vec[1].iov_len = sizeof extrabuf;

    // 如果可写的区域小于extrabuf（也就是64K），就优先将数据写到extrabuf中
    // 这么做的原因：如果写到数据很大，且没有临时栈内存而是选择直接写到底层vector容器中的话，因为vector容器空间只有1k大小，每次最多写1k数据进去
    // 注意扩容机制此时触发不了，因为readv只有1k的空间可以写，当写满1k后返回值n也是1024，下面的根据n来扩容就无法触发，
    // 只会导致readv可能还有数据没写完，等待LT水平触发下次再写。如果readv要写的数据非常大，就需要反复调用readv写到vector中
    // 这样反复调用系统调用，对服务器性能影响非常大。
    // 因此，有了临时栈内存后，优先把数据写到栈内存，这样当来的数据两较大时就可以少调用几次readv，争取把数据一次性全部读进来，
    // 然后再看写缓冲区writable够不够把栈内存里的数据倒过去，不够就扩容，把数据全部放进去
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    // 分散读，返回读取字节数
    const ssize_t n = ::readv(fd, vec, iovcnt);     
    if (n < 0) {
        *saveErrno = errno;
    }
    else if (n <= writable) {
        writerIndex_ += n;
    }
    else {  // writable bytes部分放不下，导致extrabuf里面也写了数据
        writerIndex_ = buffer_.size();
        // 扩容写，vector会自动扩容，让后面添数据就行
        append(extrabuf, n - writable);     // 从writerIndex_开始写， 写n - writable大小的数据
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0) {
        *saveErrno = errno;
    }

    return n;
}
