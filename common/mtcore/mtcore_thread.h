// mtcore_thread.h                                                    -*-C++-*-
#ifndef INCLUDED_MTCORE_THREAD
#define INCLUDED_MTCORE_THREAD

//@PURPOSE: Provide a Win32 thread wrapper and 'Runnable' protocol.
//
//@CLASSES:
//  mtcore::Runnable:        protocol for code run on a thread
//  mtcore::MemberRunnable:  adapts 'void T::f()' to 'Runnable'
//  mtcore::Thread:          '_beginthreadex' based thread handle
//
//@USAGE:
//..
//  class Pump {
//      mtcore::MemberRunnable<Pump> d_runnable;
//      mtcore::Thread               d_thread;
//      void loop();
//    public:
//      Pump() : d_runnable(this, &Pump::loop) {}
//      void start() { d_thread.start(&d_runnable); }
//  };
//..

#include "mtcore_platform.h"

#include <string>

namespace mtcore {

class Runnable {
  public:
    virtual ~Runnable();
    virtual void run() = 0;
};

template <class TYPE>
class MemberRunnable : public Runnable {
  public:
    typedef void (TYPE::*Method)();

  private:
    TYPE   *d_object_p;
    Method  d_method;

  public:
    MemberRunnable(TYPE *object, Method method)
    : d_object_p(object)
    , d_method(method)
    {
    }

    virtual void run()
    {
        (d_object_p->*d_method)();
    }
};

class Thread {
    HANDLE       d_handle;
    unsigned int d_threadId;
    Runnable    *d_runnable_p;      // owned iff 'd_ownsRunnable'
    bool         d_ownsRunnable;
    std::string  d_name;

  private:
    static unsigned int __stdcall entryPoint(void *arg);

    // NOT IMPLEMENTED
    Thread(const Thread&);
    Thread& operator=(const Thread&);

  public:
    explicit Thread(const char *name = "");
    ~Thread();
        // Destroy this object.  The thread is *not* joined; call 'join'
        // first if the thread may still be running.

    int start(Runnable *runnable, bool takeOwnership = false);
        // Begin executing 'runnable->run()' on a new thread.  Return 0 on
        // success and a non-zero value otherwise.

    int join(int timeoutMs = -1);
        // Wait for the thread to finish.  Return 0 if the thread finished.
        // Joining the calling thread itself returns -1 immediately.

    bool isRunning() const;
    bool isCurrentThread() const;
    unsigned int id() const;
    const std::string& name() const;

    static void sleep(int milliseconds);
    static unsigned int currentId();
};

}  // close namespace mtcore

#endif
