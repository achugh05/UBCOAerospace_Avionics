//
// Created by cline on 1/22/26.
//

#ifndef GSE_NETWORKLINK_H
#define GSE_NETWORKLINK_H
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <arpa/inet.h>


enum class Protocol { TCP, UDP };

using networkCallback = std::function<void(const std::string&)>;

class NetworkLink {
    Protocol protocol;
    bool isServer;

    int sock_fd = -1;
    int client_fd = -1; // only used for TCP server
    bool clientConnected = false; //only used for client


    sockaddr_in remoteAddr{};

    networkCallback callback;
    std::mutex cb_mutex;

    std::atomic_bool workerRunning;
    std::thread workerThread;

    std::string rcvBuf;

    size_t sleepTime = 1;

public:
    NetworkLink(Protocol proto, std::string localIP, int localPort, std::string remoteIP, int remotePort, bool isServer = false);
    ~NetworkLink();

    void setCallback(networkCallback cb) {
        std::lock_guard lock(cb_mutex);
        callback = cb;
    };

    ssize_t sendData(const void* data, size_t size);
    void recvData();

    bool isConnected();

private:
    void loop();
    bool attemptConnection();
    void makeNonBlocking(int fd);
};


#endif //GSE_NETWORKLINK_H