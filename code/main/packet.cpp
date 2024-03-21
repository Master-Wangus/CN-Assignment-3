#include "packet.h"
#include "Utils.h"

enum class FLGID
{
	FILE = 0x00,
	ACK = 0x01
};

Packet::Packet(const ULONG sessionID, const ULONG sequenceNo, const ULONG fileOffset, const ULONG dataLength, const char* bufferStart) :
	Flag((UCHAR)FLGID::FILE), SessionID(sessionID), SequenceNo(sequenceNo), FileOffset(fileOffset), DataLength(dataLength)
{
	for (size_t i{}; i < DataLength; ++i)
	{
		Data.append(*(bufferStart + FileOffset + i), sizeof(UCHAR));
	}
}

Packet::Packet(const ULONG sessionID, const ULONG sequenceNo) : Flag((UCHAR)FLGID::ACK), SessionID(sessionID), SequenceNo(sequenceNo), FileOffset(0), DataLength(0)
{

}

size_t Packet::GetFullLength() const
{
	return sizeof(UCHAR) + sizeof(ULONG) * 4 + DataLength;
}

std::string Packet::GetBuffer() const
{
	std::string buffer;

	buffer.append(reinterpret_cast<const char*>(&Flag), sizeof(Flag));
	buffer.append(reinterpret_cast<const char*>(&SessionID), sizeof(SessionID));
	buffer.append(reinterpret_cast<const char*>(&SequenceNo), sizeof(SequenceNo));
	buffer.append(reinterpret_cast<const char*>(&FileOffset), sizeof(FileOffset));
	buffer.append(reinterpret_cast<const char*>(&DataLength), sizeof(DataLength));
	buffer += Data;

	return buffer;
}

std::string Packet::GetNetworkBuffer() const
{
	std::string buffer;
	// Serialize each field and append to the serializedData string
	ULONG networkSessionID = htonl(SessionID);
	ULONG networkSequenceNo = htonl(SequenceNo);
	ULONG networkFileOffset = htonl(FileOffset);
	ULONG networkDatalength = htonl(DataLength);


	buffer.append(reinterpret_cast<const char*>(&Flag), sizeof(Flag));
	buffer.append(reinterpret_cast<const char*>(&networkSessionID), sizeof(networkSessionID));
	buffer.append(reinterpret_cast<const char*>(&networkSequenceNo), sizeof(networkSequenceNo));
	buffer.append(reinterpret_cast<const char*>(&networkFileOffset), sizeof(networkFileOffset));
	buffer.append(reinterpret_cast<const char*>(&networkDatalength), sizeof(networkDatalength));
	buffer += Data; 

	return buffer;
}

bool Packet::isACK() const
{
	return Flag & (UCHAR)FLGID::ACK;
}

Segment::Segment(const USHORT source, const USHORT dest, const ::Packet& packet)
	: SourcePort(source), DestPort(dest), Length(sizeof(USHORT) * 4 + packet.GetFullLength()), Checksum(0), Packet(packet) {} // Source + Dest + Length + Checksum = 32


Segment::Segment(const USHORT source, const USHORT dest, const std::string& packet)
	: SourcePort(source), DestPort(dest), Length(sizeof(USHORT) * 4 + packet.length()), Checksum(0), Packet(DecodePacketNetwork(packet)) {} // Source + Dest + Length + Checksum = 32

std::string Segment::GetBuffer() const
{
	std::string buffer;
	USHORT newCheckSum;

	buffer.append(reinterpret_cast<const char*>(&SourcePort), sizeof(SourcePort));
	buffer.append(reinterpret_cast<const char*>(&DestPort), sizeof(DestPort));
	buffer.append(reinterpret_cast<const char*>(&Length), sizeof(Length));
	buffer.append(reinterpret_cast<const char*>(&Checksum), sizeof(Checksum));
	buffer += Packet.GetBuffer();

	newCheckSum = Utils::ToChecksum(buffer);
	buffer.insert(sizeof(SourcePort) + sizeof(DestPort) + sizeof(Length), reinterpret_cast<const char*>(&newCheckSum));

	return buffer;
}

