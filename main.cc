#include "Thread_pool.h"
#include <iostream>
#include <unistd.h>

void test(void* arg){
    int num = *(int*)arg;
    printf("thread is working, num = %d, tid = %ld\n", num, pthread_self());
    usleep(1000);
    // sleep(1);
}

int main(){

    // 创建线程池
    ThreadPool* pool = threadPoolCreate(3, 10, 100);
    for(int i = 0; i < 50; i++){
        int* num = (int*)malloc(sizeof(int));
        *num = i + 50;
        add_task(pool, test, num);
    }

    sleep(10);
    int state = threadPoolDestroy(pool);
    if(state == 0)printf("success destroy\n");
    return 0;
} 