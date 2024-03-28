#include "buffer.h"

Buffer::Buffer(int initBuffSize):buffer_(initBuffSize),readPos_(0),writePos_(0){}   //构造函数

// 返回可写字节数
size_t Buffer::WritableBytes() const{
    return buffer_.size()-writePos_;
}
// 返回可读字节数
size_t Buffer::ReadableBytes() const{
    return writePos_-readPos_;
}
// 返回预备字节数
size_t Buffer::PrependableBytes() const{
    return readPos_;
}

// 返回缓冲区开始可读的地址
const char *Buffer::Peek() const{
    return &buffer_[readPos_];
}

// 确保可写的长度
void Buffer::EnsureWritable(size_t len){
    if(WritableBytes()<len){
        MakeSpace_(len);//如果不够要扩充缓冲区，对应到后面readfd
    }
    assert(WritableBytes()>=len);
}

// 移动读取位置
void  Buffer::HasWritten(size_t len){
    writePos_+=len;
}

// 移动写下标
void Buffer::Retrieve(size_t len){
    assert(len<=ReadableBytes());
    readPos_+=len;
}

// 读取直到end位置
void Buffer::RetrieveUntil(const char *end){
    assert(Peek()<end);
    Retrieve(end-Peek());//并移动写下标
}

//取出所有数据，buffer归0，读写下标归0
void Buffer::RetrieveAll(){
    bzero(&buffer_[0],buffer_.size());
    readPos_=writePos_=0; //vector 一般都不能删除值，都是用覆盖，所以直接下标操作
}

//用字符串返回所有数据并清0
std::string Buffer::RetrieveAllToStr(){
    std::string str(Peek(),ReadableBytes());
    RetrieveAll();
    return str;
}

//写指针的位置
const char *Buffer::BeginWriteConst() const{
    return &buffer_[writePos_];
}
char *Buffer::BeginWrite(){
    return &buffer_[writePos_];
}

// 将str移动到缓冲区
void Buffer::Append(const char * str, size_t len){
    assert(str);
    EnsureWritable(len); //确保可写长度
    std::copy(str,str+len,BeginWrite()); //将str放到写下标开始的位置
    HasWritten(len);//移动写下标
}
void Buffer::Append(const std::string & str){
    Append(str.c_str(),str.size());
}
// 万能指针处理方式
void Buffer::Append(const void * data, size_t len){
    Append(static_cast<const char*>(data), len);
}
// 把buff对象的读下表开始的位置的数据加到buffer中
void Buffer::Append(const Buffer & buff){
    Append(buff.Peek(),buff.ReadableBytes());
}

//从fd读数据
ssize_t Buffer::ReadFd(int fd, int * Errno){
    char buff[65535]; //放多余数据的栈
    struct iovec iov[2];
    size_t writable=WritableBytes();
    iov[0].iov_base=BeginWrite(); //第一个读入buffer的地方，如果读不满就不用第二个队列勒
    iov[0].iov_len=writable;
    iov[1].iov_base=BeginWrite();
    iov[1].iov_len=sizeof(buff);

    ssize_t len=readv(fd,iov,2);
    if(len<0){
        *Errno=errno;
    }else if(static_cast<size_t>(len)<=writable){
        writePos_+=len; //读不满缓冲区
    }else{
        writePos_=buffer_.size();//读满缓冲区勒
        Append(buff,static_cast<size_t>(len)-writable);
    }
    return len;
}

//向fd写数据
ssize_t Buffer::WriteFd(int fd, int * Errno){
    ssize_t len=write(fd,Peek(),ReadableBytes());
    if(len<0){
        *Errno=errno;
        return len;
    }
    Retrieve(len);//读了的数据要消除
    return len;
}

char * Buffer::BeginPtr_(){
    return &buffer_[0];
}
const char * Buffer::BeginPtr_() const{
    return &buffer_[0];
}


// 主要用来读取数据时vector不够来扩充vector
void Buffer::MakeSpace_(size_t len){
    if(WritableBytes()+PrependableBytes()<len){
        buffer_.resize(writePos_+len+1); //如果后面可写的部分和前面已经读完的部分加起来还不够那只有扩充vector勒
    }else{//如果够，就是要把前面的接到后面来
            //大错特错，vector怎么接？只有把还需要读复制到前面去
        size_t readabel=ReadableBytes();
        std::copy(BeginPtr_()+readPos_,BeginPtr_()+writePos_,BeginPtr_());
        readPos_=0;
        writePos_=readabel;
        assert(readabel==ReadableBytes());
    }
}

