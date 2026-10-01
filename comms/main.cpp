// main.cpp (mtcomm)                                                  -*-C++-*-
//
// Local communications process: the MockTerm analogue of a terminal vendor's
// local comms daemon.  Holds one upstream session to mtserver and serves any
// number of local clients (mockterm.exe, mtcli.exe, tests) on localhost.

#include "mtcomm_config.h"
#include "mtcomm_engine.h"

#include <mtcore_log.h>
#include <mtcore_mutex.h>
#include <mtcore_socket.h>

#include <stdio.h>

namespace {

mtcore::Event *g_shutdownEvent = NULL;

BOOL WINAPI consoleHandler(DWORD type)
{
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT
     || type == CTRL_CLOSE_EVENT || type == CTRL_SHUTDOWN_EVENT) {
        if (g_shutdownEvent) {
            g_shutdownEvent->set();
        }
        return TRUE;
    }
    return FALSE;
}

}  // close unnamed namespace

int main(int argc, char *argv[])
{
    mtcomm::CommsConfig config;
    std::string error;
    int rc = config.parse(&error, argc, argv);
    if (rc == 1) {
        mtcomm::CommsConfig::printUsage(stdout);
        return 0;
    }
    if (rc != 0) {
        fprintf(stderr, "mtcomm: %s\n", error.c_str());
        mtcomm::CommsConfig::printUsage(stderr);
        return 2;
    }

    mtcore::Logger::setProcessTag("mtcomm");
    mtcore::Logger::setLevel(config.logLevel);
    if (!config.logFile.empty()) {
        mtcore::Logger::setFile(config.logFile.c_str());
    }
    if (mtcore::SocketUtil::initialize() != 0) {
        fprintf(stderr, "mtcomm: WSAStartup failed\n");
        return 1;
    }

    mtcore::Event shutdownEvent(mtcore::Event::e_MANUAL_RESET);
    g_shutdownEvent = &shutdownEvent;
    ::SetConsoleCtrlHandler(&consoleHandler, TRUE);

    int exitCode = 0;
    {
        mtcomm::CommsEngine engine(config);
        if (engine.start() != 0) {
            exitCode = 1;
        }
        else {
            printf("MTCOMM READY port=%d\n", engine.port());
            fflush(stdout);
            shutdownEvent.wait(-1);
            engine.stop();
        }
    }

    g_shutdownEvent = NULL;
    mtcore::SocketUtil::cleanup();
    mtcore::Logger::shutdown();
    return exitCode;
}
