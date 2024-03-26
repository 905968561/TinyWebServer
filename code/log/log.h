#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           // vastart va_end
#include <assert.h>
#include <sys/stat.h>         // mkdir
#include "blockqueue.h"
#include "../buffer/buffer.h"

class Log{
public:
    void init(int level,const char * path="./log",
                const char * suffix=".log",
                int maxQueueCapacity=1024);

    static Log* Instance();
    static void FlushLogThread(); //异步写日志的公有方法

    void write(int level,const char * format,...);
    void flush();

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen(){
        return isOpen_;
    }

private:
    Log();
    void AppendLogLevelTitle_(int level);
    virtual ~Log();
    void AsyncWrite_(); //异步写日志方法

private:
    static const int LOG_PATH_LEN =256; // 日志文件名字的长度
    static const int LOG_NAME_LEN =256; // 日志最长的名字
    static const int MAX_LINES =50000; // 日志文件中最长的日志条目数

    const char * path_; //存储log的路径名
    const char * suffix_; // 后缀的名字

    int MAX_LINES_; //最大日志行数

    int lineCount_; //日志行数记录
    int toDay_; //当天的日期

    bool isOpen_;

    Buffer buff_; //缓冲区
    int level_; //日志等级
    bool isAsync_; //是否开启异步日志

    FILE* fp_;
    unique_ptr<BlockQueue<std::string>> deque_; //异步写日志的阻塞队列
    unique_ptr<std::thread> writeThread_; //写线程的指针
    mutex mtx_; //互斥量

};

#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

// 四个宏定义，主要用于不同类型的日志输出，也是外部使用日志的接口
// ...表示可变参数，__VA_ARGS__就是将...的值复制到这里
// 前面加上##的作用是：当可变参数的个数为0时，这里的##可以把把前面多余的","去掉,否则会编译出错。
#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);    
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif