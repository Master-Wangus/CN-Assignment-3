/* Start Header
*****************************************************************/
/*!
\file packet.cpp
\authors Koh Wei Ren, weiren.koh, 2202110,
		 Pang Zhi Kai, p.zhikai, 2201573
\par weiren.koh@digipen.edu
\date 03/03/2024
\brief Implementation of a packet class that encapsulates data recieved from the server or client.
Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
*******************************************************************/

#include "packet.h"
#include "Utils.h"
#include <fstream>
#include <iostream>

Packet::Packet(const ULONG sessionID, const ULONG sequenceNo, const ULONG fileOffset, const ULONG dataLength, const std::string& packetData) :
	Flag((UCHAR)FLGID::FILE), SessionID(sessionID), SequenceNo(sequenceNo), FileOffset(fileOffset), DataLength(dataLength), Data(packetData)
{
}

Packet::Packet(const ULONG sessionID, const ULONG sequenceNo) : Flag((UCHAR)FLGID::ACK), SessionID(sessionID), SequenceNo(sequenceNo), FileOffset(0), DataLength(0)
{

}

Packet::Packet(u_char flag) : Flag(flag), SessionID{}, SequenceNo{}, FileOffset{}, DataLength{}
{
}

int Packet::GetFullLength() const
{
	size_t length{};
	switch (static_cast<FLGID>(Flag))
	{
	case FLGID::FILE:
	{
		length += DataLength + 2 * sizeof(ULONG); // Data + FileOffset + DataLength
		__fallthrough;
	}
	case FLGID::ACK:
	{
		length += 2 * sizeof(ULONG); // SessionID + Sequence No.
		__fallthrough;
	}
	default:
		length += 1; // Flag
	}

	return static_cast<int>(length);
}

std::string Packet::GetBuffer() const
{
	std::string buffer;

	buffer.append(reinterpret_cast<const char*>(&Flag), sizeof(Flag));
	buffer.append(reinterpret_cast<const char*>(&SessionID), sizeof(SessionID));
	buffer.append(reinterpret_cast<const char*>(&SequenceNo), sizeof(SequenceNo));

	if (Flag & (UCHAR)FLGID::FILE)
	{
		buffer.append(reinterpret_cast<const char*>(&FileOffset), sizeof(FileOffset));
		buffer.append(reinterpret_cast<const char*>(&DataLength), sizeof(DataLength));
		buffer += Data;
	}

	return buffer;
}

std::string Packet::GetBuffer_htonl() const
{
	std::string buffer{};
	buffer.append(reinterpret_cast<const char*>(&Flag), sizeof(Flag));
	
	// Serialize each field and append to the serializedData string
	ULONG networkSessionID = htonl(SessionID);
	ULONG networkSequenceNo = htonl(SequenceNo);

	buffer.append(reinterpret_cast<const char*>(&networkSessionID), sizeof(networkSessionID));
	buffer.append(reinterpret_cast<const char*>(&networkSequenceNo), sizeof(networkSequenceNo));

	if (Flag == (UCHAR)FLGID::FILE)
	{
		ULONG networkFileOffset = htonl(FileOffset);
		ULONG networkDatalength = htonl(DataLength);

		buffer.append(reinterpret_cast<const char*>(&networkFileOffset), sizeof(networkFileOffset));
		buffer.append(reinterpret_cast<const char*>(&networkDatalength), sizeof(networkDatalength));
		buffer += Data;
	}

	return buffer;
}

Packet Packet::DecodePacket_ntohl(const std::string& networkPacketString)
{
	UCHAR Flag = networkPacketString[0];
	if (Flag == (UCHAR)FLGID::START || Flag == (UCHAR)FLGID::FIN) 
		return Packet(Flag);

	ULONG SessionID = Utils::StringTo_ntohl(networkPacketString.substr(1, sizeof(ULONG)));
	ULONG SequenceNo = Utils::StringTo_ntohl(networkPacketString.substr(5, sizeof(ULONG)));

	if (Flag == (UCHAR)FLGID::FILE)
	{
		ULONG FileOffset = Utils::StringTo_ntohl(networkPacketString.substr(9, sizeof(ULONG)));
		ULONG DataLength = Utils::StringTo_ntohl(networkPacketString.substr(13, sizeof(ULONG)));
		std::string Data = networkPacketString.substr(17);

		return Packet(SessionID, SequenceNo, FileOffset, DataLength, Data.c_str());
	}
	else
	{
		return Packet(SessionID, SequenceNo);
	}
}

