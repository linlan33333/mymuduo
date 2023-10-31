#pragma once

#include <unistd.h>
#include <sys/syscall.h>
/**
 * 这个类用于获取当前的线程的tid
 * 这个类建议等到学完Linux内核之后再来看这个类干了啥玩意
*/

namespace CurrentThread {
    // 缓存当前线程的tid
    /**
     * __thread修饰词：__thread修饰词是一种线程局部存储的机制，它修饰的变量在每个线程内只有一份实例，各线程之间
     * 的值互不干扰，但如果采用static全局变量，值的变化所有线程都能修改都能看到，就没法保存线程需要保持独立的变量
    */
    extern __thread int t_cachedTid;

    void cacheTid();

    // 获取当前线程的tid
    inline int tid() {
        // 说明还没有获取当前线程的id
        if (__builtin_expect(t_cachedTid == 0, 0)) {
            // 获取一下
            cacheTid();
        }

        return t_cachedTid;
    }
}