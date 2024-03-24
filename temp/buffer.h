#ifndef BUFFER_H
#define BUFFER_H
#include<iostream>
#include<string>
#include<vector>
#include<atomic>
#include<assert.h>

class Buffer{
public:
    Buffer(int initBuffSize = 1024); //定义缓冲区大小
    ~Buffer()=default; //析构函数

    size_t WritableBytes() const; //返回可写字节数
    size_t ReadabelBytes() const; //返回可读字节数
    size_t PrependableBytes() const; //返回预备字节数

    const char * Peek() const; //返回缓冲区起始地址
    void EnsureWritable(size_t len); //确保可写的长度
    void HasWritten(size_t len); //写入len个字节，移动写入位置

    void Retrieve(size_t len); //读取len个字节
    void RetrieveUntil(const char * end); //读取直到end

    void RetrieveAll(); //读取所有
    std::string RetrieveAllToStr(); //读取所有并返回字符串

    const char * BeginWriteConst() const; //返回可写地址
    char * BeginWrite(); //返回可写地址

    void Append(const std::string & str); //追加字符串
    void Append(const char * str, size_t len); //追加字符串
    void Append(const void * data, size_t len); //追加字符串
    void Append(const Buffer & buff); //追加字符串

    ssize_t ReadFd(int fd, int * Errno); //从fd读取数据
    ssize_t WriteFd(int fd, int * Errno); //向fd写入数据

private:
    char * BeginPtr_(); //返回缓冲区起始地址

    const char * BeginPtr_() const; //返回缓冲区起始地址

    void MakeSpace_(size_t len); //拓展缓冲区

    std::vector<char> buffer_; //缓冲区
    std::atomic<size_t> readPos_; //读取位置
    std::atomic<size_t> writePos_; //写入位置

    
};

#endif;