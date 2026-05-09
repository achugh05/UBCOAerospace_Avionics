//
// Created by cline on 2026-01-18.
//

#include "../include/Serial.h"

#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

using std::cerr;
using std::endl;

Serial::Serial(const char *portName, int speed, std::shared_ptr<Logger> l) {
    logger = l;
    if (openSerialPort(portName) == -1) {
        bIsOpen = false;
        return;
    };
    if (!configureSerialPort(speed)) {
        bIsOpen = false;
        return;
    }

    _running = true;
    workerLoop = std::thread(&Serial::loop, this);
}

Serial::~Serial() {
    _running = false;
    if (workerLoop.joinable())
        workerLoop.join();

    closeSerialPort();
}

void Serial::updateSerialPort(const char *portName, int speed) {
    _running = false;
    workerLoop.join();

    closeSerialPort();
    openSerialPort(portName);
    configureSerialPort(speed);

    _running = true;
    workerLoop = std::thread(&Serial::loop, this);
}

void Serial::closeSerialPort() {
    bIsOpen = false;
    close(fd);
}

void Serial::sendPacket(const void *buffer, size_t size) {
    write(fd, buffer, size);
}

void Serial::loop() {
    uint8_t buf[256];
    uint8_t packet[2048];
    size_t packetLen = 0;

    while (_running) {
        int n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                uint8_t c = buf[i];

                if (packetLen < sizeof(packet)) {
                    packet[packetLen++] = c;
                }

                if (c == COMM_FOOTER) {
                    if (callback) {
                        std::lock_guard lock(cbMutex);
                        callback(CommPacket(packet, packetLen));
                    }

                    packetLen = 0;
                }
            }
        }
    }
}

int Serial::openSerialPort(const char* portName) {
    fd = open(portName, O_RDWR | O_NOCTTY | O_SYNC);
    bIsOpen = true;

    if (fd < 0) {
        logger->println(VerbosityLevel::FAULT, SubsystemTag::COMMS, std::string("Error opening serial port ") + portName +", " +strerror(errno));
        return -1;
    }

    return fd;
}

bool Serial::configureSerialPort(int speed) {
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        logger->println(VerbosityLevel::FAULT, SubsystemTag::COMMS, std::string("Error setting serial port attributes: ") +strerror(errno));
        return false;
    }

    cfsetspeed(&tty, speed);

    // --- Control modes (c_cflag) ---
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    // --- Local modes (c_lflag) ---
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;

    // --- Input modes (c_iflag) ---
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    // --- Output modes (c_oflag) ---
    tty.c_oflag &= ~OPOST;

    // --- Read timeout settings ---
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 10;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        logger->println(VerbosityLevel::FAULT, SubsystemTag::COMMS, std::string("Error setting serial port attributes: ") +strerror(errno));
        return false;
    }

    return true;
}


