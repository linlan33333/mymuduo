## Timestamp类设计技巧

这个类的作用是获取当前时间，其核心函数如下

>```explicit Timestamp(int64_t microSecondsSinceEpoch);```
>
>``` static Timestamp now();```
>
>```std::string toString() const;```

now()函数是一个静态函数，返回值类型是Timestamp，目的是让该函数开箱即用，用完后返回一个临时对象，从而调用其他成员函数，非常快捷方便，不用特意创建一个对象出来。

其流程是：随便在哪调用now函数  $\longrightarrow$ 调用Timestamp有参构造函数  $\longrightarrow$ 返回临时对象  $\longrightarrow$ 方便继续调用其他成员函数如toString

示例：```Timestamp::now().toString()```即可获取当前时间，不用单独创建一个对象出来

**总结：**

* 如果某个类不需要在程序中特意创建一个对象出来，即这个类临时用一下就不再用了，那么可以设计一个静态成员函数，返回值是该类的临时对象，从而方便快捷地调用其他成员函数临时用一下功能