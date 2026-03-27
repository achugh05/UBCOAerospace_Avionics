//
// Created by cline on 3/24/26.
//

#include "../include/Camera.h"

bool CameraStreamer::start() {
    if (!openDevice()) return false;
    if (!initMMap()) return false;
    if (!startStream()) return false;

    running = true;
    worker = std::thread(&CameraStreamer::captureLoop, this);
    return true;
}

void CameraStreamer::stop() {
    running = false;
    if (worker.joinable()) worker.join();
    stopStream();
    unmapBuffers();
    close(fd);
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
}

void CameraStreamer::unmapBuffers() {
    for (auto buffer : buffers) {
        munmap(buffer.start, buffer.length);
    }
}

void CameraStreamer::captureLoop() {
    while (running) {
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
