//
// Created by cline on 3/24/26.
//

#include "../include/Camera.h"

bool CameraStreamer::prepareStream() {
    if (!openDevice()) return false;
    if (!initMMap()) return false;
    if (!startStream()) return false;

    running = true;
    worker = std::thread(&CameraStreamer::captureLoop, this);
    return true;
}

void CameraStreamer::startSending() {
    paused = false;
    logger->println(VerbosityLevel::INFO, SubsystemTag::VIDEO, "Began sending video over network link!");
}

void CameraStreamer::pauseSending() {
    paused = true;
    logger->println(VerbosityLevel::INFO, SubsystemTag::VIDEO, "Paused sending video over network link!");
}


void CameraStreamer::stop() {
    running = false;
    if (worker.joinable()) worker.join();
    stopStream();
    unmapBuffers();
    close(fd);

    udpSender->closeConn();
}

bool CameraStreamer::openDevice() {
    fd = open(cameraDevice.device.c_str(), O_RDWR | O_NONBLOCK);

    if (fd < 0 ) {
        logger->println(VerbosityLevel::FAULT, SubsystemTag::VIDEO, std::string("Cannot open device: ")+strerror(errno));
        return false;
    }

    v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        logger->println(VerbosityLevel::FAULT, SubsystemTag::VIDEO, std::string("Cannot query capabilities: ")+strerror(errno));
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        logger->println(VerbosityLevel::FAULT, SubsystemTag::VIDEO, std::string("Capturing not supported"));
        return false;
    }

    return true;
}

bool CameraStreamer::initMMap() {
    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = cameraDevice.width;
    fmt.fmt.pix.height = cameraDevice.height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        logger->println(VerbosityLevel::FAULT, SubsystemTag::VIDEO, std::string("Cannot set format: ")+strerror(errno));
        return false;
    }

    v4l2_requestbuffers req{};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        logger->println(VerbosityLevel::FAULT, SubsystemTag::VIDEO, std::string("Could not request buffers: ")+strerror(errno));
        return false;
    }

    buffers.resize(req.count);

    for (size_t i = 0; i < req.count; i++) {
        v4l2_buffer buf{};
        buf.type = req.type;
        buf.memory = req.memory;
        buf.index = i;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            logger->println(VerbosityLevel::FAULT, SubsystemTag::VIDEO, std::string("Could not query buffer (index "+std::to_string(i)+ "): ")+strerror(errno));
            return false;
        }

        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

        if (buffers[i].start == MAP_FAILED) {
            logger->println(VerbosityLevel::FAULT, SubsystemTag::VIDEO, std::string("Failed to map buffer (index "+std::to_string(i)+ "): ")+strerror(errno));
            return false;
        }

        ioctl(fd, VIDIOC_QBUF, &buf);
    }

    return true;
}

bool CameraStreamer::startStream() {
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        logger->println(VerbosityLevel::FAULT, SubsystemTag::VIDEO, std::string("Failed to initialize stream: ")+strerror(errno));
        return false;
    }

    return true;
}

bool CameraStreamer::stopStream() {
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMOFF, &type);

    return true;
}

void CameraStreamer::unmapBuffers() {
    for (auto buffer : buffers) {
        munmap(buffer.start, buffer.length);
    }
}

void CameraStreamer::captureLoop() {
    while (running) {
        if (!paused) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);

            timeval time{};
            time.tv_sec = 2;

            int r = select(fd+1, &fds, NULL, NULL, &time);

            if (r<=0) continue;

            v4l2_buffer buf{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) continue;

            uint8_t* data = static_cast<uint8_t*>(buffers[buf.index].start);
            size_t size = buf.bytesused;

            udpSender->sendData(data, size);

            ioctl(fd, VIDIOC_QBUF, &buf);
        }
    }
}


CameraReceiver::CameraReceiver(int cID, std::shared_ptr<Logger> logger, std::unique_ptr<VideoLink> udpConn) : cameraID(cID),logger(logger), udpConn(std::move(udpConn)) {
    this->udpConn->setCallback([this](VideoFrame f) -> void {
        frameCB(f);
    });

}

