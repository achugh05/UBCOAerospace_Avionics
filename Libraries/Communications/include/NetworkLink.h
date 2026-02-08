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


enum class Protocol {
    TCP,
    UDP
};

enum class callbackType {
    onJsonReceive,
    onConnection,
    onDisconnection
};

constexpr callbackType onJsonReceive = callbackType::onJsonReceive;
constexpr callbackType onConnection = callbackType::onConnection;
constexpr callbackType onDisconnection = callbackType::onDisconnection;


using networkCallback = std::function<void(const std::string&)>;

class NetworkLink {
    Protocol protocol;
    bool isServer;

    int sock_fd = -1;
    int client_fd = -1; // only used for TCP server
    bool clientConnected = false; //only used for client

    sockaddr_in remoteAddr{};

    networkCallback jsonCallback;
    networkCallback connectionCallback;
    networkCallback disconnectionCallback;
    std::mutex cb_mutex;

    std::atomic_bool workerRunning;
    std::thread workerThread;

    std::string rcvBuf;

    size_t sleepTime = 1;

public:
    NetworkLink(Protocol proto, std::string localIP, int localPort, std::string remoteIP, int remotePort, bool isServer = false);
    ~NetworkLink();

    void setCallback(callbackType cbT, networkCallback cb) {
        std::lock_guard lock(cb_mutex);
        switch (cbT) {
            case onJsonReceive: {
                jsonCallback = cb;
            } break;
            case onConnection: {
                connectionCallback = cb;
            } break;
            case onDisconnection: {
                disconnectionCallback = cb;
            } break;
        }
    };

    ssize_t sendData(const void* data, size_t size);
    void recvData();

    bool isConnected();

    void closeConn();
private:
    void loop();
    void createSocket();
    void disconnect(int fd);
    bool attemptConnection();
    void makeNonBlocking(int fd);
};


#endif //GSE_NETWORKLINK_H