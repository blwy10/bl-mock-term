// mtcore_mutex.cpp                                                   -*-C++-*-
#include "mtcore_mutex.h"

namespace mtcore {

                                // -----------
                                // class Mutex
                                // -----------

Mutex::Mutex()
{
    ::InitializeCriticalSectionAndSpinCount(&d_cs, 4000);
}

Mutex::~Mutex()
{
    ::DeleteCriticalSection(&d_cs);
}

void Mutex::lock()
{
    ::EnterCriticalSection(&d_cs);
}

void Mutex::unlock()
{
    ::LeaveCriticalSection(&d_cs);
}

bool Mutex::tryLock()
{
    return ::TryEnterCriticalSection(&d_cs) != 0;
}

                                // ---------------
                                // class LockGuard
                                // ---------------

LockGuard::LockGuard(Mutex *mutex)
: d_mutex_p(mutex)
{
    d_mutex_p->lock();
}

LockGuard::~LockGuard()
{
    if (d_mutex_p) {
        d_mutex_p->unlock();
    }
}

void LockGuard::release()
{
    if (d_mutex_p) {
        d_mutex_p->unlock();
        d_mutex_p = NULL;
    }
}

                                // -----------
                                // class Event
                                // -----------

Event::Event(ResetMode mode)
{
    d_handle = ::CreateEvent(NULL,
                             mode == e_MANUAL_RESET ? TRUE : FALSE,
                             FALSE,
                             NULL);
}

Event::~Event()
{
    if (d_handle) {
        ::CloseHandle(d_handle);
    }
}

void Event::set()
{
    ::SetEvent(d_handle);
}

void Event::reset()
{
    ::ResetEvent(d_handle);
}

bool Event::wait(int timeoutMs)
{
    DWORD rc = ::WaitForSingleObject(d_handle,
                                     timeoutMs < 0 ? INFINITE
                                                   : (DWORD)timeoutMs);
    return rc == WAIT_OBJECT_0;
}

HANDLE Event::handle() const
{
    return d_handle;
}

                                // ---------------
                                // class AtomicInt
                                // ---------------

AtomicInt::AtomicInt(int value)
: d_value(value)
{
}

int AtomicInt::increment()
{
    return (int)::InterlockedIncrement(&d_value);
}

int AtomicInt::decrement()
{
    return (int)::InterlockedDecrement(&d_value);
}

int AtomicInt::add(int delta)
{
    return (int)::InterlockedExchangeAdd(&d_value, delta) + delta;
}

int AtomicInt::get() const
{
    return (int)::InterlockedCompareExchange(
                                       const_cast<volatile LONG *>(&d_value),
                                       0,
                                       0);
}

void AtomicInt::set(int value)
{
    ::InterlockedExchange(&d_value, value);
}

int AtomicInt::testAndSwap(int expected, int value)
{
    return (int)::InterlockedCompareExchange(&d_value, value, expected);
}

}  // close namespace mtcore
