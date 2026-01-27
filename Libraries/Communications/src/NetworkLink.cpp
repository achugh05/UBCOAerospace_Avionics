//
// Created by cline on 1/22/26.
//

#include "../include/NetworkLink.h"

#include <fcntl.h>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <unistd.h>

NetworkLink::NetworkLink(Protocol proto, std::string localIP, int localPort, std::string remoteIP, int remotePort,
                         bool isServer)
        : protocol(proto), isServer(isServer) {

    createSocket();

    if (isServer || protocol == Protocol::UDP) {
        //LocalStuff
        sockaddr_in localAddr;
        localAddr.sin_family = AF_INET;
        localAddr.sin_port = htons(localPort);
        inet_pton(AF_INET, localIP.c_str(), &localAddr.sin_addr);

        //prevent errors with sockets when program crashes
        int yes = 1;
        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (bind(sock_fd, (sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
            throw std::runtime_error("Error binding local socket");
        }
    }

    //RemoteStuff
    remoteAddr.sin_family = AF_INET;
    remoteAddr.sin_port = htons(remotePort);
    inet_pton(AF_INET, remoteIP.c_str(), &remoteAddr.sin_addr);

    if (protocol == Protocol::TCP) {
        if (isServer) {
            if (listen(sock_fd, 1)< 0 ) {
                throw std::runtime_error("Error listening for TCP connection");
            }
        } else {
            int result = connect(sock_fd,
                                 (sockaddr*)&remoteAddr,
                                 sizeof(remoteAddr));

            if (result < 0 && errno != EINPROGRESS && errno !=ECONNREFUSED)
                throw std::runtime_error("connect() failed");
        }
    }

    workerRunning = true;
    workerThread = std::thread(&NetworkLink::loop, this);
}

NetworkLink::~NetworkLink() {
    if (client_fd >= 0) close(client_fd);
    if (sock_fd >=0) close(sock_fd);

    if (workerRunning) {
        workerRunning = false;
        workerThread.join();
    }
}

ssize_t NetworkLink::sendData(const void *data, size_t size) {
    if (isConnected()) {
        if (protocol == Protocol::TCP) {
            int fd = isServer ? client_fd : sock_fd;
            if (fd < 0) return -1;
            return send(fd, data, size, 0);
        } else {
            return sendto(sock_fd, data, size, 0, (sockaddr*)&remoteAddr, sizeof(remoteAddr));
        }
    } else {
        //log something
        return -1;
    }
}

void NetworkLink::recvData() {
    char buffer[2048];
    ssize_t n =0;
    int fd = sock_fd;
    if (protocol == Protocol::TCP) {
        fd = isServer ? client_fd : sock_fd;
        if (fd < 0) return;
        n = recv(fd, buffer, sizeof(buffer), 0);
    } else {
        socklen_t len = sizeof(remoteAddr);
        //n = recvfrom(fd, buffer, sizeof(buffer), 0, (sockaddr*)&remoteAddr, &len);
        n = recv(fd, buffer, sizeof(buffer), 0);
    }

    if (n > 0) {
        rcvBuf.append(buffer, n);

        size_t pos;
        while ((pos = rcvBuf.find("\n")) != std::string::npos) {
            std::string jsonPacket = rcvBuf.substr(0, pos);
            rcvBuf.erase(0, pos+1);

            std::lock_guard lock(cb_mutex);
            if (jsonCallback) jsonCallback(jsonPacket);
        }
    }
    else if (n==0) {
        disconnect(fd);
    }
    else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return; //no Data
        }
        throw std::runtime_error("Error receiving data");
    }
}

bool NetworkLink::isConnected() {
    if (protocol == Protocol::TCP) {
        if (isServer && client_fd >=0) {
            return true;
        } else if (!isServer&& sock_fd >=0) {
            return clientConnected;
        }
        return false;
    } else {
        return true; //UDP is always connected
    }
}

void NetworkLink::loop() {
    while (workerRunning) {
        if (isConnected()) {
            recvData();
        } else {
            attemptConnection();
            if (!isConnected()) {
                std::this_thread::sleep_for(std::chrono::seconds(sleepTime));
            }
        }
    }
}

void NetworkLink::createSocket() {
    sock_fd = socket(AF_INET,
                         (protocol == Protocol::TCP) ? SOCK_STREAM : SOCK_DGRAM,
                         0);

    if (sock_fd < 0) {
        throw std::runtime_error("Error creating socket");
    }
    makeNonBlocking(sock_fd);
}

void NetworkLink::disconnect(int fd) {
    close(fd);
    client_fd = -1;
    clientConnected = false;
    if (!isServer) {
        createSocket(); //to attempt reconnection for server
    }

    std::lock_guard lock(cb_mutex);
    if (disconnectionCallback) disconnectionCallback(""); //we have a logging parameter to pass but idk what to pass.
}

bool NetworkLink::attemptConnection() {
    if (protocol != Protocol::TCP || isConnected()) {
        return false; //This function should never be called if it is connected already or if it is a UDP connection
    }

    if (isServer) {
        int fd = accept(sock_fd, nullptr, nullptr);
        if (fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return false; // no client yet
            } else {
                throw std::runtime_error("Error accepting connection"); // actual error
            }
        }
        client_fd = fd;
        makeNonBlocking(client_fd);
    } else {
        int result = connect(sock_fd,
                                (sockaddr*)&remoteAddr,
                                sizeof(remoteAddr));
        if (result < 0) {
            if (errno == EINPROGRESS || errno == ECONNREFUSED) {
                return false; //no connection yet
            } else {
                throw std::runtime_error("connect() failed");
            }
        }
        clientConnected = true;
    }
    std::lock_guard lock(cb_mutex);
    if (connectionCallback) connectionCallback("");
    return true;
}

void NetworkLink::makeNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
