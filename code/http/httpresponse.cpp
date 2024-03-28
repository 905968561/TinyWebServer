//
// Created by 90596 on 2024/3/27.
//
#include "httpresponse.h"

using namespace std;

//常见后缀类型集合
const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
        { ".html",  "text/html" },
        { ".xml",   "text/xml" },
        { ".xhtml", "application/xhtml+xml" },
        { ".txt",   "text/plain" },
        { ".rtf",   "application/rtf" },
        { ".pdf",   "application/pdf" },
        { ".word",  "application/nsword" },
        { ".png",   "image/png" },
        { ".gif",   "image/gif" },
        { ".jpg",   "image/jpeg" },
        { ".jpeg",  "image/jpeg" },
        { ".au",    "audio/basic" },
        { ".mpeg",  "video/mpeg" },
        { ".mpg",   "video/mpeg" },
        { ".avi",   "video/x-msvideo" },
        { ".gz",    "application/x-gzip" },
        { ".tar",   "application/x-tar" },
        { ".css",   "text/css "},
        { ".js",    "text/javascript "},
};

//状态码集合
const unordered_map<int, string> HttpResponse::CODE_STATUS = {
        { 200, "OK" },
        { 400, "Bad Request" },
        { 403, "Forbidden" },
        { 404, "Not Found" },
};

//编码集合
const unordered_map<int, string> HttpResponse::CODE_PATH = {
        { 400, "/400.html" },
        { 403, "/403.html" },
        { 404, "/404.html" },
};

HttpResponse::HttpResponse() {
    code_=-1;
    path_=srcDir_="";
    isKeepAlive_= false;
    mmFile_= nullptr;
    mmFileStat_={0};
}

HttpResponse::~HttpResponse() {
    UnmapFile();
}

// 初始化
void HttpResponse::Init(const std::string &srcDir, std::string &path, bool isKeepAlive, int code) {
    assert(!srcDir.empty());
    if(mmFile_){ UnmapFile();}//清空文件内容
    code_=code;
    isKeepAlive_=isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr;
    mmFileStat_ = { 0 };
}

void HttpResponse::MakeResponse(Buffer &buff) {
    //检查对应文件是否存在 或者 对应文件名是否是个目录 stat获取文件状态信息存储到mmFileStat_中，执行成功会返回0
    if(stat((srcDir_+path_).data(),&mmFileStat_)<0 || S_ISDIR(mmFileStat_.st_mode)){
        code_=404;
    }else if (!(mmFileStat_.st_mode & S_IROTH)){    //检查文件是否有权限读 按位与 &如果包含其他用户的读权限就为1 在！一下就为0
        code_=403;
    }else if(code_==-1){
        code_=200;
    }
    ErrorHtml_();//根据前面code_来返回错误页面信息
    AddStateLine_(buff);
    AddHeader_(buff);
    AddContent_(buff);
}

char *HttpResponse::File() {
    return mmFile_;
}

size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size;
}

void HttpResponse::ErrorHtml_() {
    // 如果是404，403就返回错误信息
    if(CODE_PATH.count(code_)==1){
        path_=CODE_PATH.find(code_)->second;
        stat((srcDir_+path_).data(),&mmFileStat_);
    }
}

//生成响应行
void HttpResponse::AddStateLine_(Buffer &buff) {
    string  status;
    if(CODE_STATUS.count(code_)==1) {
        status = CODE_STATUS.find(code_)->second;
    }else{
        code_=400;
        status = CODE_STATUS.find(code_)->second;
    }
    buff.Append("HTTP/1.1 "+ to_string(code_)+ " "+ status+"\r\n");
}

//生成响应头，没啥注意点
void HttpResponse::AddHeader_(Buffer &buff) {
    buff.Append("Connection: ");
    if (isKeepAlive_){
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    }else{
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: "+GetFileType_()+"\r\n");
}

//添加响应内容
void HttpResponse::AddContent_(Buffer &buff) {
    int srcFd= open((srcDir_+path_).data(),O_RDONLY);
    if(srcFd<0){
        ErrorContent(buff,"File NotFound!");
        return;
    }

    //将文件映射到内存提高文件的访问速度  MAP_PRIVATE 建立一个写入时拷贝的私有映射,对映射区域的写操作不会影响原文件。
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);// mmRet就是映射到内存的地址
    if(*mmRet == -1) {
        ErrorContent(buff, "File NotFound!");
        return;
    }
    mmFile_ = (char*)mmRet;//这里转换为char型指针是为了方便操作
    close(srcFd);
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

//清空文件内容
void HttpResponse::UnmapFile() {
    if(mmFile_){
        munmap(mmFile_, mmFileStat_.st_size); //解除内存映射 mmFile_内存起始地址
        mmFile_ = nullptr;
    }
}

std::string HttpResponse::GetFileType_() {
    string::size_type idx=path_.find_last_of('.');
    if(idx==string::npos){ // 最大值 find函数在找不到指定值得情况下会返回string::npos
        return "text/plain"; //纯文本类型
    }
    string suffix=path_.substr(idx);
    if(SUFFIX_TYPE.count(suffix)==1){
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}

//错误包体
void HttpResponse::ErrorContent(Buffer &buff, std::string message) {
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request";
    }
    body += to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}