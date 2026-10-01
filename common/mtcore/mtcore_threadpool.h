// mtcore_threadpool.h                                                -*-C++-*-
#ifndef INCLUDED_MTCORE_THREADPOOL
#define INCLUDED_MTCORE_THREADPOOL

//@PURPOSE: Provide a fixed-size pool of worker threads executing 'Job's.
//
//@CLASSES:
//  mtcore::Job:         protocol for a unit of work (deleted after running)
//  mtcore::ThreadPool:  FIFO job queue drained by N worker threads

#include "mtcore_mutex.h"
#include "mtcore_thread.h"

#include <deque>
#include <vector>

namespace mtcore {

class Job {
  public:
    virtual ~Job();
    virtual void execute() = 0;
};

class ThreadPool {
    typedef std::deque<Job *>     JobQueue;
    typedef std::vector<Thread *> ThreadList;

    Mutex                     d_mutex;
    HANDLE                    d_semaphore;
    JobQueue                  d_queue;      // owned
    ThreadList                d_threads;    // owned
    MemberRunnable<ThreadPool> d_runnable;
    volatile bool             d_stopping;
    AtomicInt                 d_executed;

  private:
    void workerLoop();

    // NOT IMPLEMENTED
    ThreadPool(const ThreadPool&);
    ThreadPool& operator=(const ThreadPool&);

  public:
    ThreadPool();
    ~ThreadPool();

    int start(int numThreads);
    void stop();
        // Signal workers to exit, join them and delete queued jobs.

    bool enqueue(Job *job);
        // Take ownership of 'job' and schedule it.  Return 'false' (and
        // delete 'job') if the pool is stopping.

    int numThreads() const;
    int numPending();
    int numExecuted() const;
};

}  // close namespace mtcore

#endif
