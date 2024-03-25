#include<iostream>
#include<atomic>
#include<thread>
#include<vector>
#include<atomic>
#include <sys/time.h>
using namespace std;
 
// atomic<int> num (0);
// int num1 (0);
 
// // 线程函数,内部对num自增1000万次
// void Add()
// {
//     for(int i=0;i<10000000;i++) 
//     {
//         num++;
//         num1++;
//     }
// }
 
int main()
{
    // clock_t startClock = clock();   // 记下开始时间
    // // 3个线程,创建即运行
    // thread t1(Add);
    // thread t2(Add);
    // thread t3(Add);
    // // 等待3个线程结束
    // t1.join();
    // t2.join();
    // t3.join();
    // clock_t endClock = clock();     // 记下结束时间
    // cout<<"耗时:"<<endClock-startClock<<",单位:"<<CLOCKS_PER_SEC<<",result num:"<<num<<",result num1:"<<num1<<endl;
    // return 0;
    time_t timer=time(nullptr);
    struct tm* systime=localtime(&timer);
    cout<< systime->tm_year <<endl;
    struct timeval now={0,0};
    gettimeofday(&now,nullptr);
    time_t tSec=now.tv_sec;
    struct tm* systime1=localtime(&tSec);
    struct tm t=*systime;
    cout<< t.tm_year <<endl;
    return 0;
}