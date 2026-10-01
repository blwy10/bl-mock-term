// mtcore_process.h                                                   -*-C++-*-
#ifndef INCLUDED_MTCORE_PROCESS
#define INCLUDED_MTCORE_PROCESS

//@PURPOSE: Launch and supervise child processes.
//
//@CLASSES:
//  mtcore::ChildProcess:  a spawned process tied to the parent's lifetime
//
//@DESCRIPTION: Each child is placed in a Win32 job object configured with
// 'JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE', so it is terminated when the parent
// exits for any reason (including a crash).

#include <mtcore_platform.h>

#include <string>

namespace mtcore {

class ChildProcess {
    HANDLE d_process;
    HANDLE d_job;
    DWORD  d_pid;

  private:
    // NOT IMPLEMENTED
    ChildProcess(const ChildProcess&);
    ChildProcess& operator=(const ChildProcess&);

  public:
    ChildProcess();
    ~ChildProcess();
        // Terminate the child if it is still running.

    int start(const std::string& commandLine,
              const std::string& workingDirectory,
              bool               hideWindow);
        // Return 0 on success, or the Win32 error code.

    bool isRunning() const;
    int wait(int timeoutMs);
        // Return the exit code, or -1 on timeout.
    void terminate();
    void release();
        // Forget the child without terminating it.
    unsigned long pid() const;

    static std::string moduleDirectory();
        // Directory containing the current executable, with trailing '\'.
    static std::string quote(const std::string& argument);
};

}  // close namespace mtcore

#endif
