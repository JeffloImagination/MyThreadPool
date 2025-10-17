#include"threadpool.h"

#include<thread>
#include<iostream>

const int maxTaskQueThreshHold = 1024;
const int THREAD_MAX_THRESH_HOLD = 10;
const int THREAD_MAX_IDLE_TIME = 60;

/*--------------线程池类部分定义----------------*/
ThreadPool::ThreadPool() 
	: initThreadSize_(4)
	, taskSize_(0)
	, idleThreadSize_(0)
	, curThreadSize_(0)
	, taskQueueMaxThreshHold_(maxTaskQueThreshHold)
	, threadSizeThreshHold_(THREAD_MAX_THRESH_HOLD)
	, poolMode_(MODE_FIXED)
	, isPoolRunning_(false)
{}


ThreadPool::~ThreadPool()
{}

void ThreadPool::setPoolMode(PoolMode mode) {
	if (checkRunningState()) {
		return;
	}
	poolMode_ = mode;
}// 设置线程池模式

void ThreadPool::setTaskQueueMaxThreshHold(int maxThreshHold) {
	taskQueueMaxThreshHold_ = maxThreshHold;
} // 设置任务队列上线阈值

void ThreadPool::setThreadSizeThreshHold(int max_thresh_hold) {
	if (checkRunningState()) {
		return;
	}
	if (poolMode_ == PoolMode::MODE_CACHED) {
		threadSizeThreshHold_ = max_thresh_hold;
	}
} // 设置线程池cached模式下的线程上限阈值

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

	// cached模式 任务处理比较紧急 场景： 小而快的任务
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < threadSizeThreshHold_) {
		// 创建新线程
		std::cout << ">>> create new thread! " << std::endl;
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		//threads_.emplace_back(std::move(ptr));
		curThreadSize_++;
	}

	return Result(task);

}// 提交任务, 用户调用该接口，生成任务 =》 生产者

void ThreadPool::start(int initthreadsize) {
	isPoolRunning_ = true;
	// 记录初始线程数
	initThreadSize_ = initthreadsize;
	curThreadSize_ = initthreadsize;

	// 创建线程对象
	for (int i = 0; i < initThreadSize_; i++) {
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		// threads_.emplace_back(std::move(ptr));
		
	}

	// 启动线程
	for (int i = 0; i < initThreadSize_; i++) {
		threads_[i]->start(); // 需要去执行一个线程函数
		idleThreadSize_++; // 记录初始空闲线程的数量
	}

}// 启动线程池

void ThreadPool::threadFunc(int threadid) {

	auto lastTime = std::chrono::high_resolution_clock().now();
	
	for (;;) {
		std::shared_ptr<Task> task;
		{ //为了及时释放锁， 特地新建一个作用域
			// 先获取锁
			std::unique_lock<std::mutex> lock(taskQueueMtx_);

			std::cout << "tid: " << std::this_thread::get_id() << "尝试获取任务。。。" << std::endl;

			// cached模式下，如果（当前时间 - 上一次线程执行的时间 > 60s）， 需要对多余的线程进行回收
			if (poolMode_ == PoolMode::MODE_CACHED) {
				// 每一秒中返回一次
				while (taskQueue_.size() > 0) {
					// 条件变量， 超时返回了
					if (std::cv_status::timeout == NotEmpty_.wait_for(lock, std::chrono::seconds(1))) {
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME
							&& curThreadSize_ > initThreadSize_) {
							// 开始回收当前线程
							// 记录线程数量的相关变量的值修改
							// 把线程对象从线程列表容器中删除  threadid -> thread对象
							threads_.erase(threadid);
							curThreadSize_--;
							idleThreadSize_--;

							std::cout << "threadid:" << std::this_thread::get_id() << "exit!" << std::endl;
							return;
						}
					}
				}
			}


			// 等待notEmpty条件
			// 从任务队列中取一个任务出来，当前线程负责执行这个任务
			NotEmpty_.wait(lock, [&]()->bool {return taskQueue_.size() > 0; });
			idleThreadSize_--;

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
		lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间
		idleThreadSize_++;
	}

}// 定义线程函数

bool ThreadPool::checkRunningState() const {
	return isPoolRunning_;
}


/*-----------------线程类部分定义--------------------*/

int Thread::generateId_ = 0;

Thread::Thread(ThreadFunc func) 
	: func_(func)
	, threadId_(generateId_++)
{}

Thread::~Thread()
{}

// 启动线程
void Thread::start() {
	// 创建一个线程来执行一个线程函数
	std::thread t(func_, threadId_);  // 线程对象t 和 线程函数func_
	t.detach();  // 设置分离线程  相当于Linux中的pthread_detach

}

int Thread::getId() const {
	return threadId_;
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
