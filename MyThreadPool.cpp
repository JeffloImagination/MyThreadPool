// MyThreadPool.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
// 线程池测试函数， 包含main函数

#include <iostream>
#include<thread>
#include<chrono>
#include "threadpool.h"

class MyTask : public Task {
public:
    MyTask(int begin, int end) 
        : begin_(begin)
        , end_(end)
    {}
    Any run() {    // run方法最终就在线程池分配的线程中通过ThreadFunc函数被执行
        std::cout << "开始执行任务： " << std::endl;
        std::cout << "tid: " << std::this_thread::get_id() << " begin!" << std::endl;
        
        // 任务主体部分
        // std::this_thread::sleep_for(std::chrono::seconds(2));
        int sum = 0;
        for (int i = begin_; i <= end_; i++)
            sum += i;
        
        
        std::cout << "tid: " << std::this_thread::get_id() << " end!" << std::endl;
        std::cout << "任务执行完毕。。。" << std::endl;

        return sum;
    }
private:
    int begin_;
    int end_;
};

int main()
{
    std::cout << "Hello World!\n";
    ThreadPool Pool;
    Pool.setPoolMode(MODE_CACHED);
    Pool.start(4);

    Result res1 = Pool.submitTask(std::make_shared<MyTask>(1, 10000));
    Result res2 = Pool.submitTask(std::make_shared<MyTask>(10001, 20000));
    Result res3 = Pool.submitTask(std::make_shared<MyTask>(20001, 30000));

    int sum1 = res1.get().cast_<int>();
    int sum2 = res2.get().cast_<int>();
    int sum3 = res3.get().cast_<int>();

    std::cout << (sum1 + sum2 + sum3) << std::endl;

    /*Pool.submitTask(std::make_shared<MyTask>());
    Pool.submitTask(std::make_shared<MyTask>());
    Pool.submitTask(std::make_shared<MyTask>());*/

    std::this_thread::sleep_for(std::chrono::seconds(5));
    
}

