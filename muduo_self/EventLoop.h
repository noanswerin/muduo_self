#pragma once
#include"noncopyable.h"
#include<functional>
#include<vector>
#include<atomic>
#include"Timestamp.h"
#include<memory>
#include<mutex>
#include"CurrentThread.h"


class Channel;
class Poller;

//循环类，主要包含两个大模块，channel(事件)，Poller（epoll的抽象）（循环）
class EventLoop:noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();//开始事件循环
    void quit();//退出循环

    Timestamp pollReturnTime()const {return pollReturnTime_;}

    void runInLoop(Functor cb);//在当前loop中执行cb
    void queueInLoop(Functor cb);//把cb放入队列，唤醒loop所在的线程，执行cb

    void wakeup();//用来唤醒loop所在线程
    
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel* channel);

    //判断eventloop对象是否在自己线程中
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }


private:
    void handleRead();//wake up
    void doPendingFunctors();// 执行回调

    using ChannelList  = std::vector<Channel*>;

    std::atomic_bool looping_;//原子操作
    std::atomic_bool quit_;//退出loop循环

    const pid_t threadId_;//记录当前loop所在线程的id

    Timestamp pollReturnTime_;//poller返回发生事件的channel时间点 
    std::unique_ptr<Poller> poller_;

    int wakeupFd_;//当mainloop获取一个用户的channel，通过轮询选择一个subloop，通过该成员唤醒
    std::unique_ptr<Channel>wakeupChannel_;

    ChannelList activeChannels_;
    Channel *currentActiveChannel_;

    std::atomic_bool callingPendingFunctors_;//标识当前loop是否有需要执行的回调操作
    std::vector<Functor>pendingFunctors_;//loop所有的回调操作
    std::mutex mutex_;//互斥锁，用来保护上面vector容器的线程安全。

};