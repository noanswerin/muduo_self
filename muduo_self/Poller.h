#pragma once
#include"noncopyable.h"
#include<unordered_map>
#include<vector>
#include"Timestamp.h"
class EventLoop;
class Channel;
//muduo库中多路事件分发器的核心IO复用模块
class Poller:noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop *Loop);
    virtual ~Poller();

    //给所有Io复用保留统一的接口
    virtual Timestamp poll(int timeoutMs,ChannelList *activeChannels) = 0; 
    virtual void updateChannel(Channel *channel)=0;
    virtual void removeChannel(Channel *channel)=0; 
    
    //判断参数channel是否在当前Poller中
    bool hasChannel(Channel *channel)const;

    //EventLoop可以通过该接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop *Loop);


protected:
    using ChannelMap = std::unordered_map<int,Channel*>;//int->sockfd
    ChannelMap channels_;
private:
    EventLoop *ownerLoop_;
};