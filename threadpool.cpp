#include"threadpool.h"

#include<thread>
#include<iostream>

const int maxTaskQueThreshHold = 1024;

/*--------------�̳߳��ಿ�ֶ���----------------*/
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
}// �����̳߳�ģʽ

void ThreadPool::setTaskQueueMaxThreshHold(int maxThreshHold) {
	taskQueueMaxThreshHold_ = maxThreshHold;
} // �����������������ֵ

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
	return Result(task);

}// �ύ����, �û����øýӿڣ��������� =�� ������

void ThreadPool::start(int initthreadsize) {
	// ��¼��ʼ�߳���
	initThreadSize_ = initthreadsize;

	// �����̶߳���
	for (int i = 0; i < initThreadSize_; i++) {
		auto ptr = std::make_unique<Thread>(Thread(std::bind(&ThreadPool::threadFunc, this)));
		threads_.emplace_back(std::move(ptr));
	}

	// �����߳�
	for (int i = 0; i < initThreadSize_; i++) {
		threads_[i]->start();
	}

}// �����̳߳�

void ThreadPool::threadFunc() {
	/*std::cout << "Begin threadFunc: " << std::this_thread::get_id() << " starts\n";
	std::cout << std::this_thread::get_id() << "ends." << std::endl;*/

	std::shared_ptr<Task> task;
	for (;;) {
		{ //Ϊ�˼�ʱ�ͷ����� �ص��½�һ��������
			// �Ȼ�ȡ��
			std::unique_lock<std::mutex> lock(taskQueueMtx_);

			std::cout << "tid: " << std::this_thread::get_id() << "���Ի�ȡ���񡣡���" << std::endl;

			// �ȴ�notEmpty����
			// �����������ȡһ�������������ǰ�̸߳���ִ���������
			NotEmpty_.wait(lock, [&]()->bool {return taskQueue_.size() > 0; });

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

	}

}// �����̺߳���


/*-----------------�߳��ಿ�ֶ���--------------------*/

Thread::Thread(ThreadFunc func) 
	: func_(func)
{}

Thread::~Thread()
{}

// �����߳�
void Thread::start() {
	// ����һ���߳���ִ��һ���̺߳���
	std::thread t(func_);  // �̶߳���t �� �̺߳���func_
	t.detach();  // ���÷����߳�  �൱��Linux�е�pthread_detach

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
