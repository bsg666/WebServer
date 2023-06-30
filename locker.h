#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <exception>
#include <semaphore.h>
// 线程同步机制封装类

// 互斥锁类
class locker
{
private:    
    pthread_mutex_t m_mutex;            // 互斥量类型
public:
    locker(){
        if(pthread_mutex_init(&m_mutex, NULL) != 0){    // 创建
            throw std::exception();
        }
    }

    ~locker(){
        pthread_mutex_destroy(&m_mutex);                // 释放
    }

    bool lock(){
        return pthread_mutex_lock(&m_mutex) == 0;       // 上锁
    }

    bool unlock(){
        return pthread_mutex_unlock(&m_mutex) == 0;     // 解锁
    }

    pthread_mutex_t* get(){
        return &m_mutex;
    }
};

// 条件变量类
class cond{
private:
    pthread_cond_t m_cond;              // 条件变量类型
public:
    cond(){
        if(pthread_cond_init(&m_cond, NULL) != 0){      // 初始化
            throw std::exception();
        }
    }

    ~cond(){
        pthread_cond_destroy(&m_cond);                  // 释放
    }

    bool wait(pthread_mutex_t* mutex){              
        return pthread_cond_wait(&m_cond, mutex) == 0;  // 等待
    }
 
    bool timedwait(pthread_mutex_t* mutex, struct timespec t){  // 在一定时间等待
        return pthread_cond_timedwait(&m_cond, mutex, &t) == 0;
    }

    bool signal(){
        return pthread_cond_signal(&m_cond) == 0;       // 唤醒一个/多个线程
    }

    bool broadcast(){
        return pthread_cond_broadcast(&m_cond) == 0;    // 唤醒所有线程
    }
};

// 信号量类
class sem
{
private:
    sem_t m_sem;
public:
    sem(){
        if(sem_init(&m_sem, 0, 0) != 0){    // 信号量值初始化为0 （消费者信号量）
            throw std::exception();
        }
    }

    sem(int num){           // 传参创建
        if(sem_init(&m_sem, 0, num) != 0){
            throw std::exception();
        }
    }
    
    ~sem(){                 // 释放
        sem_destroy(&m_sem);
    }

    bool wait(){            // 等待（减少）信号量
        return sem_wait(&m_sem) == 0;
    }

    bool post(){            // 增加信号量
        return sem_post(&m_sem) == 0;
    }
};


#endif