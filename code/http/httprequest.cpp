#include "httprequest.h"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
    "/index","/register","/login","/welcome","/video","/picture"
};

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/register.html",0},{"/login.html",1},
};

void HttpRequest::Init(){
    method_=path_=version_=body_="";
    state_= REQUEST_LINE;
    header_.clear();
    post_.clear();
}

// 是否持久连接
bool HttpRequest::IsKeepAlive() const{
    if(header_.count("Connection")==1){
        return header_.find("Connection")->second=="keep-alive" && version_=="1.1";
    }
    return false;
}

bool HttpRequest::parse(Buffer & buff){
    const char CRLF[] ="\r\n";  //行结束符标志，CRLF就是回车换行的意思 注意这里是两个字符而不是4个字符！！
    if(buff.ReadableBytes()<=0){
        return false;
    }   //没有可读的数据就结束
    //读取数据
    while(buff.ReadableBytes() && state_!=FINISH){
        //从buff中的可读数据读取一行数据，并且是去除\r \n的，因为search是找到对应字符的开始的位置
        const char * lineEnd=search(buff.Peek(),buff.BeginWriteConst(),CRLF,CRLF+2);
        //转换为string
        string line(buff.Peek(),lineEnd);
        //根据有限状态机，处理对应行
        switch (state_)
        {
        case REQUEST_LINE:
            if(!ParseRequestLine_(line)){
                return false;
            }
            ParsePath_();
            break;
        case HEADERS:
            /* code */
            ParseHeader_(line);
            if(buff.ReadableBytes()<=2){ //因为尾部\r\n两个字符
                state_=FINISH;
            }
            break;
        case BODY:
            /* code */
            ParseBody_(line);
            break;
        default:
            break;
        }
        if(lineEnd==buff.BeginWriteConst()){break;} //读取到最后的回车换行部分了
        buff.RetrieveUntil(lineEnd+2); //跳过前面的尾部\r\n部分
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

//解析路劲
void HttpRequest::ParsePath_(){
    if(path_=="/"){
        path_="/index.html";
    }else{
        for(auto &item:DEFAULT_HTML){
            if(item == path_){
                path_ +=".html";
                break;
            }
        }
    }
}

//解析请求行
bool HttpRequest::ParseRequestLine_(const string& line){
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");     //([^ ]*)匹配任意个不为空的字符，^是开头匹配，$是末尾匹配
    smatch subMatch;
    //总共匹配了四项 括号匹配了3个，还有一个整体，所以下面没有取0，0就是包含的整体
    if(regex_match(line,subMatch,patten)){
        method_=subMatch[1];    //方法
        path_=subMatch[2];      //路径
        version_=subMatch[3];   //版本
        state_=HEADERS; //进入下一个状态
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

bool HttpRequest::ParseHeader_(const string & line){
    regex patten("^([^:]*): ?(.*)$");   //不以冒号开头的任意个字符，:之后在跟值
    smatch subMatch;
    if(regex_match(line,subMatch,patten)){
        header_[subMatch[1]]=subMatch[2];
        //这里没有紧跟状态转换，是因为头部有多个，不知道匹配完没有
    }else{
        state_=BODY;    //匹配不上了才转换为下一状态
    }
}

bool HttpRequest::ParseBody_(const string & line){
    body_=line;     //请求体就不用匹配了
    ParsePost_();   //post请求会携带请求体
    state_=FINISH;  //转换为下一状态
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

// 16进制转化为10进制
int HttpRequest::ConverHex(char ch) {
    if(ch>='A' && ch<='F') return ch-'A'+10;
    if(ch>='a' && ch<='f') return ch-'a'+10;
    return ch;  // 不是字符就返回原数字
}

// 处理post请求，解析请求体是否有请求头，另外就是处理特殊的注册和登录 post请求
void HttpRequest::ParsePost_() {
    if(method_=="POST" && header_["Content-Type"]=="application/x-www-form-urlencoded"){
        ParseFromUrlencoded_(); //POST请求体实例
        if(DEFAULT_HTML_TAG.count(path_)){  //处理登录/注册请求
            int tag=DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("TAG::%d",tag);
            if(tag==1 || tag==0){
                bool isLogin=(tag==1);
                if(UserVerify(post_["username"],post_["password"],isLogin)){
                    path_="/welcome.html";
                }else{
                    path_="/error.html";
                }
            }
        }

    }
}

//从url中解析请求体
void HttpRequest::ParseFromUrlencoded_() {
    if(body_.empty()){return ;}

    string key,value;
    int num=0;
    int n=body_.size();
    int i=0,j=0;

    for (; i < n; ++i) {
        char ch=body_[i];
        switch (ch) {
            case '=':   // 遇到等号就要把前面的key保留下来
                key=body_.substr(j,i-j);
                j=i+1;
                break;
            case '+':
                body_[i]=' ';
                break;
            case '%':
                num= ConverHex(body_[i+1])*16+ ConverHex(body_[i+2]);
                body_[i+1]=num%10+'0';
                body_[i+2]=num/10+'0';
                i+=2;
                break;
            case '&':
                value=body_.substr(j,i-j); //获取值
                j=i+1;//开始下一个键值对的解析
                post_[key]=value;
                LOG_DEBUG("%s=%s",key.c_str(),value.c_str());
                break;
            default:
                break;
        }
    }
    assert(j<=i);
    if(post_.count(key)==0 && j<i){ //值的判断是根据&，但是最后一个值后面没有&就要手动来识别
        value=body_.substr(j,i-j);
        post_[key]=value;
    }

}

bool HttpRequest::UserVerify(const std::string &name, const std::string &pwd, bool isLogin) {
    if(name==" " || pwd==" ")return false;
    LOG_INFO("Verify name::%s pwd::%s",name.c_str(),pwd.c_str());
    MYSQL* sql;
    SqlConnRAII(&sql,SqlConnPool::Instance());
    assert(sql);

    bool flag=false;
    unsigned int j=0;
    char order[256]={0};
    MYSQL_FIELD * fields= nullptr;
    MYSQL_RES *res= nullptr;

    if(!isLogin){flag=true;}
    /*查询用户名和密码是否存在*/
    snprintf(order,256,"SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s",order);

    if(mysql_query(sql,order)){//查询成功才是返回0，不然都是返回其他
        mysql_free_result(res);
        return false;
    }
    res=mysql_store_result(sql);
    j=mysql_num_fields(res);
    fields=mysql_fetch_fields(res);

    while(MYSQL_ROW row=mysql_fetch_row(res)){
        LOG_DEBUG("MYSQL ROW:%s %s",row[0],row[1]);
        string password(row[1]);
        // 登录
        if(isLogin){
            if(pwd==password){flag=true;}//密码正确
            else {
                flag = false;//密码错误
                LOG_INFO("pwd error!");
            }
        }else{
            //注册，用户名被使用
            flag=false;
            LOG_INFO("user used!");
        }
    }
    mysql_free_result(res);

    //注册
    if(!isLogin && flag){
        LOG_DEBUG("register!");
        bzero(order,256);
        snprintf(order,256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s",order);
        if(mysql_query(sql,order)){
            LOG_DEBUG("Insert error!");
            flag=false;
        }
        flag=true;
    }
    LOG_DEBUG("User Verify Success!");
    return flag;
}

std::string HttpRequest::path() const {
    return path_;
}

std::string &HttpRequest::path() {
    return path_;
}

std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const char *key) const {
    assert(key != nullptr);
    if(post_.count(key)!=0){
        return post_.find(key)->second;
    }
    return " ";
}

std::string HttpRequest::GetPost(const std::string &key) const {
    assert(!key.empty());
    if(post_.count(key)!=0){
        return post_.find(key)->second;
    }
    return " ";
}