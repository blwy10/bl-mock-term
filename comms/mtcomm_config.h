// mtcomm_config.h                                                    -*-C++-*-
#ifndef INCLUDED_MTCOMM_CONFIG
#define INCLUDED_MTCOMM_CONFIG

//@PURPOSE: Provide mtcomm configuration and command-line parsing.
//
//@CLASSES:
//  mtcomm::CommsConfig:  every tunable, with defaults

#include <mtcore_log.h>
#include <mtcore_platform.h>

#include <stdio.h>
#include <string>

namespace mtcomm {

struct CommsConfig {
    std::string             listenAddress;
    int                     port;
    std::string             upstreamHost;
    int                     upstreamPort;
    bool                    spawnServer;
    std::string             serverExe;
    int                     defaultConflateMs;
    int                     maxConflateMs;
    int                     clientTimeoutMs;
    int                     heartbeatMs;
    int                     reconnectMinMs;
    int                     reconnectMaxMs;
    int                     statsIntervalMs;
    std::string             logFile;
    mtcore::LogLevel::Value logLevel;

    CommsConfig();

    int parse(std::string *error, int argc, char *argv[]);
        // Return 0 on success, 1 if usage was requested, -1 on bad input.

    static void printUsage(FILE *stream);
};

}  // close namespace mtcomm

#endif
