#pragma once
#include"noncopyable.h"
#include<string>
//定义日志级别 INFO ERROR FATAL DEBUG
#define LOG_INFO(LogmsgFormat,...)\
    do\
    {\
        Logger &logger =Logger::instance();\
        logger.setLogLevel(INFO);\
        char buf[1024] = {0};\
        snprintf(buf,1024,LogmsgFormat,##__VA_ARGS__);\
        logger.log(buf);\
     } while(0)
#define LOG_ERROR(LogmsgFormat,...)\
    do\
    {\
        Logger &logger =Logger::instance();\
        logger.setLogLevel(ERROR);\
        char buf[1024] = {0};\
        snprintf(buf,1024,LogmsgFormat,##__VA_ARGS__);\
        logger.log(buf);\
     } while(0)
#define LOG_FATAL(LogmsgFormat,...)\
    do\
    {\
        Logger &logger =Logger::instance();\
        logger.setLogLevel(FATAL);\
        char buf[1024] = {0};\
        snprintf(buf,1024,LogmsgFormat,##__VA_ARGS__);\
        logger.log(buf);\
        exit(-1); \
    } while(0)

#ifdef MUDEBUG
#define LOG_DEBUG(LogmsgFormat,...)\
    do\
    {\
        Logger &Logger =Logger::instance();\
        logger.setLogLevel(DEBUG);\
        char buf[1024] = {0};\
        snprintf(buf,1024,LogmsgFormat,##__VA_ARGS__);\
        logger.log(buf);\
    } while(0)
#else
    #define LOG_DEBUG(LogmsgFormat,...)
#endif

enum LogLevel
{
    INFO,
    ERROR,
    FATAL,
    DEBUG,
};

//输出日志类
class Logger
{
public:
    static Logger& instance();//获取日志唯一对象实例
    void setLogLevel(int Level);//设置日志级别
    void log(std::string msg);
private:
    int logLevel_;
    Logger(){}
};