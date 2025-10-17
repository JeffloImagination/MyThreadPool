#include"threadpool.h"

#include<thread>
#include<iostream>

const int maxTaskQueThreshHold = 1024;
const int THREAD_MAX_THRESH_HOLD = 10;
const int THREAD_MAX_IDLE_TIME = 60;

/*--------------�̳߳��ಿ�ֶ���----------------*/
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
}// �����̳߳�ģʽ

void ThreadPool::setTaskQueueMaxThreshHold(int maxThreshHold) {
	taskQueueMaxThreshHold_ = maxThreshHold;
} // �����������������ֵ

void ThreadPool::setThreadSizeThreshHold(int max_thresh_hold) {
	if (checkRunningState()) {
		return;
	}
	if (poolMode_ == PoolMode::MODE_CACHED) {
		threadSizeThreshHold_ = max_thresh_hold;
	}
} // �����̳߳�cachedģʽ�µ��߳�������ֵ

Result ThreadPool::submitTask(std::shared_ptr<Task> task) {
	// ��ȡ��
	std::unique_lock<std::mutex> lock(taskQueueMtx_); // �̵߳�ͨ�š� �ȴ���������п���

	// �ȴ���������п��࣬ ���������������У� ��notEmpty_�Ͻ���֪ͨ
	// �û��ύ������������ܳ���1s
	if (!NotFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool { return taskQueue_.size() < taskQueueMaxThreshHold_; })) {
		// ��ʾ�ȴ�ʱ���Ѿ�����1s�� ������Ȼû�б�����
		std::cerr << "Task queue is full, submit task failed." << std::endl;
		return Result(task, false);
	}

	taskQueue_.emplace(task);
	taskSize_++;

	NotEmpty_.notify_all();  // ֪ͨ������ -�� �̳߳�

	// cachedģʽ ������ȽϽ��� ������ С���������
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < threadSizeThreshHold_) {
		// �������߳�
		std::cout << ">>> create new thread! " << std::endl;
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		//threads_.emplace_back(std::move(ptr));
		curThreadSize_++;
	}

	return Result(task);

}// �ύ����, �û����øýӿڣ��������� =�� ������

void ThreadPool::start(int initthreadsize) {
	isPoolRunning_ = true;
	// ��¼��ʼ�߳���
	initThreadSize_ = initthreadsize;
	curThreadSize_ = initthreadsize;

	// �����̶߳���
	for (int i = 0; i < initThreadSize_; i++) {
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		// threads_.emplace_back(std::move(ptr));
		
	}

	// �����߳�
	for (int i = 0; i < initThreadSize_; i++) {
		threads_[i]->start(); // ��Ҫȥִ��һ���̺߳���
		idleThreadSize_++; // ��¼��ʼ�����̵߳�����
	}

}// �����̳߳�

void ThreadPool::threadFunc(int threadid) {

	auto lastTime = std::chrono::high_resolution_clock().now();
	
	for (;;) {
		std::shared_ptr<Task> task;
		{ //Ϊ�˼�ʱ�ͷ����� �ص��½�һ��������
			// �Ȼ�ȡ��
			std::unique_lock<std::mutex> lock(taskQueueMtx_);

			std::cout << "tid: " << std::this_thread::get_id() << "���Ի�ȡ���񡣡���" << std::endl;

			// cachedģʽ�£��������ǰʱ�� - ��һ���߳�ִ�е�ʱ�� > 60s���� ��Ҫ�Զ�����߳̽��л���
			if (poolMode_ == PoolMode::MODE_CACHED) {
				// ÿһ���з���һ��
				while (taskQueue_.size() > 0) {
					// ���������� ��ʱ������
					if (std::cv_status::timeout == NotEmpty_.wait_for(lock, std::chrono::seconds(1))) {
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME
							&& curThreadSize_ > initThreadSize_) {
							// ��ʼ���յ�ǰ�߳�
							// ��¼�߳���������ر�����ֵ�޸�
							// ���̶߳�����߳��б�������ɾ��  threadid -> thread����
							threads_.erase(threadid);
							curThreadSize_--;
							idleThreadSize_--;

							std::cout << "threadid:" << std::this_thread::get_id() << "exit!" << std::endl;
							return;
						}
					}
				}
			}


			// �ȴ�notEmpty����
			// �����������ȡһ�������������ǰ�̸߳���ִ���������
			NotEmpty_.wait(lock, [&]()->bool {return taskQueue_.size() > 0; });
			idleThreadSize_--;

			std::cout << "tid: " << std::this_thread::get_id() << "��ȡ����ɹ�������" << std::endl;

			task = taskQueue_.front();
			taskQueue_.pop();
			taskSize_--;

			// �ж϶����Ƿ�Ϊ�գ� ֪ͨ�����߳̽�������
			if (taskSize_ > 0) {
				NotEmpty_.notify_all();
			}

			// ��ʱ���п϶������� ֪ͨsubmitTask���Լ���������������
			NotFull_.notify_all();

		} // �ͷ�unique_lock

		// ִ������
		if (task != nullptr) {
			// task->run();   // 1. ִ������  2. ������ķ���ֵͨ��setVal��������Result
			task->exec();
 		}
		lastTime = std::chrono::high_resolution_clock().now(); // �����߳�ִ���������ʱ��
		idleThreadSize_++;
	}

}// �����̺߳���

bool ThreadPool::checkRunningState() const {
	return isPoolRunning_;
}


/*-----------------�߳��ಿ�ֶ���--------------------*/

int Thread::generateId_ = 0;

Thread::Thread(ThreadFunc func) 
	: func_(func)
	, threadId_(generateId_++)
{}

Thread::~Thread()
{}

// �����߳�
void Thread::start() {
	// ����һ���߳���ִ��һ���̺߳���
	std::thread t(func_, threadId_);  // �̶߳���t �� �̺߳���func_
	t.detach();  // ���÷����߳�  �൱��Linux�е�pthread_detach

}

int Thread::getId() const {
	return threadId_;
}

/*-----------------  Task����ʵ�� --------------------*/
Task::Task() : result_(nullptr)
{}

void Task::exec() {

	if (result_ != nullptr) {
		result_->setVal(run()); // ���﷢����̬����
	}
}

void Task::setResult(Result* res) {
	result_ = res;
}



// ----------------- Result������ʵ�� ---------------------------
Result::Result(std::shared_ptr<Task> task, bool isValid) 
	: task_(task), isValid_(isValid)
{
	task_->setResult(this);
}

Any Result::get() {
	if (!isValid_) {
		return "";
	}
	sem_.wait(); // task�������û��ִ���꣬�������������û����߳�
	return std::move(any_);
}

void Result::setVal(Any any) {
	// �洢task�ķ���ֵ
	this->any_ = std::move(any);
	sem_.post();  // �Ѿ���ȡ������ķ���ֵ�������ź�����Դ 
}
