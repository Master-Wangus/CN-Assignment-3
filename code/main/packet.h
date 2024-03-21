
#include <vector>
#include <string>
#include <Windows.h>

struct Packet
{
    Packet(const ULONG sessionID, const ULONG sequenceNo, const ULONG fileOffset, const ULONG dataLength, const char* bufferStart); // Data Packet
    Packet(const ULONG sessionID, const ULONG sequenceNo); // Ack Packet

    std::string GetBuffer() const; // in bytes!
    std::string GetNetworkBuffer() const; // we return the whole packet in an already nicely network ordered buffer in bytes
    bool isACK() const;

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
    Segment(const USHORT source, const USHORT dest, const std::string& packet); // Length is calculated using .length on the packet

    std::string GetBuffer() const; // in bytes! will set the checksum value as well
    std::string GetNetworkBuffer() const; // we return the whole segment in an already nicely network ordered buffer in bytes
    USHORT UpdateChecksum(); // will return checksum in host order but set checksum variable to network order
    Packet GetPacket();

    USHORT SourcePort;
    USHORT DestPort;
    USHORT Length; // Will be used by the client to get the whole packet
    USHORT Checksum;
    std::string Packet; 
};

// Decode Utility Functions
Packet DecodePacket(const std::string& packetString);
Packet DecodePacketNetwork(const std::string& networkPacketString);
Segment DecodeSegmentNetwork(const std::string& networkSegmentString, bool& isChecksumBroken);



