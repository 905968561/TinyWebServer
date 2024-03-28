#include "webserver.h"

using namespace std;

//设置服务器参数　＋　初始化定时器／线程池／反应堆／连接队列
WebServer::WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger,
        int sqlPort, const char* sqlUser, const  char* sqlPwd,
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize):
        port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
        timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller()){
    srcDir_= getcwd(nullptr,256);
    assert(srcDir_);
    strcat(srcDir_, "/resources/");
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;

    //初始化数据库连接
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);  // 连接池单例的初始化
    //初始化事件处理模式和socket
    InitEventMode_(trigMode);
    if (!InitSocket_()){isClose_=true;}

    //打开日志
    if(openLog) {
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                     (listenEvent_ & EPOLLET ? "ET": "LT"),
                     (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ =true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}


//这里按位与操作可以根据不同位，来集合不同的状态
void WebServer::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;    // 检测socket关闭
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;     // EPOLLONESHOT由一个线程处理
    switch (trigMode)
    {
        case 0:
            break;
        case 1:
            connEvent_ |= EPOLLET;
            break;
        case 2:
            listenEvent_ |= EPOLLET;
            break;
        case 3:
            listenEvent_ |= EPOLLET;
            connEvent_ |= EPOLLET;
            break;
        default:
            listenEvent_ |= EPOLLET;
            connEvent_ |= EPOLLET;
            break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET); //边缘触发
}

void WebServer::Start() {
    int timeMS=-1;
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    while(!isClose_){
        if(timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick();     // 获取下一次的超时等待事件
        }
        int eventCnt=epoller_->Wait(timeMS);//epoll_wait获取发生状态变化的事件
        for (int i = 0; i < eventCnt; ++i) {
            //处理对应事件
            int fd=epoller_->GetEventFd(i);
            uint32_t events=epoller_->GetEvents(i);
            if(fd==listenFd_){
                DealListen_();
            }else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){//events事件是EPOLLRDHUP | EPOLLHUP | EPOLLERR任意一个
                assert(users_.count(fd)>0);
                CloseConn_(&users_[fd]);
            }else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            }
            else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::SendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

//关闭一个连接时 要关闭对应epollfd，连接池要释放
void WebServer::CloseConn_(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

//第一次建立连接时要建立时间堆，和用epoll监听对应的连接
void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if(timeoutMS_ > 0) {
        //这里不写bind 就访问不了users_[fd]和&WebServer::CloseConn_，因为他是私有的
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_); //监听对应的fd
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

//处理监听套接字，accept建立连接，再addclient，加入timer和epoller
void WebServer::DealListen_() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if(fd <= 0) { return;} //非阻塞状态下，如果没有连接 accept会返回-1，并将errno设置成eagain
        else if(HttpConn::userCount >= MAX_FD) {
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);
    } while(listenEvent_ & EPOLLET);
}


//处理读事件，主要逻辑是将OnRead加入线程池的任务队列中
void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);//刷新时间
    //同样this传入该实例，保证私有函数能够访问
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client)); // 这是一个右值，bind将参数和函数绑定
}

// 处理写事件，主要逻辑是将OnWrite加入线程池的任务队列中
void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

void WebServer::OnRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);         // 读取客户端套接字的数据，读到httpconn的读缓存区
    if(ret <= 0 && readErrno != EAGAIN) {   // 读异常就关闭客户端
        CloseConn_(client);
        return;
    }
    // 业务逻辑的处理（先读后处理）
    OnProcess(client);
}

/* 处理读（请求）数据的函数 */
void WebServer::OnProcess(HttpConn* client) {
    // 首先调用process()进行逻辑处理
    if(client->process()) { // 根据返回的信息重新将fd置为EPOLLOUT（写）或EPOLLIN（读）
        //读完事件就跟内核说可以写了
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);    // 响应成功，修改监听事件为写,等待OnWrite_()发送
    } else {
        //这里实际上 是 写完事件就跟内核说可以读了，但后面直接modfd了，所以其实没什么用了
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {
            // OnProcess(client); 承接上面的
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN); // 回归换成监测读事件
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {  // 缓冲区满了
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

//初始化监听套接字，其中包括了epoller进行监听，非阻塞
bool WebServer::InitSocket_() {
    int ret;
    struct sockaddr_in addr;
    if(port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    // 优雅关闭
    {
        struct linger optLinger = { 0 };
        if(openLinger_) {
            /* 优雅关闭: 直到所剩数据发送完毕或超时 */
            optLinger.l_onoff = 1;
            optLinger.l_linger = 1;
        }

        listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
        if(listenFd_ < 0) {
            LOG_ERROR("Create socket error!", port_);
            return false;
        }

        ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
        if(ret < 0) {
            close(listenFd_);
            LOG_ERROR("Init linger error!", port_);
            return false;
        }
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    // 绑定
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // 监听
    ret = listen(listenFd_, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);  // 将监听套接字加入epoller
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}