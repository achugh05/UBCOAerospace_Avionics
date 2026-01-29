//
// Created by cline on 1/28/26.
//

#ifndef GSE_LOGGER_H
#define GSE_LOGGER_H
#include <atomic>
#include <string>
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>

#define VerbosityTable \
    X(INFO) \
    X(WARNING) \
    X(FAULT) \
    X(FATAL)

#define SubsystemTable \
    X(GSE) \
    X(STATION) \
    X(AVIONIX) \
    X(TELEMETRY) \
    X(COMMS) \
    X(LOGGING)

enum class VerbosityLevel {
#define X(name) name,
    VerbosityTable
#undef X
};

enum class SubsystemTag {
#define X(name) name,
    SubsystemTable
#undef X
};

static std::string getVerbosity(VerbosityLevel c) {
    switch (c) {
    #define X(name) case VerbosityLevel::name: return #name;
        VerbosityTable
    #undef X
    }
    return "";
}
static std::string getSubsystem(SubsystemTag t) {
    switch (t) {
    #define X(name) case SubsystemTag::name: return #name;
        SubsystemTable
    #undef X
    }
    return "";
}

using FolderPath = std::string;
using FilePath = std::string;

struct logMessage {
    uint64_t timestamp;
    VerbosityLevel v_level;
    SubsystemTag s_tag;
    std::string message;
};

class logQueue {
    std::mutex mutex;
    std::queue<logMessage> queue;

public:
    void push(logMessage&& m);
    bool pop(logMessage& out);
};

class Logger {
    FolderPath logFolder;
    FilePath logPath;

    std::ofstream logFile;

    logQueue queue;
    //thread stuff
    std::atomic_bool _running;
    std::thread worker;
public:
    Logger(FolderPath logFolder, std::string SystemName);
    ~Logger();
    void println(VerbosityLevel v, SubsystemTag tag, std::string logMessage);

private:
    bool init(FolderPath path, std::string SystemName);

    void loop();
    std::string formatMsg(logMessage& m);

};


#endif //GSE_LOGGER_H