void CameraReceiver::frameCB(VideoFrame f) {
    buf.putFrame(f);

    logger->println(VerbosityLevel::DEBUG, SubsystemTag::FOOTBALL, "Received frame num: "+std::to_string(f.frameID) + " for Camera #"+std::to_string(cameraID));
}

size_t Filled(size_t start, size_t end, size_t size) {
    return (end + size - start) % size;
}

size_t Free(size_t start, size_t end, size_t size) {
    return std::max<size_t>(size - Filled(start, end, size), 4) - 4;
}

bool frameBuilder::isFrameCompleted(size_t frame) {
    size_t pos = calcFramePos(frame);

    return expectedPacketForFrames[pos].first.first == expectedPacketForFrames[pos].first.second && expectedPacketForFrames[pos].first.second != 0;
}

VideoFrame* frameBuilder::popCompletedFrame(size_t frame) {
    if (calcFramePos(frame) > 0) {
        if (isFrameCompleted(frame)) {
            size_t pos = calcFramePos(frame);

            std::vector<uint8_t> jpeg;

            for (auto& pkt : storedFrames[pos]) {
                jpeg.insert(jpeg.end(), pkt.begin(), pkt.end());
            }

            return new VideoFrame{(uint32_t) frame, jpeg};
        }
    }

    return nullptr;
}

void frameBuilder::putPacket(VideoHeader hdr, const uint8_t *payload, size_t payloadSize) {
    if (hdr.frameId > highestFrame-size) {
        bool newFrame = calcNewFrame(hdr.frameId);
        size_t pos = calcFramePos(hdr.frameId);
        if (newFrame) {
            storedFrames[pos].clear();
            storedFrames[pos].resize(hdr.packetCount);
            expectedPacketForFrames[pos].first = {hdr.packetCount, 0};
        }
        if (storedFrames[pos][hdr.packetId].empty()) {
            storedFrames[pos][hdr.packetId].assign(payload, payload + payloadSize);
            expectedPacketForFrames[pos].first.second++;
        }
    }
}

size_t frameBuilder::calcFramePos(size_t f) {
    auto n = std::max(f-currZeroFrame, f-(currZeroFrame-size));
    if (n ==100) {
        return 0;
    }

    return n;
}

bool frameBuilder::calcNewFrame(size_t f) {
    if (f > highestFrame) {
        highestFrame = f;
    }

    size_t pos = calcFramePos(f);
    if ( pos > size) {
        currZeroFrame = f;
    }

    if (expectedPacketForFrames[pos].second != f) {
        return true;
    }

    return false;
}

size_t videoBuffer::availFrames() const {
    auto s = start.load(std::memory_order_relaxed);
    auto e = end.load(std::memory_order_relaxed);

    return Filled(s, e, size);
}

size_t videoBuffer::popXFrames(std::vector<VideoFrame> buf, size_t numFrames) {
    auto s = start.load(std::memory_order_relaxed);
    auto e = end.load(std::memory_order_acquire);
    numFrames = std::min(numFrames, Filled(s, e, size));

    size_t copied = 0;

    while (numFrames) {
        auto block = std::min(numFrames, size-s);

        buf.insert(buf.end(), frames.begin()+s, frames.begin()+s+block);

        s = (s+block) % size;
        numFrames -= block;
        copied += block;
    }

    start.store(s, std::memory_order_release);
    return copied;
}

size_t videoBuffer::availForPut() const {
    auto s = start.load(std::memory_order_relaxed);
    auto e = end.load(std::memory_order_relaxed);

    return Free(s, e, size);
}

void videoBuffer::putFrame(VideoFrame frame) {
    auto s = start.load(std::memory_order_relaxed);
    auto e = end.load(std::memory_order_relaxed);

    if (availForPut() > frame.frameID-highestFrame) {
        auto pos = calcFramePos(frame.frameID);

        if (pos > size) {
            currZeroFrame = frame.frameID;
            pos = 0;
        }
        if (frame.frameID > highestFrame) {
            highestFrame = frame.frameID;
        }

        frames[pos] = frame;

        if (pos > e) {
            end.store(pos, std::memory_order_release);
        }
    }

}

size_t videoBuffer::calcFramePos(size_t f) {
    return std::max(f-currZeroFrame, f-(currZeroFrame-size));
}




