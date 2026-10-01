// mtcore_thread.cpp                                                  -*-C++-*-
#include "mtcore_thread.h"

#include <process.h>

namespace mtcore {

Runnable::~Runnable()
{
}

                                // ------------
                                // class Thread
                                // ------------

unsigned int __stdcall Thread::entryPoint(void *arg)
{
    Thread *self = static_cast<Thread *>(arg);
    self->d_runnable_p->run();
    return 0;
}

Thread::Thread(const char *name)
: d_handle(NULL)
, d_threadId(0)
, d_runnable_p(NULL)
, d_ownsRunnable(false)
, d_name(name ? name : "")
{
}

Thread::~Thread()
{
    if (d_handle) {
        ::CloseHandle(d_handle);
        d_handle = NULL;
    }
    if (d_ownsRunnable) {
        delete d_runnable_p;
    }
}

int Thread::start(Runnable *runnable, bool takeOwnership)
{
    if (d_handle != NULL || runnable == NULL) {
        return -1;
    }
    d_runnable_p   = runnable;
    d_ownsRunnable = takeOwnership;
    uintptr_t h = _beginthreadex(NULL,
                                 0,
                                 &Thread::entryPoint,
                                 this,
                                 0,
                                 &d_threadId);
    if (h == 0) {
        return -2;
    }
    d_handle = (HANDLE)h;
    return 0;
}

int Thread::join(int timeoutMs)
{
    if (d_handle == NULL) {
        return 0;
    }
    if (isCurrentThread()) {
        return -1;
    }
    DWORD rc = ::WaitForSingleObject(d_handle,
                                     timeoutMs < 0 ? INFINITE
                                                   : (DWORD)timeoutMs);
    return rc == WAIT_OBJECT_0 ? 0 : 1;
}

bool Thread::isRunning() const
{
    if (d_handle == NULL) {
        return false;
    }
    return ::WaitForSingleObject(d_handle, 0) == WAIT_TIMEOUT;
}

bool Thread::isCurrentThread() const
{
    return d_handle != NULL && ::GetCurrentThreadId() == d_threadId;
}

unsigned int Thread::id() const
{
    return d_threadId;
}

const std::string& Thread::name() const
{
    return d_name;
}

void Thread::sleep(int milliseconds)
{
    ::Sleep((DWORD)milliseconds);
}

unsigned int Thread::currentId()
{
    return ::GetCurrentThreadId();
}

}  // close namespace mtcore
