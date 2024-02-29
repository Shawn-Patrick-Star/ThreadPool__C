#ifndef _THREADPOOL_H
#define _THREADPOOL_H
#include <stdlib.h>
#include <string.h>

#define ADD_NUM 2
#define DEL_NUM 2
#define DEBUG 1

#if DEBUG
#define Log(fmt, ...) \
        printf("[FILE:%s] [FUNC:%s] [LINE:%d]  " fmt, \
                __FILE__, __FUNCTION__ ,__LINE__, __VA_ARGS__)

#else
#define Log(fmt, ...)
#endif

typedef struct Task Task;
typedef struct ThreadPool ThreadPool;
// public:
ThreadPool* threadPoolCreate(int min, int max, int queueCapacity);
int threadPoolDestroy(ThreadPool* pool);

void add_task(ThreadPool* pool, void(*func)(void*), void* arg); // 添加任务（函数）（生产者）

int Get_aliveNum(ThreadPool* pool); // 获取当前存活的线程数
int Get_busyNum(ThreadPool* pool); // 获取当前 忙（处理任务）的线程数

// private:
void* worker(void* arg); // 工作者（消费者）
void* manager(void* arg); // 管理者

void product(ThreadPool* pool, void(*func)(void*), void* arg);
void consume(ThreadPool* pool, Task* task);

void threadExit(ThreadPool* pool); // 线程退出


#endif  // _THREADPOOL_H