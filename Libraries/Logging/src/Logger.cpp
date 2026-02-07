//
// Created by cline on 1/28/26.
//

#include "../include/Logger.h"
#include <sys/stat.h>
#include <iomanip>
#include <iostream>

void logQueue::push(logMessage &&m) {
    std::lock_guard lock(mutex);
    queue.push(std::move(m));
}

bool logQueue::pop(logMessage &out) {
    std::lock_guard lock(mutex);
    if (queue.empty()) return false;
    out = std::move(queue.front());
    queue.pop();
    return true;
}

Logger::Logger(FolderPath logFolder, std::string SystemName, VerbosityLevel logLevel) {
    init(logFolder, SystemName);
    _running = true;
    worker = std::thread(&Logger::loop, this);
}

Logger::~Logger() {
    logFile.close();
    _running = false;
    if (worker.joinable()) worker.join();
}
bool Logger::init(FolderPath path,std::string SystemName) {
    std::string fileName = path;
    if (path.back() != '/') {
        fileName += "/";
    }
    {
        char buf[90];
        std::time_t t = std::time(nullptr);
        strftime(buf, sizeof(buf), "%Y-%m-%d", localtime(&t));
        fileName.append(buf);
    }
    fileName.append(" "+SystemName+".log");

    logPath = path+fileName;
    std::string initialMessage;

    struct stat sb;
    std::string currentTime;
    {
        char buf[90];
        std::time_t t = std::time(nullptr);
        strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&t));
        currentTime = buf;
    }
    if (stat(logPath.c_str(), &sb) == 0) {
        initialMessage = "___________________________________________________________________________________\n"
                         "Logging Resumed at time:\n"
                         +currentTime+ "\n"
                         "___________________________________________________________________________________\n";
    } else {
        initialMessage = "Logging began at time:\n"
                         +currentTime+"\n"
                         "For Device:\n"
                         +SystemName+"\n"
                         "____________________________________________________________________________________\n";
    }

    logFile = std::ofstream(logPath.c_str(), std::ios::out | std::ios::app);
    if (logFile) {
        logFile << initialMessage;
        return true;
    }

    std::cerr << "Failed to initialize logger, Unable to access file location"<<std::endl;
    return false;
}

void Logger::loop() {
    logMessage msg;
    while (_running) {
        while (queue.pop(msg)) {
            logFile<<formatMsg(msg)<<std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

std::string Logger::formatMsg(logMessage &m) {
    std::string time;
    {
        char buf[90];
        std::time_t t = m.timestamp;
        strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&t));
        time = buf;
    }
    auto test = getSubsystem(m.s_tag);
    auto test2 = getVerbosity(m.v_level);
    std::string msg= "["+time+"]["+test+"]["+test2+"] "+ m.message;
    return msg;
}

void Logger::println(VerbosityLevel v, SubsystemTag tag, std::string msg) {
    if (v < logLevel) {
        return;
    }
    logMessage logMsg;
    logMsg.timestamp = time(nullptr);
    logMsg.v_level = v;
    logMsg.s_tag = tag;
    logMsg.message = msg;

    queue.push(std::move(logMsg));
}