std::string Segment::GetNetworkBuffer() const
{
	std::string buffer;
	USHORT newCheckSum;

	// Serialize each field and append to the serializedData string
	USHORT networkSourcePort = htons(SourcePort);
	USHORT networkDestPort = htons(DestPort);
	USHORT networkLength = htons(Length);

	buffer.append(reinterpret_cast<const char*>(&networkSourcePort), sizeof(networkSourcePort));
	buffer.append(reinterpret_cast<const char*>(&networkDestPort), sizeof(networkDestPort));
	buffer.append(reinterpret_cast<const char*>(&networkLength), sizeof(networkLength));
	buffer.append(reinterpret_cast<const char*>(&Checksum), sizeof(Checksum));
	buffer += Packet.GetNetworkBuffer();

	newCheckSum = htons(Utils::ToChecksum(buffer));
	buffer.insert(sizeof(SourcePort) + sizeof(DestPort) + sizeof(Length), reinterpret_cast<const char*>(&newCheckSum));
}

USHORT Segment::UpdateChecksum()
{
	USHORT newCheckSum = htons(Utils::ToChecksum(GetNetworkBuffer()));
	Checksum = htons(newCheckSum);
	return newCheckSum;
}

Packet DecodePacket(const std::string& packetString)
{
	auto stringToUlong = [](const std::string& str)
		{
			u_long ret = 0;
			std::memcpy(&ret, str.data(), sizeof(ULONG));
			return ret;
		};
	UCHAR Flag = packetString[0];
	ULONG SessionID = stringToUlong(packetString.substr(1, sizeof(ULONG)));
	ULONG SequenceNo = stringToUlong(packetString.substr(5, sizeof(ULONG)));

	if (Flag & (UCHAR)FLGID::FILE)
	{
		ULONG FileOffset = stringToUlong(packetString.substr(9, sizeof(ULONG)));
		ULONG DataLength = stringToUlong(packetString.substr(13, sizeof(ULONG)));
		std::string Data = packetString.substr(17);

		return Packet(SessionID, SequenceNo, FileOffset, DataLength, Data.c_str());
	}
	else if (Flag & (UCHAR)FLGID::ACK)
		return Packet(SessionID, SequenceNo);
}

Packet DecodePacketNetwork(const std::string& networkPacketString)
{
	UCHAR Flag = networkPacketString[0];
	ULONG SessionID = Utils::StringTo_ntohl(networkPacketString.substr(1, sizeof(ULONG)));
	ULONG SequenceNo = Utils::StringTo_ntohl(networkPacketString.substr(5, sizeof(ULONG)));

	if (Flag & (UCHAR)FLGID::FILE)
	{
		ULONG FileOffset = Utils::StringTo_ntohl(networkPacketString.substr(9, sizeof(ULONG)));
		ULONG DataLength = Utils::StringTo_ntohl(networkPacketString.substr(13, sizeof(ULONG)));
		std::string Data = networkPacketString.substr(17);

		return Packet(SessionID, SequenceNo, FileOffset, DataLength, Data.c_str());
	}
	else if (Flag & (UCHAR)FLGID::ACK)
		return Packet(SessionID, SequenceNo);
}

Segment DecodeSegmentNetwork(const std::string& networkSegmentString, bool& isChecksumBroken)
{
	USHORT SourcePort = Utils::StringTo_ntohs(networkSegmentString.substr(0, sizeof(USHORT)));
	USHORT DestPort = Utils::StringTo_ntohs(networkSegmentString.substr(2, sizeof(USHORT)));
	USHORT Length = Utils::StringTo_ntohs(networkSegmentString.substr(4, sizeof(USHORT)));
	USHORT Checksum = Utils::StringTo_ntohs(networkSegmentString.substr(6, sizeof(USHORT)));
	std::string PacketStr = networkSegmentString.substr(8, Length - sizeof(SourcePort) - sizeof(DestPort) - sizeof(Length) - sizeof(Checksum));

	Segment seggs = Segment(SourcePort, DestPort, PacketStr); // constructor already handles network ordered packet
	if (seggs.UpdateChecksum() == Checksum)
		isChecksumBroken = false;
	else
		isChecksumBroken = true;

	return seggs;
}
