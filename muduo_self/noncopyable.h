#pragma once
/**
 *noncopyable被继承以后，派生类对象可以正常进行构造和析构，但是派生类对象
 *无法进行拷贝工作和赋值。
*/

class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&)= delete;
protected:
    noncopyable()=default;
    ~noncopyable() = default;

};  