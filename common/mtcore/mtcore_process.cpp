// mtcore_process.cpp                                                 -*-C++-*-
#include <mtcore_process.h>

#include <mtcore_log.h>

#include <string.h>
#include <vector>

namespace mtcore {

ChildProcess::ChildProcess()
: d_process(NULL)
, d_job(NULL)
, d_pid(0)
{
}

ChildProcess::~ChildProcess()
{
    terminate();
}

int ChildProcess::start(const std::string& commandLine,
                        const std::string& workingDirectory,
                        bool               hideWindow)
{
    terminate();

    d_job = ::CreateJobObjectA(NULL, NULL);
    if (d_job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
        memset(&info, 0, sizeof(info));
        info.BasicLimitInformation.LimitFlags =
                                          JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        ::SetInformationJobObject(d_job,
                                  JobObjectExtendedLimitInformation,
                                  &info,
                                  sizeof(info));
    }

    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    std::vector<char> cmd(commandLine.begin(), commandLine.end());
    cmd.push_back(0);
    DWORD flags = CREATE_SUSPENDED;
    if (hideWindow) {
        flags |= CREATE_NO_WINDOW;
    }
    BOOL ok = ::CreateProcessA(NULL,
                               &cmd[0],
                               NULL,
                               NULL,
                               FALSE,
                               flags,
                               NULL,
                               workingDirectory.empty()
                                   ? NULL
                                   : workingDirectory.c_str(),
                               &si,
                               &pi);
    if (!ok) {
        int error = (int)::GetLastError();
        MT_LOG_ERROR("CreateProcess failed (%d): %s",
                     error,
                     commandLine.c_str());
        if (d_job) {
            ::CloseHandle(d_job);
            d_job = NULL;
        }
        return error;
    }
    if (d_job && !::AssignProcessToJobObject(d_job, pi.hProcess)) {
        MT_LOG_WARN("AssignProcessToJobObject failed (%lu)",
                    ::GetLastError());
    }
    ::ResumeThread(pi.hThread);
    ::CloseHandle(pi.hThread);
    d_process = pi.hProcess;
    d_pid     = pi.dwProcessId;
    MT_LOG_INFO("started pid %lu: %s", d_pid, commandLine.c_str());
    return 0;
}

bool ChildProcess::isRunning() const
{
    return d_process != NULL
        && ::WaitForSingleObject(d_process, 0) == WAIT_TIMEOUT;
}

int ChildProcess::wait(int timeoutMs)
{
    if (d_process == NULL) {
        return -1;
    }
    DWORD rc = ::WaitForSingleObject(d_process,
                                     timeoutMs < 0 ? INFINITE
                                                   : (DWORD)timeoutMs);
    if (rc != WAIT_OBJECT_0) {
        return -1;
    }
    DWORD code = 0;
    ::GetExitCodeProcess(d_process, &code);
    return (int)code;
}

void ChildProcess::terminate()
{
    if (d_process) {
        if (::WaitForSingleObject(d_process, 0) == WAIT_TIMEOUT) {
            ::TerminateProcess(d_process, 1);
            ::WaitForSingleObject(d_process, 5000);
        }
        ::CloseHandle(d_process);
        d_process = NULL;
    }
    if (d_job) {
        ::CloseHandle(d_job);
        d_job = NULL;
    }
    d_pid = 0;
}

void ChildProcess::release()
{
    if (d_process) {
        ::CloseHandle(d_process);
        d_process = NULL;
    }
    d_job = NULL;   // leak the job handle deliberately: closing kills child
    d_pid = 0;
}

unsigned long ChildProcess::pid() const
{
    return d_pid;
}

std::string ChildProcess::moduleDirectory()
{
    char path[MAX_PATH];
    DWORD n = ::GetModuleFileNameA(NULL, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return std::string();
    }
    std::string result(path, n);
    size_t slash = result.find_last_of("\\/");
    return slash == std::string::npos ? std::string()
                                      : result.substr(0, slash + 1);
}

std::string ChildProcess::quote(const std::string& argument)
{
    if (!argument.empty()
     && argument.find_first_of(" \t\"") == std::string::npos) {
        return argument;
    }
    std::string result = "\"";
    for (size_t i = 0; i < argument.size(); ++i) {
        if (argument[i] == '"') {
            result += '\\';
        }
        result += argument[i];
    }
    result += '"';
    return result;
}

}  // close namespace mtcore
