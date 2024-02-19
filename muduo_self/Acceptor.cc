#include"Acceptor.h"
#include"Logger.h"
#include"Socket.h"
#include"InetAddress.h"

#include<sys/types.h>
#include<sys/socket.h>
#include<errno.h>
#include<unistd.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET,SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,0);
    if(sockfd<0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n",__FILE__,__FUNCTION__,__LINE__,errno);
    }
    else
    {
        return sockfd;
    }
}

Acceptor::Acceptor(EventLoop *loop,const InetAddress &listenAddr,bool reuseport)
    :loop_(loop)
    ,acceptSocket_(createNonblocking())//socket
    ,acceptChannel_(loop,acceptSocket_.fd())
    ,listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);//bind
    //tcpserver::started() acceptor.listen 有新用户连接，执行回调。connfd-》channel-》subloop
    // baseloop =》acceptchannel_(listenfd)=>
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead,this));
}


Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

    
void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();

}

//listenfd有事件发生了，就是有新用户连接了
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd>= 0)
    {
        if(newConnectionCallback_)
        {
            newConnectionCallback_(connfd,peerAddr);//轮询找subloop 唤醒，分发当前新的客户端channel
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept create err:%d \n",__FILE__,__FUNCTION__,__LINE__,errno);
        if(errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit \n",__FILE__,__FUNCTION__,__LINE__);
        }
    }
}