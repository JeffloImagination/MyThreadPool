# 线程池项目
这是一个基于 C++17 的线程池实现，支持固定大小和动态缓存两种模式，提供了任务提交、结果获取和线程管理功能。
## 开发环境
- Visual Studio2022开发
- C++17标准

## 功能特性
- 支持两种线程池模式：固定大小(MODE_FIXED)和动态缓存(MODE_CACHED)
- 类型安全的任意类型(Any)存储和转换
- 任务结果异步获取机制(Result)
- 线程安全的任务队列管理
- 信号量(Semaphore)实现线程同步
- 任务队列阈值控制
- 禁止拷贝构造和赋值操作

## 核心代码简介

- **ThreadPool类**
线程池类： 负责创建线程池，在线程池中启动线程，提供用户提交任务接口，包含线程列表、任务队列等信息

- **Thread类**
线程类： 线程池中线程列表存储的类型，其中记录线程函数

- **Task类**
抽象任务基类： 定义了run接口，用户需要编写自己想实现的任务类MyTask并以这个Task类作为基类，需要实现子类的run方法

- **Any类**
任意类型容器类： 实现了存储任意数据类型数据和提取出存储的数据的方法

- **Result类**
任务结果类： 可以接收提交到线程池的task执行完成后的返回值类型

## 使用示例
### 1. 创建自定义任务
```cpp
class MyTask : public Task {
public:
    Any run()  {
        // 执行具体任务
        int result = a + b;
        return result;
    }
private:
  int a, b;
};
```

### 2. 使用线程池
```cpp
int main() {
    ThreadPool pool;
    pool.setPoolMode(MODE_CACHED); // 使用动态缓存模式
    pool.start(4); // 启动4个初始线程
    
    // 提交任务
    auto task = std::make_shared<MyTask>(1, 2);
    Result result = pool.submitTask(task);
    
    // 获取结果
    int value = result.get().cast_<int>();
    std::cout << "Result: " << value << std::endl;
    
    return 0;
}

```
