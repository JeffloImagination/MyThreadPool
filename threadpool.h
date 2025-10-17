#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<thread>
#include<vector>
#include<queue>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<unordered_map>

// Any类型： 可以接收任意数据的类型
class Any {
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&& ) = default;

	// 接受一个任意类型的数据
	template<typename T> 
	Any(T data) : base_(std::make_unique<Derive<T>>(data)) 
	{}

	// 这个函数可以从Any类型中通过base_提取中具体的数据类型
	template<typename T>
	T cast_() {
		Derive<T>* ptr = dynamic_cast<Derive<T>*>(base_.get());
		if (ptr == nullptr) {
			throw "Uncompatible type! ";
		}
		return ptr->data_;
	}


private:
	// 基类类型
	class Base {
	public:
		virtual ~Base() = default;
		Base() = default;
	};

	// 派生类类型
	template<typename T>
	class Derive : public Base {
	public:
		Derive(T data) : data_(data) {}
		T data_; // 保存了任意的其他类型
	};

private:
	// 定义一个基类的指针
	std::unique_ptr<Base> base_;
};

// 实现一个信号量类
class Semaphore {
public:
	Semaphore(int limit = 0) : resLimit_(limit) 
	{}
	~Semaphore() = default;

	void wait() {
		std::unique_lock<std::mutex> lock(mtx_);
		// 等待信号量有资源，若没有，阻塞当前线程
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}

	void post() {
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}

private:
	int resLimit_; // 计数资源
	std::mutex mtx_;
	std::condition_variable cond_;
};


class Result;


class Thread {
public:
	using ThreadFunc = std::function<void(int)>;

	Thread(ThreadFunc func_);
	~Thread();

	// 启动线程
	void start();

	// 获取线程id
	int getId() const;
	
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_; // 保存线程id
};

// 抽象任务基类
class Task {
public:
	Task();
	~Task() = default;

	virtual Any run() = 0;
	void exec();
	void setResult(Result* res);

private:
	Result* result_; // result的生命周期是长于task的
};

// 实现一个Result类型，可以接收提交到线程池的task执行完成后的返回值类型
class Result {
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;

	Any get();

	void setVal(Any any);

private:
	Any any_; // 存储任务的返回值
	Semaphore sem_; // 线程通信信号量
	std::shared_ptr<Task> task_;  // 指向对应获取返回值的任务对象
	std::atomic_bool isValid_; // 返回值是否有效
};

enum PoolMode {
	MODE_FIXED,
	MODE_CACHED
};


// 线程池类
class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();

	void setPoolMode(PoolMode mode); // 设置线程池模式

	void setTaskQueueMaxThreshHold(int maxThreshHold); // 设置任务队列上线阈值

	void setThreadSizeThreshHold(int max_thresh_hold); // 设置线程池cached模式下的线程上限阈值

	Result submitTask(std::shared_ptr<Task> task); // 提交任务

	void start(int initthreadsize = 4); // 启动线程池

	void threadFunc(int threadid); // 定义线程函数

	/*禁止用户使用拷贝构造函数与赋值构造函数*/
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	// 检查pool运行状态
	bool checkRunningState() const;

private:
	// std::vector<std::unique_ptr<Thread>> threads_;  // 线程列表
	std::unordered_map<int, std::unique_ptr<Thread>> threads_; // 线程列表
	size_t initThreadSize_;  // 初始线程数量
	std::atomic_int curThreadSize_;	// 记录当前线程池里面的线程总数量
	int threadSizeThreshHold_; // 线程数量上限阈值

	std::queue<std::shared_ptr<Task> > taskQueue_;  // 任务队列
	std::atomic_int taskSize_;  // 任务数量
	int taskQueueMaxThreshHold_;  // 任务数上限阈值

	std::mutex taskQueueMtx_; // 任务队列互斥锁
	std::condition_variable NotFull_; // 任务队列未满条件变量
	std::condition_variable NotEmpty_; // 任务队列未空条件变量

	PoolMode poolMode_;  // 线程池模式

	std::atomic_bool isPoolRunning_;  // 表示线程池是否启动
	std::atomic_int idleThreadSize_; // 空闲线程的数量
};



#endif // THREADPOOL_H



