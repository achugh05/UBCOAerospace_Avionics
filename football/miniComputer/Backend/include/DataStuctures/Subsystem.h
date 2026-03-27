//
// Created by cline on 3/12/26.
//

#ifndef GSE_SUBSYSTEM_H
#define GSE_SUBSYSTEM_H
#include "CommunicationFrameworks.h"


enum systemState {
    booting,
    normal,
    critical,
    fatal
};

struct Subsystem {
    std::string name;
    SystemIDs systemID;

    systemState state = booting;
    bool awaitingResponse = false;

    int errorCode = -1;

    CommPacket lastReceivedMessage;

    Subsystem() {
        name = "";
        systemID = miniPC;
        lastReceivedMessage = CommPacket();
    }

    Subsystem(std::string name) {
        this->name = name;
        systemID = getSystemIdFromName(name);

        lastReceivedMessage = CommPacket();
    }

    bool isValid() {
        return systemID != miniPC;
    }
};


#endif //GSE_SUBSYSTEM_H