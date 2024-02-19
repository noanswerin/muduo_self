#include"Buffer.h"
#include<errno.h>
#include <unistd.h>

#include<sys/uio.h>


//从fd读取数据 poller工作在LT模式
//Buffer缓冲区有大小，但读数据时，不知道TCP数据最终的大小
ssize_t Buffer::readFd(int fd,int* saveErrno)
{
    char extrabuf[65536] = {0};//64k
    
    struct iovec vec[2];
    
    const size_t writable = writableBytes();//这是buffer底层缓冲区剩余空间大小
    //第一块缓冲区
    vec[0].iov_base = begin()+writerIndex_;
    vec[0].iov_len = writable;
    //第二块缓冲区
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2:1;
    const ssize_t n = ::readv(fd,vec,iovcnt);
    if(n<0)
    {
        *saveErrno = errno;
    }
    else if(n <= writable)
    {
        writerIndex_ +=n;
    }
    else
    {
        writerIndex_ = buffer_.size();//满了
        append(extrabuf,n-writable);
    }

    return n;

}

ssize_t Buffer::writeFd(int fd,int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}