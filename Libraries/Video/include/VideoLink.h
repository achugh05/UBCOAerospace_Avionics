//
// Created by cline on 3/26/26.
//

#ifndef GSE_VIDEOLINK_H
#define GSE_VIDEOLINK_H
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <netinet/in.h>

//#include "Camera.h"
#include "Logger.h"

#define UDPPACKETSIZE 1400


struct VideoFrame;
class frameBuilder;

using videoCallback = std::function<void(VideoFrame)>;

struct VideoHeader {
    uint32_t frameId;     // increments every frame
    uint16_t packetId;    // which packet this is
    uint16_t packetCount; // total packets in this frame
};

//Very Similar to network link class but specifically for video instead of commands like network link
class VideoLink {
    bool isReceive;

    int sock_fd = -1;
    sockaddr_in remoteAddr{};

    size_t sleepTime = 1;
    std::atomic_bool failed = false;
    std::shared_ptr<Logger> logger;

    //ONLY NEEDED FOR SENDER
    std::atomic_int nextFrameID = 0;

    //ONLY NEEDED FOR RECEIVER
    videoCallback frameCallback;
    std::mutex cb_mutex;
    std::atomic_bool workerRunning;
    std::thread workerThread;

    frameBuilder* frameStorage;
    size_t currentFrame = 0;
public:
    VideoLink(std::shared_ptr<Logger> l, std::string localIP, int localPort, std::string remoteIP, int remotePort, bool isReceive = false);
    ~VideoLink();

    void setCallback(videoCallback cb) {
        frameCallback = cb;
    }

    ssize_t sendData(const uint8_t* data, size_t size);
    void recvData();

    void closeConn();

private:
    void loop();
    void createSocket();
    void makeNonBlocking(int fd);
};


#endif //GSE_VIDEOLINK_H