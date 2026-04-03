//
// Created by cline on 3/24/26.
//

#ifndef GSE_CAMERA_H
#define GSE_CAMERA_H

#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <utility>
#include <vector>
#include <atomic>
#include <iostream>

#include "VideoLink.h"
#include "../../Config/include/configStructs.h"


struct CameraConfig : configBase{
    std::vector<std::string> devices = {"/dev/video0"};
    int width = 1600;
    int height = 1200;
    int firstPort = 7000;

    bool validate(defaults *d) override {
        bool r = true;
        for (int i = 0; i < devices.size(); i++) {
            auto s = devices[i];
            if (strncmp(s.c_str(), "/dev/video", 10) != 0) {
                if (configLoader::getLogger())
                    configLoader::getLogger()->println(VerbosityLevel::WARNING, SubsystemTag::CONFIG, "Specified device path "+ s +" is Invalid, removing from list");

                devices.erase(devices.begin() + i);
                r=false;
            }
        }

        if (width <0) {
            if (configLoader::getLogger())
                configLoader::getLogger()->println(VerbosityLevel::WARNING, SubsystemTag::CONFIG, "Camera width must be a positive number, defaulting to 1600");
            width = 1600;
            r=false;
        }

        if (height <0) {
            if (configLoader::getLogger())
                configLoader::getLogger()->println(VerbosityLevel::WARNING, SubsystemTag::CONFIG, "Camera height must be a positive number, defaulting to 1200");
            width = 1200;
            r=false;
        }

        if (firstPort <= MINPORT || firstPort > MAXPORT) {
            if (configLoader::getLogger())
                configLoader::getLogger()->println(VerbosityLevel::FAULT, SubsystemTag::CONFIG, "Specified port for remote outside safe operating range, Fallback to default value");
            firstPort = 7000;
            r = false;
        }

        return r;
    }

    void from_toml(const toml::table &t) override {
        if (auto* arr = t["capture_devices"].as_array()) {
            devices.reserve(arr->size());
            for (auto&& elem : *arr) {
                if (auto v = elem.value<std::string>())
                    devices.push_back(*v);
            }
        }

        width = t["camera_width"].value_or(width);
        height = t["camera_height"].value_or(height);
        firstPort = t["first_camera_port"].value_or(firstPort);
    }

    toml::table to_toml() {
        toml::table tbl{
            {"camera_width", width},
            {"camera_height",height},
            {"first_camera_port", firstPort}
        };

        toml::array arr;

        for (auto s : devices)
            arr.push_back(s);

        tbl.insert("systemsInNetwork", arr);

        return tbl;
    }
};

struct CameraDevice {
    std::string device;
    int width;
    int height;
};

class CameraStreamer {
    struct Buffer {
        void* start;
        size_t length;
    };

    CameraDevice cameraDevice;
    int fd = -1;
    std::vector<Buffer> buffers;
    std::thread worker;
    std::atomic<bool> running;
    std::unique_ptr<VideoLink> udpSender;
    std::shared_ptr<Logger> logger;


public:
    CameraStreamer(CameraDevice device, std::unique_ptr<VideoLink> udpConn, std::shared_ptr<Logger> logger)
        :cameraDevice(device), udpSender(std::move(udpConn)), logger(std::move(logger)), running(false){}

    ~CameraStreamer() {if (running) stop();}

    bool start();
    void stop();

private:
    bool openDevice();
    bool initMMap();
    bool startStream();
    bool stopStream();

    void unmapBuffers();

    void captureLoop();
};

struct VideoFrame {
    uint32_t frameID;
    std::vector<uint8_t> img;
};

class videoBuffer {
    std::vector<VideoFrame> frames;
};

class CameraReceiver {

};


#endif //GSE_CAMERA_H