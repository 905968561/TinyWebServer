#include<buffer.h>

Buffer::Buffer(int initBuffSize):buffer_(initBuffSize),readPos_(0),writePos_(0){}   //构造函数

// 返回可写字节数
size_t Buffer::WritableBytes() const{
    return buffer_.size()-writePos_;
}
// 返回可读字节数
size_t Buffer::ReadabelBytes() const{
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
        MakeSpace_(len);
    }
    assert(WritableBytes()>=len);
}

// 移动读取位置
void  Buffer::HasWritten(size_t len){
    writePos_+=len;
}

// 移动写下标
void Buffer::Retrieve(size_t len){
    assert(len<=ReadabelBytes());
    readPos_+=len;
}

// 读取直到end位置
void Buffer::RetrieveUntil(const char *end){
    assert(Peek()<end);
    Retrieve(end-Peek());//并移动写下标
}



void Buffer::MakeSpace_(size_t len){
    if(WritableBytes()+PrependableBytes()<len){
        buffer_.resize(writePos_+len+1);
    }else
    
}

