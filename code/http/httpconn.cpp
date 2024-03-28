//
// Created by 90596 on 2024/3/28.
//

#include "httpconn.h"
using namespace std;

const char* HttpConn::srcDir;
atomic<int> HttpConn::userCount;
bool HttpConn::isET;

HttpConn::HttpConn() {
    fd_=-1;
    addr_={0};
    isClose_=true;
}

HttpConn::~HttpConn() {
    Close();
}

void HttpConn::init(int sockFd, const int &addr) {
    assert(sockFd > 0);
    userCount++;
    addr_ = addr;
    fd_ = sockFd;
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int) userCount);
}

void HttpConn::Close() {
    response_.UnmapFile();//清空映射
    if(isClose_== false){
        isClose_= true;
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }

}

int HttpConn::GetFd() const {
    return fd_;
}

int HttpConn::GetAddr() const {
    return addr_;
}

const char *HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

//读取缓冲区数据
ssize_t HttpConn::read(int *saveErrno) {
    ssize_t len=-1;
    do{
        len=readBuff_.ReadFd(fd_,saveErrno);
        if(len<0){
            break;
        }
    } while (isET);//边缘触发while循环保证一次将数据读完，这个是配合epoll来用的
    return len;
}

// write来将缓冲区数据写入fd中
ssize_t HttpConn::write(int *saveErrno) {
    ssize_t len =-1;
    do {
        len=writev(fd_,iov_,iovCnt_);
        if(len<=0){
            *saveErrno=errno;
            break;
        }
        if(iov_[0].iov_len+iov_[1].iov_len ==0){ break;}//数据传输完毕
        else if(static_cast<size_t>(len)>iov_[0].iov_len){ //返回长度大于第一个vec的长度，证明第一个读取完毕
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len); //发送第二个剩余数据
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {   //这里是一次就读完第一个iov，就要手动去设置下
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }else{
            iov_[0].iov_base=(uint8_t*)iov_[0].iov_base +len;
            iov_[0].iov_len -= len;
            writeBuff_.Retrieve(len);
        }
    } while (isET || ToWriteBytes() > 10240);
    return len;
}

//再此之前要先read到buff中，之后再write到fd中
bool HttpConn::process() {
    request_.Init();
    if(readBuff_.ReadableBytes()<=0){
        return false;
    }else if(request_.parse(readBuff_)){
        LOG_DEBUG("%s", request_.path().c_str());
        response_.Init(srcDir,request_.path(),request_.IsKeepAlive(),200);
    }else {
        response_.Init(srcDir,request_.path(),false,400);
    }

    response_.MakeResponse(writeBuff_); //生成响应报文到writeBuff中

    //响应报头内容(buffer中)放入 iov[0]中
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    //文件内容放入 iov[1]中，因为之前将内容映射到了内存中
    if(response_.FileLen()>0 && response_.File()){
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    return true;
}