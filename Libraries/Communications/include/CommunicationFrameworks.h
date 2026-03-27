//
// Created by cline on 3/2/26.
//

#ifndef GSE_TELEMETRYSTRUCTS_H
#define GSE_TELEMETRYSTRUCTS_H
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include <Logger.h>

#define COMM_HEADER 0xAB
#define COMM_VERSION 1
#define COMM_FOOTER 0xEF

#define SystemIDTable \
    X(lora_station) \
    X(manifold1) \
    X(manifold2) \
    X(lora_football) \
    X(mega_football) \
    X(pi_station) \
    X(capstone) \
    X(miniPC)

#define CommandTable \
    X(lora_station_diagnostic, 0) \
    X(hand_control_to_pi, 1) \
    X(take_control_from_pi, 2) \
    X(arm_ignition, 3) \
    X(ignition, 4) \
    X(block_manifold_commands, 5) \
    X(Lock, 6) \
    X(Unlock, 7) \
    X(Diagnose_Manifold, 8) \
    X(Move_Servo_A, 9) \
    X(Calibrate_Servos, 12) \
    X(Close_All_Servos, 13) \
    X(Calibrate_Pressure_Sensor, 14) \
    X(Clear_Faults, 99) \
    X(get_system_state, 102) \
    X(request_telemetry, 49) \
    X(zero_load_cell, 50) \
    X(scale_to_weight, 51) \
    X(get_zeroed_value, 52) \
    X(get_scaled_factor, 53) \
    X(manual_zero_set, 54) \
    X(manual_scale_factor, 55) \
    X(change_delay_amount, 56) \

#define AcknowledgeTable \
    X(acknowledgement, 100) \
    X(return_system_state, 103) \

#define ErrorTable \
    X(sd_card_error, 208) \
    X(critical_error, 250) \

#define TelemetryTable \
    X(Main_Telemetry, 105) \
    X(capstone_Telemetry, 48) \

enum SystemIDs {
#define X(name) name,
    SystemIDTable
#undef X
};

enum Commands {
#define X(name, value) name = value,
    CommandTable
    AcknowledgeTable
    ErrorTable
    TelemetryTable
#undef X
};

static std::string getSystemIdName(uint8_t c) {
    switch (c) {
#define X(name) case SystemIDs::name: return #name;
        SystemIDTable
    #undef X
    }
    return "";
}

static bool isValidSystemId(uint8_t c) {
    switch (c) {
#define X(name) case SystemIDs::name: return true;
        SystemIDTable
    #undef X
    }
    return false;
}

static bool isValidSystemName(std::string c) {
#define X(name) if (c == #name) return true;
    SystemIDTable
#undef X
    return false;
}

static SystemIDs getSystemIdFromName(std::string s) {
#define X(name) if (s == #name) return SystemIDs::name;
    SystemIDTable
#undef X
    return miniPC;
}

static std::string getCommandName(uint8_t c) {
    switch (c) {
#define X(name, value) case Commands::name: return #name;
        CommandTable
        AcknowledgeTable
        ErrorTable
        TelemetryTable
    #undef X
    }
    return "";
}

static bool isValidCommand(uint8_t c) {
    switch (c) {
#define X(name, value) case Commands::name: return true;
        CommandTable
        AcknowledgeTable
        ErrorTable
        TelemetryTable
    #undef X
    }
    return false;
}

static bool isErrorMsg(uint8_t c) {
    switch (c) {
#define X(name, value) case Commands::name: return true;
        ErrorTable
    #undef X
    }
    return false;
}

static bool isTelemetryMsg(uint8_t c) {
    switch (c) {
#define X(name, value) case Commands::name: return true;
        TelemetryTable
    #undef X
    }
    return false;
}

static bool isAcknowledge(uint8_t c) {
    switch (c) {
#define X(name, value) case Commands::name: return true;
        AcknowledgeTable
    #undef X
    }
    return false;
}

static SubsystemTag sourceIDtoSubsystemTag(uint8_t sourceID) {
    switch (sourceID) {
        case 0:
        case 1:
        case 2:
        case 5:{
            return SubsystemTag::STATION;
        } break;
        case 3:
        case 4:
        case 7:{
            return SubsystemTag::FOOTBALL;
        }break;
        case 6: {
            return SubsystemTag::CAPSTONE;
        }
    }

    return SubsystemTag::FOOTBALL;
}

using messageData = std::vector<uint8_t>;
using messagePointer = uint8_t*;

enum PacketType {
    InvalidPacket,
    CommandPacket,
    AcknowledgementPacket,
    ErrorPacket,
    TelemetryPacket,
    VideoPacket,
};

struct CommPacket {
    uint8_t header;

    uint8_t version;

    uint8_t Destination_ID;
    uint8_t SourceID;

    uint8_t MessageID;

    std::vector<uint8_t> data;
    uint8_t CRC;

    uint8_t footer;

    CommPacket() {
        header = 0;
    }

    CommPacket(uint8_t DestID, uint8_t SrcID, uint8_t msgID) {
        header = COMM_HEADER;
        version = COMM_VERSION;
        Destination_ID = DestID;
        SourceID = SrcID;
        MessageID = msgID;
        CRC = computeCRC();
        footer = COMM_FOOTER;
    }

    CommPacket(messageData packet) {
        initCommPacket(packet);
    }

    CommPacket(messagePointer packet, size_t packetLen) {
        initCommPacket(packet, packetLen);
    }

    PacketType getPacketType() {
        if (!validate()) {
            return InvalidPacket;
        } if (isErrorMsg(MessageID)) {
            return ErrorPacket;
        } else if (isTelemetryMsg(MessageID)) {
            return TelemetryPacket;
        } else if (isAcknowledge(MessageID)) {
            return AcknowledgementPacket;
        } else {
            return CommandPacket;
        }
    }

    void initCommPacket(messageData packet);
    void initCommPacket(messagePointer packet, size_t packetLen);

    bool validate();

    uint8_t computeCRC();
    bool validateCRC();

    std::string toString();

    messageData toMessage();

    static CommPacket makeAcknowledgementPacket(SystemIDs source, SystemIDs dest);

    std::string toErrorString();
};

struct TelemetryData : CommPacket {

    std::vector<uint16_t> servoAngles;
    std::vector<uint16_t> pressureData;

    bool isValid = false;

    TelemetryData(CommPacket p, int numServos, int numPressure) : CommPacket(p) {
        servoAngles.resize(numServos);
        pressureData.resize(numPressure);

        isValid = buildTelemetryData(numServos, numPressure) && validate();
    }


    //FOR TESTING PURPOSES ONLY
    TelemetryData(uint8_t DestID, uint8_t SrcID, std::vector<uint16_t> tData, int numServos, int numPressure): CommPacket(DestID, SrcID, Main_Telemetry) {
        servoAngles.resize(numServos);
        pressureData.resize(numPressure);

        for (int i = 0; i < numServos; ++i) {
            servoAngles[i] = tData[i];
        }
        for (int i = 0; i < numPressure; ++i) {
            pressureData[i] = tData[i+numServos];
        }

        buildMessageData();

        computeCRC();
    }

    void buildMessageData();

    bool buildTelemetryData(int numServos, int numPressure);

    std::string toString();

    bool isValidPacket() { return isValid; }

};

#endif //GSE_TELEMETRYSTRUCTS_H