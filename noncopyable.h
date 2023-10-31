#pragma once

/**
 * noncopyable被继承后，派生类对象可以正常的构造和析构，但是派生类对象无法进行拷贝构造和拷贝赋值操作。
 * 原因在于子类的拷贝需要先调用父类的拷贝构造函数和拷贝赋值函数将父类中的成员拷贝过来，
 * 再调用子类中的拷贝构造和拷贝赋值函数将子类中特有的成员拷贝过来
*/
class noncopyable 
{
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
protected:      // protected中的成员子类可以访问外部类不可访问
    noncopyable() = default;
    ~noncopyable() = default;
};