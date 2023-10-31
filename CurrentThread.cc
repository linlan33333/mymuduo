#include "CurrentThread.h"

namespace CurrentThread {
    // 用来保存当前线程的pid的值，为什么不在用的时候直接查？因为系统调用需要切换用户态和内核态，
    // 性能消耗太大，建议查一次后缓存起来
    __thread int t_cachedTid = 0;

    void cacheTid() {
        if (t_cachedTid == 0) {
            // 通过Linux系统调用获取当前线程的pid值
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_getpid));
        }
    }
}