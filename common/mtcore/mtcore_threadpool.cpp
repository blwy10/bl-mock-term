// mtcore_threadpool.cpp                                              -*-C++-*-
#include "mtcore_threadpool.h"

namespace mtcore {

Job::~Job()
{
}

ThreadPool::ThreadPool()
: d_semaphore(NULL)
, d_runnable(this, &ThreadPool::workerLoop)
, d_stopping(false)
{
    d_semaphore = ::CreateSemaphore(NULL, 0, 0x7fffffff, NULL);
}

ThreadPool::~ThreadPool()
{
    stop();
    ::CloseHandle(d_semaphore);
}

int ThreadPool::start(int numThreads)
{
    d_stopping = false;
    for (int i = 0; i < numThreads; ++i) {
        Thread *t = new Thread("pool");
        if (t->start(&d_runnable) != 0) {
            delete t;
            return -1;
        }
        d_threads.push_back(t);
    }
    return 0;
}

void ThreadPool::stop()
{
    if (d_threads.empty()) {
        return;
    }
    d_stopping = true;
    ::ReleaseSemaphore(d_semaphore, (LONG)d_threads.size(), NULL);
    for (ThreadList::iterator it = d_threads.begin();
         it != d_threads.end();
         ++it) {
        (*it)->join();
        delete *it;
    }
    d_threads.clear();

    LockGuard guard(&d_mutex);
    for (JobQueue::iterator it = d_queue.begin(); it != d_queue.end(); ++it) {
        delete *it;
    }
    d_queue.clear();
}

bool ThreadPool::enqueue(Job *job)
{
    if (d_stopping) {
        delete job;
        return false;
    }
    {
        LockGuard guard(&d_mutex);
        d_queue.push_back(job);
    }
    ::ReleaseSemaphore(d_semaphore, 1, NULL);
    return true;
}

void ThreadPool::workerLoop()
{
    for (;;) {
        ::WaitForSingleObject(d_semaphore, INFINITE);
        if (d_stopping) {
            return;
        }
        Job *job = NULL;
        {
            LockGuard guard(&d_mutex);
            if (!d_queue.empty()) {
                job = d_queue.front();
                d_queue.pop_front();
            }
        }
        if (job) {
            job->execute();
            delete job;
            d_executed.increment();
        }
    }
}

int ThreadPool::numThreads() const
{
    return (int)d_threads.size();
}

int ThreadPool::numPending()
{
    LockGuard guard(&d_mutex);
    return (int)d_queue.size();
}

int ThreadPool::numExecuted() const
{
    return d_executed.get();
}

}  // close namespace mtcore
