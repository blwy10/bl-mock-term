// main.cpp (mtserver)                                                -*-C++-*-
//
// Simulated remote market-data server.  Plays the role of the vendor's data
// centre: ticker plant, reference data, history, news and messaging, served
// over the MockTerm wire protocol (see docs/PROTOCOL.md).

#include "mtsrv_config.h"
#include "mtsrv_server.h"

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
    mtsrv::ServerConfig config;
    std::string error;
    int rc = config.parse(&error, argc, argv);
    if (rc == 1) {
        mtsrv::ServerConfig::printUsage(stdout);
        return 0;
    }
    if (rc != 0) {
        fprintf(stderr, "mtserver: %s\n", error.c_str());
        mtsrv::ServerConfig::printUsage(stderr);
        return 2;
    }

    mtcore::Logger::setProcessTag("mtserver");
    mtcore::Logger::setLevel(config.logLevel);
    if (!config.logFile.empty()) {
        mtcore::Logger::setFile(config.logFile.c_str());
    }
    if (mtcore::SocketUtil::initialize() != 0) {
        fprintf(stderr, "mtserver: WSAStartup failed\n");
        return 1;
    }

    mtcore::Event shutdownEvent(mtcore::Event::e_MANUAL_RESET);
    g_shutdownEvent = &shutdownEvent;
    ::SetConsoleCtrlHandler(&consoleHandler, TRUE);

    int exitCode = 0;
    {
        mtsrv::Server server(config);
        if (server.start() != 0) {
            exitCode = 1;
        }
        else {
            // Line read by launchers (mtcomm, tests) to discover the port.
            printf("MTSERVER READY port=%d\n", server.port());
            fflush(stdout);
            shutdownEvent.wait(-1);
            server.stop();
        }
    }

    g_shutdownEvent = NULL;
    mtcore::SocketUtil::cleanup();
    mtcore::Logger::shutdown();
    return exitCode;
}
