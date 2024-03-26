#include<sqlconnpool.h>

SqlConnPool * SqlConnPool::Instance(){
    static SqlConnPool pool;
    return &pool;
}//单例懒汉模式

//初始化
void SqlConnPool::Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize = 10){
                assert(connSize>0);
                for (int i = 0; i < connSize; i++)
                {
                    MYSQL * conn=nullptr;
                    conn=mysql_init(conn);
                    if(!conn){
                        LOG_ERROR("Mysql init Error!");
                        assert(conn);
                    }
                    conn=mysql_real_connect(conn, host, user, pwd, dbName, port, nullptr, 0);
                    if (!conn)
                    {
                        LOG_ERROR("Mysql connect Error!");

                    }
                    connQue_.emplace(conn);//右值传递又来了
                    
                }
                MAX_CONN_ =connSize;
                sem_init(&semId_,0,MAX_CONN_);//初始化信号量
              }

//从连接池当中获取一个连接
MYSQL *SqlConnPool::GetConn(){
    MYSQL * conn=nullptr;
    if(connQue_.empty()){
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId_);
    lock_guard<mutex> locker(mtx_);
    conn=connQue_.front();
    connQue.pop();
    return conn;
}

//释放一个连接，实际是把这个连接存到连接池
void SqlConnPool::FreeConn(MYSQL * conn){
    assert(conn);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(conn);
    sem_post(&semId_);
}

void SqlConnPool::ClosePool(){
    lock_guard<mutex> locker(mtx_);
    while(!connQue_.empty()){ //释放连接池中的连接
        auto conn=connQue_.front();
        connQue_.pop();
        mysql_close(conn);
    }
    mysql_library_end();
}

int SqlConnPool::GetFreeConnCount(){
    lock_guard<mutex> locker(mtx_);
    return connQue_.size();
}