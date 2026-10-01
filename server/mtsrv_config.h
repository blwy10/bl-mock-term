// mtsrv_config.h                                                     -*-C++-*-
#ifndef INCLUDED_MTSRV_CONFIG
#define INCLUDED_MTSRV_CONFIG

//@PURPOSE: Provide mtserver configuration and command-line parsing.
//
//@CLASSES:
//  mtsrv::ServerConfig:  every tunable, with defaults

#include <mtcore_log.h>
#include <mtcore_platform.h>

#include <stdio.h>
#include <string>

namespace mtsrv {

struct ServerConfig {
    std::string             listenAddress;
    int                     port;
    mtcore::Uint64          seed;
    int                     tickIntervalMs;
    double                  tickProbability;
    double                  dropProbability;
    int                     workerThreads;
    int                     heartbeatTimeoutMs;
    int                     newsMinIntervalMs;
    int                     newsMaxIntervalMs;
    int                     newsBackfill;
    int                     statsIntervalMs;
    std::string             logFile;
    mtcore::LogLevel::Value logLevel;

    ServerConfig();

    int parse(std::string *error, int argc, char *argv[]);
        // Parse "--name value" pairs.  Return 0 on success, 1 if usage was
        // requested, and -1 (with 'error' set) on bad input.

    static void printUsage(FILE *stream);
};

}  // close namespace mtsrv

#endif
