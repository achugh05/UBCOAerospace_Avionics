//
// Created by cline on 2026-01-18.
//

#ifndef UBCO_FOOTBALL_PI_SERIAL_H
#define UBCO_FOOTBALL_PI_SERIAL_H

#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

struct Packet
{

};

using Callback = std::function<void(const std::string&)>;

class Serial
{
    int fd = -1;
    bool bIsOpen = false;

    Callback callback;

    std::thread workerLoop;
    std::mutex cbMutex;
    std::atomic_bool _running {false};
public:
    Serial(const char *portName, int speed);
    Serial(const Serial&) = delete;
    Serial& operator=(const Serial&) = delete;

    ~Serial();

    void updateSerialPort(const char* portName, int speed);

    void closeSerialPort();
    void sendPacket(const char* buffer, size_t size);

    int getPortID() const {return fd;}
    void setCallback(Callback cb) {
        std::lock_guard lock(cbMutex);
        callback = cb;
    };

    bool isOpen() {return bIsOpen;}
private:
    void loop();
    int openSerialPort(const char* portName);
    bool configureSerialPort(int speed);
};


#endif //UBCO_FOOTBALL_PI_SERIAL_H