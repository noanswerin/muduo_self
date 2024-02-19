#include"TcpConnection.h"
#include"Logger.h"
#include"Socket.h"
#include"Channel.h"
#include"EventLoop.h"

#include<functional>
#include<errno.h>
#include<memory>
#include<sys/types.h>
#include<sys/socket.h>
#include<strings.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include<string>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}


TcpConnection::TcpConnection(EventLoop *loop,
                const std::string &nameArg,
                int sockfd,
                const InetAddress &localAddr,
                const InetAddress &peerAddr)
    :loop_(CheckLoopNotNull(loop))
    ,name_(nameArg)
    ,state_(kConnecting)
    ,reading_(true)
    ,socket_(new Socket(sockfd))
    ,channel_(new Channel(loop,sockfd))
    ,localAddr_(localAddr)
    ,peerAddr_(peerAddr)
    ,highWaterMark_(64*1024*1024)
{
    // 下面给channel设置相应的回调函数 poller给channel通知感兴趣的事件发生了
    //channel会回调相应的回调函数
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d\n", 
                name_.c_str(), channel_->fd(), (int)state_);
}

void TcpConnection::send(const std::string &buf)
{
    if(state_==kConnected)
    {
        if(loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(),buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,
                this,
                buf.c_str(),
                buf.size()
            ));
        }
    }
}

//发送数据 应用写的快，而内核发送数据慢，把待发送数据写入缓冲区，设置水位回调
void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0;
    ssize_t remaining = len ; 
    bool faultError = false;

    if(state_==kDisConnected)//shutdown has done
    {
        LOG_ERROR("disconnected,give up writing!");
        return;
    }
    //表示channel第一次开始写数据，而且缓冲区没有待发送数据
    if(!channel_->isWriting()&&outputBuffer_.readableBytes()==0)
    {
        nwrote = ::write(channel_->fd(),data,len);
        if(nwrote>=0)
        {
            remaining = len -nwrote;
            if(remaining==0&&writeCompleteCallback_)
            {
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_,shared_from_this())
                );
            }
        }
        else
        {
            nwrote = 0;
            if(errno!=EWOULDBLOCK)
            {
                LOG_ERROR("Tcpconnection ::sendInLoop");
                if(errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
    }

    //这一次write没有把数据发送出去。剩余需保存到缓冲区。然后给channel注册epollout事件
    //poller发现tcp的缓冲区有空间，会通知相应的sock-channel，调用writecallback方法
    //也就是调用handlewrite方法，把缓冲区中数据全部发送完成
    if(!faultError && remaining >0)
    {
       size_t oldlen = outputBuffer_.readableBytes();
       if(oldlen + remaining >= highWaterMark_ 
            && oldlen <highWaterMark_ 
            && highWaterMarkCallback_)
        {
            loop_ -> queueInLoop(
                std::bind(highWaterMarkCallback_,shared_from_this(),oldlen+remaining)
            );
        }
        outputBuffer_.append((char*)data + nwrote,remaining);
        if(!channel_->isWriting())
        {
            channel_ ->enableWriting();
        }
    }
}
//关闭连接
void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisConnecting);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop,this)
        );
    }
}

void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting()) // 说明当前outputbuffer数据已经全部发送完成
    {
        socket_ ->shutdownWrite();
    }
}

//连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();//向poller注册channel的epollin事件

    //新连接建立，执行回调
    connectionCallback_(shared_from_this());
}
//连接销毁
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisConnected);
        channel_->disableAll(); // 把channel的所有感兴趣的事件从poller中删除掉
        connectionCallback_(shared_from_this());
    }
    channel_->remove(); // 把channel从poller中删除掉
}


void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(),&savedErrno);
    if(n>0)
    {
        //已建立连接的用户，有可读事件发生。调用用户传入的回调操作
         messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);

    }
    else if(n == 0)
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if(channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(),&savedErrno);
        if(n>0)
        {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    // TcpConnection对象在其所在的subloop中 向pendingFunctors_中加入回调
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisConnecting)
                {
                    shutdownInLoop(); // 在当前所属的loop中把TcpConnection删除掉
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing", channel_->fd());
    }
}

void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d\n", channel_->fd(), (int)state_);
    setState(kDisConnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);   // 执行连接关闭的回调
    closeCallback_(connPtr);        // 执行关闭连接的回调 执行的是TcpServer::removeConnection回调方法   
                                    // must be the last line
}
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if(::getsockopt(channel_->fd(),SOL_SOCKET,SO_ERROR,&optval,&optlen)<0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
     LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d\n", name_.c_str(), err);
}


