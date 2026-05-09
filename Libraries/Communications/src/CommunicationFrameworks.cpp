//
// Created by cline on 3/9/26.
//

#include "CommunicationFrameworks.h"


void CommPacket::initCommPacket(messageData packet) {
    // for (int i = 0; i < packet.size(); ++i) {
    //     std::cout<<std::to_string(packet[i]);
    //     std::cout<<" ";
    // }
    // std::cout<<std::endl;
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
    else {
        std::cout<<"WHY TF IS THIS HAPPENING"<<std::endl;
    }
}

void CommPacket::initCommPacket(messagePointer packet, size_t packetLen) {
    // for (int i = 0; i < packetLen; i++) {
    //     std::cout<<std::to_string(packet[i]);
    //     std::cout<<" ";
    // }
    // std::cout<<std::endl;

    if (packetLen >=6) {
        header = packet[0];
        version = packet[1];
        Destination_ID = packet[2];
        SourceID = packet[3];
        MessageID = packet[4];

        data.clear();
        if (packetLen!=6) {
            for (int i = 5; i < packetLen-2; i++) {
                data.push_back(packet[i]);
            }
        }
        CRC = packet[packetLen-2];
        footer = packet[packetLen-1];
    }
}

bool CommPacket::validate() {
    return header == COMM_HEADER
        && version == COMM_VERSION
        && footer == COMM_FOOTER
        && isValidSystemId(Destination_ID)
        && isValidSystemId(SourceID)
        && isValidCommand(MessageID);
}

uint8_t CommPacket::computeCRC() {
    uint8_t crc = 0x00;
    auto data = toMessage();

    for (int i = 0; i < data.size()-2; ++i) {
        crc ^= data[i];

        for (int j = 0; j < 8; ++j) {
            if (crc & 0x80) {
                crc = (crc<<1) ^ 0x07;
            }
            else {
                crc<<=1;
            }
        }
    }
    return crc;
}

bool CommPacket::validateCRC() {
    if (CRC == computeCRC()) {
        return true;
    }
    return false;
}

std::string CommPacket::toString() {
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

messageData CommPacket::toMessage() {
    std::vector<unsigned char> messageArr = {header, version, Destination_ID, SourceID, MessageID};
    for (auto d: data) {
        messageArr.push_back(d);
    }

    messageArr.push_back(footer);

    return messageArr;
}

CommPacket CommPacket::makeAcknowledgementPacket(SystemIDs source, SystemIDs dest) {
    auto p =  CommPacket(dest, source, acknowledgement);
    p.data.push_back(0);
    return p;
}

std::string CommPacket::toErrorString() {
    if (getPacketType() == ErrorPacket) {
        std::string errMsg = "Error of type: "+getCommandName(MessageID) + " (" + std::to_string(MessageID) + ")"" occurred in device: "+ getSystemIdName(SourceID) + " (" + std::to_string(SourceID) + ")";
        return errMsg;
    }

    return "Received Packet is not an error message";
}

void TelemetryData::buildMessageData() {
    int dataIndex = 0;

    data.resize(pressureData.size()*2+servoAngles.size()*2);
    for (auto d: servoAngles) {
        data[dataIndex++] =( d >> 8) & 0xFF;
        data[dataIndex++] =d & 0xFF;
    }

    for (auto d: pressureData) {
        data[dataIndex++] =( d >> 8) & 0xFF;
        data[dataIndex++] =d & 0xFF;
    }
}

bool TelemetryData::buildTelemetryData(int numServos, int numPressure) {
    if (numServos*2+numPressure*2 == data.size()) {
        int dataIndex = 0;
        for (int i = 0; i < numServos; i++) {
            servoAngles[i] = data[dataIndex] <<8 | data[dataIndex+1];

            dataIndex += 2;
        }
        for (int i = 0; i < numPressure; i++) {
            pressureData[i] = data[dataIndex] <<8 | data[dataIndex+1];

            dataIndex += 2;
        }
        return true;
    } else {
        return false;
    }
}

std::string TelemetryData::toString() {
    std::string str = "{Telemetry Packet: ";
    str += "Src: "+getSystemIdName(SourceID)+" (" + std::to_string(SourceID) + "), ";
    str += "Pressure Data: [";

    for (int i = 0; i < pressureData.size(); ++i) {
        auto d = pressureData[i];
        str += std::to_string(d);

        if (i != pressureData.size()-1) {
            str += ", ";
        }
    }
    str += "], ";
    str += "Servo Angles: [";
    for (int i = 0; i < servoAngles.size(); ++i) {
        auto d = servoAngles[i];
        str += std::to_string(d);

        if (i != servoAngles.size()-1) {
            str += ", ";
        }
    }
    str += "], ";
    str += "Is Valid: "+ std::to_string(isValid)+"}";

    return str;
}

