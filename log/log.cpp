#include "log.h"

//构造函数，先构造
Log::Log(){
    fp_=nullptr;
    deque_=nullptr;
    writeThread_=nullptr;
    lineCount_=0;
    toDay_=0;
    isOpen_=false;
}

Log::~Log(){
    while(!deque_->empty()){
        deque_->flush(); //如果阻塞队列不为空，那么唤醒队列的消费者，直到为空
    }
    deque_->Close();
    writeThread_->join(); //等待当前线程完成任务
    if(fp_){
        lock_guard<mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

void Log::flush(){
    if(isAsync_){ //异步才会用到deque
        deque_->flush();
    }
    fflush(fp_); //清空文件输入缓冲区
} 

Log* Log::Instance(){
    static Log log;//局部静态变量，懒汉模式又能线程安全
    return &log;
}

void Log::FlushLogThread(){
    Log::Instance->AsyncWrite_(); //异步写日志的方法
}

//写线程的真正执行函数
void Log::AsyncWrite_(){
    String str ="";
    while(deque_->pop(str)){
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(),fp_);
    }
}

void Log::init(int level,const char* path, const char* suffix, int maxQueCapacity){
    isOpen_=true;
    level_=level;
    path_=path;
    suffix_=suffix;
    if(maxQueCapacity){//如果有阻塞队列，就异步写入缓冲区
        isAsync_=true;
        if(!deque_){//阻塞队列不存在就初始化一个
            unique_ptr<BlockQueue<string>> newQue(new BlockQueue<string>);
            deque_=move(newQue);//move移交控制权
            unique_ptr<std::thread> newThread(new thread(FlushLogThread));//新建线程的时候就绑定方法了
            writeThread_=move(newThread);
        }
    }else{
        isAsync_=false;
    }

    lineCount_=0;
    time_t timer=time(nullptr);
    struct tm* systime=localtime(&timer);//也是获取当前时间
    char fileName[LOG_NAME_LEN]={0};
    snprintf(fileName,LOG_NAME_LEN-1,"%s/%04d_%02d_%02d%s",
            path_,systime->tm_year+1900,systime->tm_mon + 1, systime->tm_mday, suffix_);
    toDay_=systime->tm_mday;

    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll(); //buffer清空
        if(fp_){// fp_清空
            flush();
            fclose();
        }
        fp_=fopen(fileName,"a");
        if(fp_==nullptr){
            mkdir(fileName,0777); //不存在就生成
            fp_=fopen(fileName,"a");
        }
        assert(fp_!=nullptr);
    }//初始化完毕，阻塞队列初始化，写线程初始化，buffer清空，fp打开对应文件并正确命名
}

void Log::write(int level,const char * format,...){
    struct timeval now={0,0};
    gettimeofday(&now,nullptr);
    time_t tSec=now.tv_sec;
    struct tm* systime=localtime(&tSec);
    struct tm t=*systime; //主要就是获取当前时间的一系列操作
    va_list vaList;

    //如果日期不对 或者 行数超了 就得新建文件了
    if(toDay_!=t.tm_mday || (lineCount_ &&(lineCount_%MAX_LINES==0))){
        unique_lock<mutex> locker(mtx_);
        locker.unlock();

        char newFile[LOG_NAME_LEN];
        char tail[36]={0}; //tail用来记录日期的
        snprintf(tail,36,"%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if(toDay_!= t.tm_mday){//日期不对就新建新日期的文件名
            snprintf(newFile,LOG_NAME_LEN-72,"%s/%s%s", path_, tail, suffix_);
            toDay_=t.tm_mday;
            lineCount_=0;
        }else{//日期对了就添加当天日志的附件1，2，3，4等等
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
        }

        locker.lock();
        flush(); //清空文件缓冲区和队列的缓冲区
        fclose(fp_);
        fp_=fopen(newFile,'a');//重新打开目标文件
        assert(fp!=nullptr);
    }

    //向buffer当中写日志，等异步写线程来写，或者是直接写进文件
    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        int n=snprintf(buff_.BeginWrite(),128,"%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);//先把日志的日期格式输入buffer,并返回输入字符的个数
        buff_.HasWritten(n);
        AppendLogLevelTitle_(level);//然后开始输入日志的level

        //紧接着日志本体信息
        va_start(vaList,format);
        int m=vsnprintf(buff_.BeginWrite(),buff_.WritableBytes(),format,vaList);//同上，但这里用了一个valist来输入信息
        va_end(vaList);

        buff_.HasWritten(m);
        buff_.Append("\n\0",2); //添加末尾

        if(isAsync_ && deque_ && !deque_->full()){//异步方式（把缓冲区数据一次督导阻塞队列，等写线程写）
            deque_->push_back(buff_.RetrieveAllToStr);
        }else{//同步
            fputs(buff_.Peek(),fp_); //直到读到字符结束符\0
        }
        buff_.RetrieveAll();//刷新缓冲区
    }
}


void Log::AppendLogLevelTitle_(int level){
    switch (level)
    {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info] : ", 9);
        break;
    case 2:
        buff_.Append("[warn] : ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info] : ", 9);
        break;
    }
}

int Log::GetLevel(){
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level){
    lock_guard<mutex> locker(mtx_);
    level_=level;
}