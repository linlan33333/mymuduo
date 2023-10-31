### 使用muduo库编写代码时的注意事项
***************
#### 一、程序的编译链接
要想编译链接使用了muduo库的程序，需要用```g++ XXX.cpp -lmuduo_net -lmuduo_base -lpthread -std=c++11```进行编译，换句话说，muduo库的使用需要libmuduo_base.so、libmuduo_net.so、libpthread.so这三个动态库。<font color=red>注意，lmuduo_net需要在lmuduo_base的前面，因为lmuduo_base也要用到lmuduo_net</font>