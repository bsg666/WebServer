#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include "locker.h"
#include <cstdio>

// 线程池类，定义成模板类，为了代码的复用，模板参数T是任务类
template<typename T>
class threadpool
{
private:
    int m_thread_num;               // 线程数量
    pthread_t * m_threads;          // 线程池数组，大小为m_thread_num，声明为指针，后面动态创建数组
    int m_max_requests;             // 请求队列中的最大等待数量
    std::list<T*> m_workqueue;      // 请求队列
    locker m_queue_locker;          // 互斥锁
    sem m_queue_stat;               // 信号量
    bool m_stop;                    // 是否结束线程，线程根据该值判断是否要停止

    static void* worker(void* arg); // 静态函数，线程调用，不能访问非静态成员
    void run();                     // 线程池已启动，执行函数

public:
    threadpool(int thread_num = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T* request);    // 添加任务的函数
};


template<typename T>
threadpool<T>::threadpool(int thread_num, int max_requests) :   // 构造函数，初始化
        m_thread_num(thread_num), m_max_requests(max_requests),
        m_stop(false), m_threads(NULL)
{
    if(thread_num <= 0 || max_requests <= 0){
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_num];    // 动态分配，创建线程池数组
    if(!m_threads){
        throw std::exception();
    }

    for(int i = 0; i < thread_num; ++i){
        printf("creating the N0.%d thread.\n", i);

        // 创建线程, worker（线程函数） 必须是静态的函数
        if(pthread_create(m_threads + i, NULL, worker, this) != 0){     // 通过最后一个参数向 worker 传递 this 指针，来解决静态函数无法访问非静态成员的问题
            delete [] m_threads;        // 创建失败，则释放数组空间，并抛出异常
            throw std::exception();
        }

        // 设置线程分离，结束后自动释放空间
        if(pthread_detach(m_threads[i])){
            delete [] m_threads;
            throw std::exception();
        }  
    }
}

template<typename T>
threadpool<T>::~threadpool(){       // 析构函数
    delete [] m_threads;            // 释放线程数组空间
    m_stop = true;                  // 标记线程结束
}

template<typename T>
bool threadpool<T>::append(T* request){     // 添加请求队列
      m_queue_locker.lock();                // 队列为共享队列，上锁
      if(m_workqueue.size() > m_max_requests){
        m_queue_locker.unlock();            // 队列元素已满
        return false;                       // 添加失败
      }

      m_workqueue.push_back(request);       // 将任务加入队列
      m_queue_locker.unlock();              // 解锁
      m_queue_stat.post();                  // 增加信号量，线程根据信号量判断阻塞还是继续往下执行
      return true;
}

template<typename T>
void* threadpool<T>::worker(void* arg){     // arg 为线程创建时传递的threadpool类的 this 指针参数
    threadpool* pool = (threadpool*) arg;
    pool->run();    // 线程实际执行函数
    return pool;    // 无意义
}

template<typename T>
void threadpool<T>::run(){              // 线程实际执行函数
    while(!m_stop){                     // 判断停止标记
        m_queue_stat.wait();            // 等待信号量有数值（减一）
        m_queue_locker.lock();          // 上锁
        if(m_workqueue.empty()){        // 空队列
            m_queue_locker.unlock();    // 解锁
            continue;
        }

        T* request = m_workqueue.front();   // 取出任务
        m_workqueue.pop_front();            // 移出队列
        m_queue_locker.unlock();            // 解锁
        if(!request){
            continue;
        }

        request->process();             // 任务类 T 的执行函数
    }

}

#endif