// mtcore_mutex.h                                                     -*-C++-*-
#ifndef INCLUDED_MTCORE_MUTEX
#define INCLUDED_MTCORE_MUTEX

//@PURPOSE: Provide a recursive mutex, scoped guard, event and atomic counter.
//
//@CLASSES:
//  mtcore::Mutex:      wrapper around a Win32 CRITICAL_SECTION
//  mtcore::LockGuard:  scoped lock for 'Mutex'
//  mtcore::Event:      Win32 event (manual- or auto-reset)
//  mtcore::AtomicInt:  interlocked 32-bit counter

#include "mtcore_platform.h"

namespace mtcore {

class Mutex {
    CRITICAL_SECTION d_cs;

  private:
    // NOT IMPLEMENTED
    Mutex(const Mutex&);
    Mutex& operator=(const Mutex&);

  public:
    Mutex();
    ~Mutex();

    void lock();
    void unlock();
    bool tryLock();
};

class LockGuard {
    Mutex *d_mutex_p;  // held, not owned

  private:
    // NOT IMPLEMENTED
    LockGuard(const LockGuard&);
    LockGuard& operator=(const LockGuard&);

  public:
    explicit LockGuard(Mutex *mutex);
    ~LockGuard();

    void release();
        // Unlock the mutex early.  The destructor then does nothing.
};

class Event {
    HANDLE d_handle;

  private:
    // NOT IMPLEMENTED
    Event(const Event&);
    Event& operator=(const Event&);

  public:
    enum ResetMode { e_AUTO_RESET = 0, e_MANUAL_RESET = 1 };

    explicit Event(ResetMode mode = e_AUTO_RESET);
    ~Event();

    void set();
    void reset();

    bool wait(int timeoutMs);
        // Return 'true' if the event was signalled before 'timeoutMs'
        // elapsed.  A negative 'timeoutMs' waits forever.

    HANDLE handle() const;
};

class AtomicInt {
    volatile LONG d_value;

  private:
    // NOT IMPLEMENTED
    AtomicInt(const AtomicInt&);
    AtomicInt& operator=(const AtomicInt&);

  public:
    explicit AtomicInt(int value = 0);

    int increment();   // returns the new value
    int decrement();   // returns the new value
    int add(int delta);
    int get() const;
    void set(int value);
    int testAndSwap(int expected, int value);  // returns the old value
};

}  // close namespace mtcore

#endif