Packet Packet::DecodePacket_htonl(const std::string& hostPacketString)
{
	UCHAR Flag = hostPacketString[0];
	ULONG SessionID = Utils::StringTo_htonl(hostPacketString.substr(1, sizeof(ULONG)));
	ULONG SequenceNo = Utils::StringTo_htonl(hostPacketString.substr(5, sizeof(ULONG)));

	if (Flag & (UCHAR)FLGID::FILE)
	{
		ULONG FileOffset = Utils::StringTo_htonl(hostPacketString.substr(9, sizeof(ULONG)));
		ULONG DataLength = Utils::StringTo_htonl(hostPacketString.substr(13, sizeof(ULONG)));
		std::string Data = hostPacketString.substr(17);

		return Packet(SessionID, SequenceNo, FileOffset, DataLength, Data.c_str());
	}
	else
	{
		return Packet(SessionID, SequenceNo);
	}
}

std::string Packet::GetStartPacket()
{
	return std::string{ static_cast<u_char>(FLGID::START) };
}

std::string Packet::GetEndPacket()
{
	return std::string{static_cast<u_char>(FLGID::FIN)};
}

bool Packet::isACK() const
{
	return Flag == (UCHAR)FLGID::ACK;
}

std::vector<Packet> PackFromFile(const ULONG sessionID, const std::filesystem::path& path)
{
	std::vector<Packet> packets;
	std::ifstream file(path, std::ios::binary);

	if (!file) 
	{
		std::cerr << "Could not open the file: " << path << std::endl;
		return packets; // Return an empty vector in case of failure
	}

	unsigned long offset = 0;
	ULONG sequenceNo = 0;
	while (file) 
	{
		// Read a segment of the file
		char* buffer = new char[PACKET_SIZE];
		file.read(buffer, PACKET_SIZE);
		std::streamsize bytesRead = file.gcount();

		// Set Packet fields
		Packet packet(sessionID, sequenceNo, offset, static_cast<ULONG>(bytesRead), std::string(buffer, bytesRead));

		// Clean up the temporary buffer
		delete[] buffer;

		// Only add the packet if we read something
		if (bytesRead > 0) 
		{
			packets.push_back(packet);
		}

		offset += static_cast<unsigned long>(bytesRead);
		++sequenceNo;
	}

	return packets;
}

void AppendPacketToFile(const Packet& packet, const std::filesystem::path filePath)
{
	std::ofstream outputFile(filePath, std::ios::binary | std::ios::app);

	outputFile.write(packet.Data.c_str(), packet.DataLength);

	outputFile.close();

	if (outputFile.bad())
	{
		std::cerr << "An error occurred while writing to the file: " << filePath << std::endl;
	}
	else {
		std::cout << "File successfully reconstructed from packet: " << packet.SequenceNo << std::endl;
	}
}

std::vector<ULONG> UnpackToFile(const std::vector<Packet>& packetVector, const std::filesystem::path filePath)
{
	std::vector<ULONG> segmentIDs;
	std::ofstream outputFile(filePath, std::ios::binary | std::ios::app);
	if (packetVector.begin()->SequenceNo == 0) outputFile.clear();

	for (ULONG segmentID{}; segmentID < packetVector.size(); ++segmentID)
	{
		outputFile.write(packetVector[segmentID].Data.c_str(), packetVector[segmentID].DataLength);
	}
	outputFile.close();

	if (outputFile.bad()) 
	{
		std::cerr << "An error occurred while writing to the file: " << filePath << std::endl;
	}
	else {
		std::cout << "File successfully reconstructed from packets." << std::endl;
	}

	return segmentIDs;
}