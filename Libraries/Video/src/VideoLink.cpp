//
// Created by cline on 3/26/26.
//

#include "../include/VideoLink.h"

#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

VideoLink::VideoLink(std::shared_ptr<Logger> l, std::string localIP, int localPort, std::string remoteIP, int remotePort, bool isReceive) {
    logger = l;

    createSocket();

    //LocalStuff
    sockaddr_in localAddr;
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(localPort);
    inet_pton(AF_INET, localIP.c_str(), &localAddr.sin_addr);

    //prevent errors with sockets when program crashes
    int yes = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    if (bind(sock_fd, (sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        logger->println(VerbosityLevel::FATAL, SubsystemTag::NETWORK, "Error binding local socket, port: " + std::to_string(localPort));
        failed = true;
        return;
    }

    //RemoteStuff
    remoteAddr.sin_family = AF_INET;
    remoteAddr.sin_port = htons(remotePort);
    inet_pton(AF_INET, remoteIP.c_str(), &remoteAddr.sin_addr);

    if (isReceive) {
        workerRunning = true;
        workerThread = std::thread(&VideoLink::loop, this);
    }
}

VideoLink::~VideoLink() {
    closeConn();
}

ssize_t VideoLink::sendData(const uint8_t *data, size_t size) {
    size_t count = 0;
    uint32_t frameID = nextFrameID++;
    size_t packetCount = (size + UDPPACKETSIZE -1)/UDPPACKETSIZE;

    for (uint16_t packetID = 0; packetID < packetCount; ++packetID) {
        size_t offset = packetID * UDPPACKETSIZE;
        size_t chunkSize = std::min((size_t) UDPPACKETSIZE, size - offset) ;

        VideoHeader hdr{
            frameID,
            packetID,
            (uint16_t)packetCount
        };

        uint8_t packet[1500];
        memcpy(packet, &hdr, sizeof(hdr));
        memcpy(packet+sizeof(hdr),data+offset, chunkSize);

        count += sendto(sock_fd, packet, sizeof(hdr)+chunkSize, 0, (sockaddr*)&remoteAddr, sizeof(remoteAddr));
    }

    return count;
}

void VideoLink::recvData() {
    uint8_t buffer[1500];
    ssize_t n = recv(sock_fd, buffer, sizeof(buffer), 0);

    if (n <= 0) return;

    if (n < sizeof(VideoHeader)) return;

    VideoHeader hdr;
    memcpy(&hdr, buffer, sizeof(hdr));

    const uint8_t* payload = buffer + sizeof(hdr);
    size_t payloadSize = n - sizeof(hdr);

    int index = hdr.frameId - (currentFrame-1);

    //New Frame?
    if ( index > storedFrames.size()) {
        storedFrames.resize(index);
        storedFrames[index].resize(hdr.packetCount);

        expectedPacketForFrames.resize(index);
        expectedPacketForFrames[index] = {hdr.packetCount, 0};
    }

    if (expectedPacketForFrames[index].first != hdr.packetCount) {
        storedFrames[index].resize(hdr.packetCount);
        expectedPacketForFrames[index] = {hdr.packetCount, 0};
    }

    if (storedFrames[index][hdr.packetId].empty()) {
        storedFrames[index][hdr.packetId].assign(payload, payload + payloadSize);
        expectedPacketForFrames[index].second++;
    }

    if (expectedPacketForFrames[index].first == expectedPacketForFrames[index].second) {
        std::vector<uint8_t> jpeg;

        for (auto& pkt : storedFrames[index]) {
            jpeg.insert(jpeg.end(), pkt.begin(), pkt.end());
        }

        // Deliver full MJPEG frame
        std::lock_guard lock(cb_mutex);
        if (frameCallback)
            frameCallback(CameraFrame{hdr.frameId, jpeg});

    }
}


void VideoLink::closeConn() {
    if (workerRunning) {
        workerRunning = false;
        workerThread.join();
    }
    if (sock_fd >=0) close(sock_fd);
}

void VideoLink::loop() {
    while (workerRunning && !failed) {
        recvData();
    }
}

void VideoLink::createSocket() {
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock_fd < 0) {
        logger->println(VerbosityLevel::FATAL, SubsystemTag::NETWORK, "Error creating socket");
        failed = true;
        return;
    }
    makeNonBlocking(sock_fd);
}

void VideoLink::makeNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
