#include "Thread_pool.h"
#include <pthread.h>
#include <iostream>
#include <unistd.h>


typedef struct Task
{
    void (*function)(void* arg);
    void* arg;
}Task;


struct ThreadPool
{
    // 任务队列
    Task* taskQueue;
    int queueCapacity;  // 容量
    int taskNum;       // 当前任务个数
    int queueFront;     // 队头 -> 放数据
    int queueRear;      // 队尾 -> 取数据
    // 管理者线程
    pthread_t managerID;    
    // 工作线程组
    pthread_t* threadIDs;   
    int minNum;             // 最小线程数量
    int maxNum;             // 最大线程数量
    int busyNum;            // 忙的线程个数
    int aliveNum;            // 存活的线程个数
    int exitNum;            // 要销毁的线程个数

    pthread_mutex_t mutexPool;  // 锁整个线程池
    pthread_mutex_t mutexBusy;  // 锁busyNum变量
    pthread_cond_t notFull_cond;     // 任务队列是否满
    pthread_cond_t notEmpty_cond;     // 任务队列是否空
    
    bool shutdown;           // 是否销毁线程池


};

ThreadPool* threadPoolCreate(int min, int max, int queueCapacity){
    // 开辟线程池内存
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    do{
        if(pool == NULL){
            std::cout << "malloc threadpool fail...\n";
            break;
        }
        // 开辟工作线程组内存
        pool->threadIDs = (pthread_t*)malloc(max * sizeof(pthread_t));
        if(pool->threadIDs == NULL){
            std::cout << "malloc threadIDs fail...\n";
            break;
        }
        
        // 初始化
        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;
        pool->aliveNum = min;
        pool->exitNum = 0;
        
        if(pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
           pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
           pthread_cond_init(&pool->notEmpty_cond, NULL) != 0 ||
           pthread_cond_init(&pool->notFull_cond, NULL) != 0){
            printf("initial mutex or cond fail...\n");
            break;
        } 

        // 开辟任务队列内存
        pool->taskQueue = (Task*)malloc(queueCapacity * sizeof(Task));
        if(pool->taskQueue == NULL){
            std::cout << "malloc taskQueue fail...\n";
            break;
        }
        // 初始化任务队列
        pool->queueCapacity = queueCapacity;
        pool->taskNum = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;

        pool->shutdown = 0;

        // 创建管理者线程
        pthread_create(&pool->managerID, NULL, manager, pool);
        // 创建工作线程
        for(int i = 0; i < min; i++){
            pthread_create(&pool->threadIDs[i], NULL, worker, pool);
        }

        return pool;
    }while(0);// 使用do-while 为了当内存分配失败时用break退出 统一释放内存 
    if(pool && pool->threadIDs) free(pool->threadIDs);
    if(pool && pool->taskQueue) free(pool->taskQueue);
    free(pool);

    return NULL;
}



void* worker(void* arg){
    ThreadPool* pool = (ThreadPool*)arg;
    while(1){
        pthread_mutex_lock(&pool->mutexPool);
        // 判断当前任务队列是否为空
        while(pool->taskNum == 0 && !pool->shutdown){
            Log("%s\n", "worker wait");
            // 等待add_task信号
            pthread_cond_wait(&pool->notEmpty_cond, &pool->mutexPool);
            // 判断是否需要销毁线程 没测试过
            while(pool->exitNum > 0){
                pool->exitNum--;
                if(pool->aliveNum > pool->minNum){
                    pool->aliveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);// 把锁释放 避免死锁
                    threadExit(pool);
                }
            }
        }
        if(pool->shutdown){ // 判断线程池是否关闭
            Log("threadID: %ld over\n", pthread_self());
            pool->aliveNum--;
            Log("aliveNum: %d\n", Get_aliveNum(pool));
            pthread_mutex_unlock(&pool->mutexPool);
            pthread_exit(NULL);
        }
        // 从任务队列中取出一个任务
        Task* task = (Task*)malloc(sizeof(Task));
        consume(pool, task);

        pthread_cond_broadcast(&pool->notFull_cond); // 唤醒生产者
        pthread_mutex_unlock(&pool->mutexPool);

        Log("threadID: %ld start working...\n", pthread_self());
        
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexBusy);

        // 执行任务函数
        task->function(task->arg);
        free(task->arg);task->arg = NULL;
        free(task);task = NULL;
        
        Log("threadID: %ld end working...\n", pthread_self());

        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);
    }
    return NULL;
}

