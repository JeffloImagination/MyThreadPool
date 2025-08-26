#include"threadpool.h"

#include<thread>
#include<iostream>

const int maxTaskQueThreshHold = 1024;

/*--------------线程池类部分定义----------------*/
ThreadPool::ThreadPool() 
	: initThreadSize_(4)
	, taskSize_(0)
	, taskQueueMaxThreshHold_(maxTaskQueThreshHold)
	, poolMode_(MODE_FIXED)
{}


ThreadPool::~ThreadPool()
{}

void ThreadPool::setPoolMode(PoolMode mode) {
	poolMode_ = mode;
}// 设置线程池模式

void ThreadPool::setTaskQueueMaxThreshHold(int maxThreshHold) {
	taskQueueMaxThreshHold_ = maxThreshHold;
} // 设置任务队列上线阈值

Result ThreadPool::submitTask(std::shared_ptr<Task> task) {
	// 获取锁
	std::unique_lock<std::mutex> lock(taskQueueMtx_); // 线程的通信、 等待任务队列有空余

	// 等待任务队列有空余， 将任务放入任务队列， 在notEmpty_上进行通知
	// 用户提交任务，最长阻塞不能超过1s
	if (!NotFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool { return taskQueue_.size() < taskQueueMaxThreshHold_; })) {
		// 表示等待时间已经超过1s， 条件依然没有被满足
		std::cerr << "Task queue is full, submit task failed." << std::endl;
		return Result(task, false);
	}

	taskQueue_.emplace(task);
	taskSize_++;

	NotEmpty_.notify_all();  // 通知消费者 -》 线程池
	return Result(task);

}// 提交任务, 用户调用该接口，生成任务 =》 生产者

void ThreadPool::start(int initthreadsize) {
	// 记录初始线程数
	initThreadSize_ = initthreadsize;

	// 创建线程对象
	for (int i = 0; i < initThreadSize_; i++) {
		auto ptr = std::make_unique<Thread>(Thread(std::bind(&ThreadPool::threadFunc, this)));
		threads_.emplace_back(std::move(ptr));
	}

	// 启动线程
	for (int i = 0; i < initThreadSize_; i++) {
		threads_[i]->start();
	}

}// 启动线程池

void ThreadPool::threadFunc() {
	/*std::cout << "Begin threadFunc: " << std::this_thread::get_id() << " starts\n";
	std::cout << std::this_thread::get_id() << "ends." << std::endl;*/

	std::shared_ptr<Task> task;
	for (;;) {
		{ //为了及时释放锁， 特地新建一个作用域
			// 先获取锁
			std::unique_lock<std::mutex> lock(taskQueueMtx_);

			std::cout << "tid: " << std::this_thread::get_id() << "尝试获取任务。。。" << std::endl;

			// 等待notEmpty条件
			// 从任务队列中取一个任务出来，当前线程负责执行这个任务
			NotEmpty_.wait(lock, [&]()->bool {return taskQueue_.size() > 0; });

			std::cout << "tid: " << std::this_thread::get_id() << "获取任务成功。。。" << std::endl;

			task = taskQueue_.front();
			taskQueue_.pop();
			taskSize_--;

			// 判断队列是否为空， 通知其他线程进行消费
			if (taskSize_ > 0) {
				NotEmpty_.notify_all();
			}

			// 此时队列肯定非满， 通知submitTask可以继续进行生产任务
			NotFull_.notify_all();

		} // 释放unique_lock

		// 执行任务
		if (task != nullptr) {
			// task->run();   // 1. 执行任务  2. 把任务的返回值通过setVal方法给到Result
			task->exec();
 		}

	}

}// 定义线程函数


/*-----------------线程类部分定义--------------------*/

Thread::Thread(ThreadFunc func) 
	: func_(func)
{}

Thread::~Thread()
{}

// 启动线程
void Thread::start() {
	// 创建一个线程来执行一个线程函数
	std::thread t(func_);  // 线程对象t 和 线程函数func_
	t.detach();  // 设置分离线程  相当于Linux中的pthread_detach

}

/*-----------------  Task方法实现 --------------------*/
Task::Task() : result_(nullptr)
{}

void Task::exec() {

	if (result_ != nullptr) {
		result_->setVal(run()); // 这里发生多态调用
	}
}

void Task::setResult(Result* res) {
	result_ = res;
}



// ----------------- Result方法的实现 ---------------------------
Result::Result(std::shared_ptr<Task> task, bool isValid) 
	: task_(task), isValid_(isValid)
{
	task_->setResult(this);
}

Any Result::get() {
	if (!isValid_) {
		return "";
	}
	sem_.wait(); // task任务如果没有执行完，会在这里阻塞用户的线程
	return std::move(any_);
}

void Result::setVal(Any any) {
	// 存储task的返回值
	this->any_ = std::move(any);
	sem_.post();  // 已经获取的任务的返回值，增加信号量资源 
}
