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
    //Packet(const ULONG sessionID, const ULONG sequenceNo, const ULONG fileOffset, const ULONG dataLength, const char* bufferStart); // Data Packet
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

struct Segment
{
    Segment(const USHORT source, const USHORT dest, const Packet& packet); // Length is calculated using .length on the packet 
    Segment(const USHORT source, const USHORT dest, const std::string& packet); // ASSUMPTION: packet string only contains the packet info 
                                                                                //             and is in network order

    std::string GetBuffer() const; // in bytes! will set the checksum value as well
    std::string GetNetworkBuffer() const; // we return the whole segment in an already nicely network ordered buffer in bytes
    USHORT UpdateChecksum(); // will return checksum in host order but set checksum variable to network order

    USHORT SourcePort;
    USHORT DestPort;
    USHORT Length; // Will be used by the client to get the whole packet
    USHORT Checksum;
    Packet Packet; 
};

// Decode Utility Functions
Segment DecodeSegmentNetwork(const std::string& networkSegmentString, bool& isChecksumBroken);
std::vector<Packet> PackFromFile(const ULONG sessionID, const std::filesystem::path& path);
void AppendPacketToFile(const Packet& packetVector, const std::filesystem::path filePath); // Appends a packet to the file
std::vector<ULONG> UnpackToFile(const std::vector<Packet>& packetVector, const std::filesystem::path filePath); // 
                                                                          // returns segments ids that are missing if unpack is unsuccessful