void* manager(void* arg){
    ThreadPool* pool = (ThreadPool*)arg;
    while(!pool->shutdown){
        // 每隔3s进行一次检查
        sleep(3);
        // 获取当前存活的线程数 任务数 忙线程数
        pthread_mutex_lock(&pool->mutexPool);
        int aliveNum = pool->aliveNum;
        int busyNum = pool->busyNum;
        int taskNum = pool->taskNum;
        pthread_mutex_unlock(&pool->mutexPool);
        
        // 线程过少 添加线程
        if(taskNum > aliveNum && aliveNum < pool->maxNum){
            pthread_mutex_lock(&pool->mutexPool);
            int counter = 0;
            for (int i = 0; i < pool->maxNum && counter < ADD_NUM && pool->aliveNum < pool->maxNum; i++){
                if (pool->threadIDs[i] == 0){
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                    counter++;
                    pool->aliveNum++;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);

        }
        // 线程过多 销毁线程
        if(busyNum * 2 < aliveNum && aliveNum > pool->minNum){
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = DEL_NUM;
            pthread_mutex_unlock(&pool->mutexPool);
            // 引导工作线程自杀
            for(int i = 0; i < DEL_NUM; i++){
                pthread_cond_broadcast(&pool->notEmpty_cond);
            }
        }
        Log("aliveNum: %d\n", Get_aliveNum(pool));
        // pthread_mutex_lock(&pool->mutexPool);
        Log("shutdown: %d\n", pool->shutdown);
        // pthread_mutex_unlock(&pool->mutexPool);
    }

    // 线程池关闭 回收worker线程资源 唤醒阻塞的消费者线程
    for(int i = 0; i < pool->aliveNum; i++){
        pthread_cond_broadcast(&pool->notEmpty_cond);
    }
    for(int i = 0; i < pool->maxNum; i++){
        if(pool->threadIDs[i] != 0){
            pthread_join(pool->threadIDs[i], NULL);
        }
    }
    
    pthread_exit(NULL);
}

void threadExit(ThreadPool* pool){
    pthread_t tid = pthread_self();
    for(int i = 0; i < pool->maxNum; i++){
        if(pool->threadIDs[i] == tid){
            pool->threadIDs[i] = 0;
            printf("threadExit() called, ID:%ld exiting...\n", tid);
            break;
        }
    }
    pthread_exit(NULL);
}

void add_task(ThreadPool* pool, void(*func)(void*), void* arg){
    if(pool == NULL){
        std::cout << "pool isn't initialized , add_task fail\n";
        return;
    }
    pthread_mutex_lock(&pool->mutexPool);
    // 队列满时 等待消费者唤醒
    while(pool->taskNum == pool->queueCapacity && !pool->shutdown){
        pthread_cond_wait(&pool->notFull_cond, &pool->mutexPool);
    }
    // 若线程池关闭 直接return
    if(pool->shutdown){
        pthread_mutex_unlock(&pool->mutexPool);
        std::cout << "pool is close, add_task fail\n";
        return;
    }
    // 添加任务
    product(pool, func, arg);
    pthread_cond_broadcast(&pool->notEmpty_cond); // 唤醒worker
    pthread_mutex_unlock(&pool->mutexPool);
}

int Get_aliveNum(ThreadPool *pool){
    if(pool == NULL) return 0;
    int aliveNum = 0;
    if(pthread_mutex_trylock(&pool->mutexPool) == 0){
        aliveNum = pool->aliveNum;
        pthread_mutex_unlock(&pool->mutexPool);
    }else{
        aliveNum = pool->aliveNum;
    }
    return aliveNum;
}

int Get_busyNum(ThreadPool* pool){
    if(pool == NULL) return 0;
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}

int threadPoolDestroy(ThreadPool* pool){
    if(pool == NULL) return -1;
    // 关闭线程池
    pool->shutdown = 1;
    // 回收管理者线程
    pthread_join(pool->managerID, NULL);
    // 释放堆内存
    if(pool && pool->threadIDs) free(pool->threadIDs);
    if(pool && pool->taskQueue) free(pool->taskQueue);
    
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_mutex_destroy(&pool->mutexPool);
    pthread_cond_destroy(&pool->notFull_cond);
    pthread_cond_destroy(&pool->notEmpty_cond);
    
    free(pool);
    pool = NULL;
    return 0;
}

void product(ThreadPool *pool, void (*func)(void *), void *arg){
    /* 这里暂时不添加各种判断 比如 线程池是否被创建 
                                  队列是否已满      */ 
    pool->taskQueue[pool->queueRear].function = func;
    pool->taskQueue[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
    pool->taskNum++;
}

void consume(ThreadPool* pool, Task* task){
    /* 这里暂时不添加各种判断 比如 线程池是否被创建 
                                  队列是否已空      */ 
    task->function = pool->taskQueue[pool->queueFront].function;
    task->arg = pool->taskQueue[pool->queueFront].arg;
    pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
    pool->taskNum--;
}