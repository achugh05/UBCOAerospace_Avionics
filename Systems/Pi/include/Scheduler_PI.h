//
// Created by cline on 3/26/26.
//

#ifndef GSE_SCHEDULER_PI_H
#define GSE_SCHEDULER_PI_H
#include "NetworkLink.h"
#include "Serial.h"
#include <Scheduler.h>
#include "DataStructures/PiConfig.h"

class Scheduler_PI : Scheduler{
    PiConfig cfg;

    std::vector<std::unique_ptr<CameraStreamer>> cameraStreamers;

public:
    Scheduler_PI();
    ~Scheduler_PI() override;

public:
    //Callbacks

    void initCameras();

    void networkPacketTCP_CB(CommPacket packet) override;
    void networkPacketUDP_CB(CommPacket packet) override;
    void serialPacketCB(CommPacket packet) override;

    void nConnMadeCB() override;
    void nConnLostCB() override;

    commandResponseAction processPacket(CommPacket p) override;
};


#endif //GSE_SCHEDULER_PI_H