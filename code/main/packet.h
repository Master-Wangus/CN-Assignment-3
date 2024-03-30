/* Start Header
*****************************************************************/
/*!
\file packet.h
\authors Koh Wei Ren, weiren.koh, 2202110, 
         Pang Zhi Kai, p.zhikai, 2201573
\par weiren.koh@digipen.edu
\date 03/03/2024
\brief Definition of a packet class that encapsulates data recieved from the server or client.
Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
*******************************************************************/
#pragma once

#include <vector>
#include <string>
#include <Windows.h>
#include <filesystem>

#define PACKET_SIZE size_t(1000)

enum class FLGID
{
    FILE = (unsigned char)0x00,
    ACK = (unsigned char)0x01,
    START = (unsigned char)0x03,
    FIN = (unsigned char)0x04
};

struct Packet
{
    Packet(const ULONG sessionID, const ULONG sequenceNo, const ULONG fileOffset, const ULONG dataLength, const std::string& packetData); // Data Packet
    Packet(const ULONG sessionID, const ULONG sequenceNo); // Ack Packet
    Packet(u_char Flag); // Start/Finish flag

    int GetFullLength() const; // in bytes!
    std::string GetBuffer() const; // in bytes!
    std::string GetBuffer_htonl() const; // we return the whole packet in an already nicely network ordered buffer in bytes
    bool isACK() const;

    static Packet DecodePacket_ntohl(const std::string& networkPacketString);
    static Packet DecodePacket_htonl(const std::string& hostPacketString);
    static std::string GetStartPacket();
    static std::string GetEndPacket();

    // Packet variables are to be stored in host order
    UCHAR Flag;
    ULONG SessionID;
    ULONG SequenceNo;
    ULONG FileOffset; // we are dealing with char arrays so assume its a char offset!
    ULONG DataLength; // in bytes!
    std::string Data;
};

std::vector<Packet> PackFromFile(const ULONG sessionID, const std::filesystem::path& path);
void AppendPacketToFile(const Packet& packetVector, const std::filesystem::path filePath); // Appends a packet to the file
std::vector<ULONG> UnpackToFile(const std::vector<Packet>& packetVector, const std::filesystem::path filePath); // 
                                                                          // returns segments ids that are missing if unpack is unsuccessful
