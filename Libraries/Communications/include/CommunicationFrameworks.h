//
// Created by cline on 3/2/26.
//

#ifndef GSE_TELEMETRYSTRUCTS_H
#define GSE_TELEMETRYSTRUCTS_H
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#define COMM_HEADER 0xAB
#define COMM_VERSION 1
#define COMM_FOOTER 0xFF

#define SystemIDTable \
    X(lora_station) \
    X(manifold1) \
    X(manifold2) \
    X(lora_football) \
    X(mega_football) \
    X(pi_station) \
    X(capstone) \
    X(miniPC) \

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
    X(acknowledgement, 100) \
    X(zero_load_cell, 50) \
    X(scale_to_weight, 51) \
    X(get_zeroed_value, 52) \
    X(get_scaled_factor, 53) \
    X(manual_zero_set, 54) \
    X(manual_scale_factor, 55) \
    X(change_delay_amount, 56) \

#define ErrorTable \
    X(sd_card_error, 208) \
    X(critical_error, 250) \

#define TelemetryTable \
    X(Main_Telemetry, 101) \
    X(capstone_Telemetry, 49) \

enum SystemIDs {
#define X(name) name,
    SystemIDTable
#undef X
};

enum Commands {
#define X(name, value) name = value,
    CommandTable
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

static std::string getCommandName(uint8_t c) {
    switch (c) {
#define X(name, value) case Commands::name: return #name;
        CommandTable
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

struct TelemetryData {
    uint16_t* servoAngles;
    uint16_t* pressureData;
};

using messageData = std::vector<uint8_t>;

enum PacketType {
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

    uint8_t footer;

    CommPacket(uint8_t DestID, uint8_t SrcID, uint8_t msgID) {
        header = COMM_HEADER;
        version = COMM_VERSION;
        Destination_ID = DestID;
        SourceID = SrcID;
        MessageID = msgID;
        footer = COMM_FOOTER;
    }

    CommPacket(messageData packet) {
        initCommPacket(packet);
    }

    void initCommPacket(messageData packet) {
        if (packet.size()>=6) {
            header = packet[0];
            version = packet[1];
            Destination_ID = packet[2];
            SourceID = packet[3];
            MessageID = packet[4];

            //length of data
            data.clear();
            if (packet.size()!=6) {
                data.insert(data.end(), packet.begin()+5, packet.end()-1);
            }
            footer = packet[packet.size()-1];
        }
    }

    bool validate() {
        return header == COMM_HEADER
        && version == COMM_VERSION
        && footer == COMM_FOOTER
        && isValidSystemId(Destination_ID)
        && isValidSystemId(SourceID)
        && isValidCommand(MessageID);
    }

    std::string toString() {
        std::string str = "{";
        str += "Dest: "+getSystemIdName(Destination_ID) +" (" + std::to_string(Destination_ID);
        str += "), Src: "+getSystemIdName(SourceID)+" (" + std::to_string(SourceID);
        str+= "), MessageID: "+getCommandName(MessageID)+" (" + std::to_string(MessageID);
        str+= "), Data: [";
        for (int i = 0; i < data.size(); ++i) {
            auto d = data[i];
            str += std::to_string(d);

            if (i != data.size()-1) {
                str += ", ";
            }
        }

        str += "], Valid: " + std::to_string(validate()) + "}";

        return str;
    }

    messageData toMessage() {
        std::vector<unsigned char> messageArr = {header, version, Destination_ID, SourceID, MessageID};
        for (auto d: data) {
            messageArr.push_back(d);
        }

        messageArr.push_back(footer);
        messageArr.push_back('\n');

        return messageArr;
    }


};

#endif //GSE_TELEMETRYSTRUCTS_H