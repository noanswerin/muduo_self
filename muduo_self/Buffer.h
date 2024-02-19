#pragma once
#include<vector>
#include<string>
#include<algorithm>

//网络库底层缓冲区类型定义
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t KInitialSize = 1024;

    explicit Buffer(size_t initialSize = KInitialSize)
        :buffer_(kCheapPrepend+ initialSize)
        ,readerIndex_(kCheapPrepend)
        ,writerIndex_(kCheapPrepend)
    {}

    size_t readableBytes() const{return writerIndex_ - readerIndex_;}
    size_t writableBytes() const {return buffer_.size()- writerIndex_;}
    size_t prependableBytes() const {return readerIndex_;}
    
    //返回缓冲区中可读数据的起始地址
    const char* peek() const
    {
        return begin()+readerIndex_;
    }

    void retrieve(size_t len)//onMessage
    {
        if(len<readableBytes())
        {
            readerIndex_ +=len;//读取部分数据后，还有readerIndex_ +=len这个位置到writerIndex未读

        }
        else
        {
            retrieveAll();
        }
    } 

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ =kCheapPrepend;//复位
    }   

    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());//应用可读取数据的长度
    }

    std::string  retrieveAsString(size_t len)
    {
        std::string result(peek(),len);
        retrieve(len);//上把缓冲区数据读取出来，对缓冲区进行复位操作
        return result;
    }

    void ensureWriteableBytes(size_t len)
    {
        if(writableBytes()<len)
        {
            makeSpace(len);
        }
    }
    //把data data+len 内存上数据添加。
    void append(const char *data,size_t len)
    {
        ensureWriteableBytes(len);
        std::copy(data,data+len,beginWrite());
        writerIndex_ +=len;
    }

    char* beginWrite() {return begin()+writerIndex_;}
    const char* beginWrite() const {return begin()+writerIndex_;}

    //从fd上读取数据
    ssize_t readFd(int fd,int* saveErrno);
    //通过fd发送数据
    ssize_t writeFd(int fd,int* saveErrno);
private:
    char* begin()
    {
        return &*buffer_.begin();
    } 

    const char* begin() const
    {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len)
    {
        if(writableBytes()+prependableBytes()<len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_+len);
        }
        else
        {
           size_t readable = readableBytes();
           std::copy(begin() + readerIndex_,begin() + writerIndex_,  
                    begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;

};