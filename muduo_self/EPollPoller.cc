#include"EPollPoller.h"
#include"Logger.h"
#include<errno.h>
#include <unistd.h>
#include"Channel.h"
#include<strings.h>

//未添加
const int kNew = -1;//channel->index:-1
//channel 已经添加到poller
const int kAdded = 1;
const int kDeleted = 2;


//                           EventLoop
//      ChannelList（事件表）         Poller
//                                   ChannelMap<fd,channel*>  向poller注册的事件。




EPollPoller::EPollPoller(EventLoop *loop)
    :Poller(loop)
    ,epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    ,events_(KInitEventListSize)
{
    if(epollfd_<0) LOG_FATAL("epoll_create error:%d \n",errno);
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels) 
{
    LOG_INFO("func:%s => fd total count:%lu\n",__FUNCTION__,channels_.size());

    int numEvents = ::epoll_wait(epollfd_,&*events_.begin(),static_cast<int>(events_.size()),timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if(numEvents > 0)
    {
        LOG_INFO("%d events happened\n",numEvents);
        fillActiveChannels(numEvents,activeChannels);
        if(numEvents == events_.size())
        {
            events_.resize(events_.size()*2);//扩容
        }
    }
    else if(numEvents == 0)
    {
        LOG_DEBUG("%S timeout!\n",__FUNCTION__);
    }
    else
    {
        if(saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EpollPoller::poll()err!");
        }
    }
    return now;
} 


void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("fuc=%s,fd=%d events=%d index=%d\n",__FUNCTION__,channel->fd(),channel->events(),index);
    if(index == kNew||index == kDeleted)
    {
        if(index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD,channel);
    }
    else
    {
        int fd = channel->fd();
        if(channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL,channel);
            channel->set_index(kDeleted);

        }
        else{
            update(EPOLL_CTL_MOD,channel);
        }
    }
}

//从poller删除channel
void EPollPoller::removeChannel(Channel *channel) 
{
    int fd = channel->fd();
    channels_.erase(fd);
    
    LOG_INFO("fuc=%s=>fd=%d ",__FUNCTION__,fd);
    int index= channel->index();
    if(index = kAdded)
    {
        update(EPOLL_CTL_DEL,channel);
    }
    channel->set_index(kNew);
}

//填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents,ChannelList *activeChannels) const
{
    for(int i=0;i < numEvents;++i)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);//EventLoop就拿到它的poller给他返回的所有发生事件的channel列表了
    }
}



//更新channel通道 epoll_ctl,epoll_del
void EPollPoller::update(int operation,Channel *channel)
{
    epoll_event event;
    bzero(&event,sizeof event);
    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd=fd;
    event.data.ptr = channel;
    //找到fd，找到channel


    if(::epoll_ctl(epollfd_,operation,fd,&event)<0)
    {
        if(operation==EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n",errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n",errno);
        }
    }
}
