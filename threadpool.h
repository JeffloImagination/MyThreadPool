#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<thread>
#include<vector>
#include<queue>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>

// Any���ͣ� ���Խ����������ݵ�����
class Any {
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&& ) = default;

	// ����һ���������͵�����
	template<typename T> 
	Any(T data) : base_(std::make_unique<Derive<T>>(data)) 
	{}

	// ����������Դ�Any������ͨ��base_��ȡ�о������������
	template<typename T>
	T cast_() {
		Derive<T>* ptr = dynamic_cast<Derive<T>*>(base_.get());
		if (ptr == nullptr) {
			throw "Uncompatible type! ";
		}
		return ptr->data_;
	}


private:
	// ��������
	class Base {
	public:
		virtual ~Base() = default;
		Base() = default;
	};

	// ����������
	template<typename T>
	class Derive : public Base {
	public:
		Derive(T data) : data_(data) {}
		T data_; // �������������������
	};

private:
	// ����һ�������ָ��
	std::unique_ptr<Base> base_;
};

// ʵ��һ���ź�����
class Semaphore {
public:
	Semaphore(int limit = 0) : resLimit_(limit) 
	{}
	~Semaphore() = default;

	void wait() {
		std::unique_lock<std::mutex> lock(mtx_);
		// �ȴ��ź�������Դ����û�У�������ǰ�߳�
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}

	void post() {
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}

private:
	int resLimit_; // ������Դ
	std::mutex mtx_;
	std::condition_variable cond_;
};


class Result;


class Thread {
public:
	using ThreadFunc = std::function<void()>;

	Thread(ThreadFunc func_);
	~Thread();

	// �����߳�
	void start();
	
private:
	ThreadFunc func_;
};

// �����������
class Task {
public:
	Task();
	~Task() = default;

	virtual Any run() = 0;
	void exec();
	void setResult(Result* res);

private:
	Result* result_; // result�����������ǳ���task��
};

// ʵ��һ��Result���ͣ����Խ����ύ���̳߳ص�taskִ����ɺ�ķ���ֵ����
class Result {
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;

	Any get();

	void setVal(Any any);

private:
	Any any_; // �洢����ķ���ֵ
	Semaphore sem_; // �߳�ͨ���ź���
	std::shared_ptr<Task> task_;  // ָ���Ӧ��ȡ����ֵ���������
	std::atomic_bool isValid_; // ����ֵ�Ƿ���Ч
};

enum PoolMode {
	MODE_FIXED,
	MODE_CACHED
};


// �̳߳���
class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();

	void setPoolMode(PoolMode mode); // �����̳߳�ģʽ

	void setTaskQueueMaxThreshHold(int maxThreshHold); // �����������������ֵ

	Result submitTask(std::shared_ptr<Task> task); // �ύ����

	void start(int initthreadsize = 4); // �����̳߳�

	void threadFunc(); // �����̺߳���

	/*��ֹ�û�ʹ�ÿ������캯���븳ֵ���캯��*/
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;


private:
	std::vector<std::unique_ptr<Thread>> threads_;  // �߳��б�
	size_t initThreadSize_;  // ��ʼ�߳�����

	std::queue<std::shared_ptr<Task> > taskQueue_;  // �������
	std::atomic_int taskSize_;  // ��������
	int taskQueueMaxThreshHold_;  // ������������ֵ

	std::mutex taskQueueMtx_; // ������л�����
	std::condition_variable NotFull_; // �������δ����������
	std::condition_variable NotEmpty_; // �������δ����������

	PoolMode poolMode_;  // �̳߳�ģʽ
};



#endif // THREADPOOL_H



