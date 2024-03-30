/* Start Header
*****************************************************************/
/*!
\file echoclient.cpp
\author Koh Wei Ren, weiren.koh, 2202110
		Pang Zhi Kai, p.zhikai, 2201573
\par weiren.koh@digipen.edu
	 p.zhikai@digipen.edu
\date 03/03/2024
\brief A multithreaded client that sends formatted data to a server. Has dedicated threads for receiving and sending information.
Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
*******************************************************************/

//*******************************************************************************
// * A multi-threaded TCP/IP client application
// ******************************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Windows.h"		// Entire Win32 API...
 // #include "winsock2.h"	// ...or Winsock alone
#include "ws2tcpip.h"		// getaddrinfo()

// Tell the Visual Studio linker to include the following library in linking.
// Alternatively, we could add this file to the linker command-line parameters,
// but including it in the source code simplifies the configuration.
#pragma comment(lib, "ws2_32.lib")

#include <iostream>			// cout, cerr
#include <string>			// string
#include <sstream>
#include <iomanip>
#include <thread>
#include <queue>

#include "Utils.h"			// helper file
#include "packet.h"

// forward declarations
void receive(SOCKET,SOCKET);

enum CMDID {
	UNKNOWN = (unsigned char)0x0,//not used
	REQ_QUIT = (unsigned char)0x1,
	REQ_DOWNLOAD = (unsigned char)0x2,
	RSP_DOWNLOAD = (unsigned char)0x3,
	REQ_LISTFILES = (unsigned char)0x4,
	RSP_LISTFILES = (unsigned char)0x5,
	CMD_TEST = (unsigned char)0x20,//not used
	DOWNLOAD_ERROR = (unsigned char)0x30
};

std::string g_downloadPath;
std::string g_fileName;
size_t g_WindowSize{};
float g_packLossRate{};
// This program requires one extra command-line parameter: a server hostname.
int main(int argc, char** argv)
{
	std::string ServerIP{}, TCPServerPort{}, UDPServerPort{}, UDPClientPort{};

	std::cout << "Server IP Address: ";
	std::cin >> ServerIP;
	std::cout << std::endl;

	std::cout << "Server TCP Port Number: ";
	std::cin >> TCPServerPort;
	std::cout << std::endl;

	std::cout << "Server UDP Port Number: ";
	std::cin >> UDPServerPort;
	std::cout << std::endl;

	std::cout << "Client UDP Port Number: ";
	std::cin >> UDPClientPort;
	std::cout << std::endl;

	std::cout << "Path to store files: ";
	std::cin >> g_downloadPath;
	std::cout << std::endl;

	std::cout << "Packet loss rate [0.f, 1.f]: ";
	std::cin >> g_packLossRate;
	std::cout << std::endl;


	// -------------------------------------------------------------------------
	// Start up Winsock, asking for version 2.2.
	//
	// WSAStartup()
	// -------------------------------------------------------------------------

	// This object holds the information about the version of Winsock that we
	// are using, which is not necessarily the version that we requested.
	WSADATA wsaData{};
	SecureZeroMemory(&wsaData, sizeof(wsaData));

	// Initialize Winsock. You must call WSACleanup when you are finished.
	// As this function uses a reference counter, for each call to WSAStartup,
	// you must call WSACleanup or suffer memory issues.
	int errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (errorCode != NO_ERROR)
	{
		std::cerr << "WSAStartup() failed." << std::endl;
		return errorCode;
	}

	// -------------------------------------------------------------------------
	// Resolve a server host name into IP addresses (in a singly-linked list).
	//
	// getaddrinfo()
	// -------------------------------------------------------------------------

	// Object hints indicates which protocols to use to fill in the info.
	addrinfo TCPhints{};
	SecureZeroMemory(&TCPhints, sizeof(TCPhints));
	TCPhints.ai_family = AF_INET;			// IPv4
	TCPhints.ai_socktype = SOCK_STREAM;	// Reliable delivery
	// Could be 0 to autodetect, but reliable delivery over IPv4 is always TCP.
	TCPhints.ai_protocol = IPPROTO_TCP;	// TCP

	addrinfo* TCPinfo = nullptr;
	errorCode = getaddrinfo(ServerIP.c_str(), TCPServerPort.c_str(), &TCPhints, &TCPinfo);
	if ((errorCode) || (TCPinfo == nullptr))
	{
		std::cerr << "Unable to connect to the server..." << std::endl;
		WSACleanup();
		return errorCode;
	}

	// -------------------------------------------------------------------------
	// Create a socket and attempt to connect to the first resolved address.
	//
	// socket()
	// connect()
	// -------------------------------------------------------------------------

	SOCKET TCPSocket = socket(
		TCPinfo->ai_family,
		TCPinfo->ai_socktype,
		TCPinfo->ai_protocol);
	if (TCPSocket == INVALID_SOCKET)
	{
		std::cerr << "socket() failed." << std::endl;
		freeaddrinfo(TCPinfo);
		WSACleanup();
		return 2;
	}

	errorCode = connect(
		TCPSocket,
		TCPinfo->ai_addr,
		static_cast<int>(TCPinfo->ai_addrlen));
	if (errorCode == SOCKET_ERROR)
	{
		std::cerr << "connect() failed." << std::endl;
		freeaddrinfo(TCPinfo);
		closesocket(TCPSocket);
		WSACleanup();
		return 3;
	}
	freeaddrinfo(TCPinfo);


	/// UDP

	// -------------------------------------------------------------------------
	// Resolve own host name into IP addresses (in a singly-linked list).
	//
	// getaddrinfo()
	// -------------------------------------------------------------------------

	// Object hints indicates which protocols to use to fill in the info.
	addrinfo UDPhints{};
	SecureZeroMemory(&UDPhints, sizeof(UDPhints));
	UDPhints.ai_family = AF_INET;			// IPv4
	// For UDP use SOCK_DGRAM instead of SOCK_STREAM.
	UDPhints.ai_socktype = SOCK_DGRAM;		// Best effort
	// Could be 0 for autodetect, but best effort over IPv4 is always UDP.
	UDPhints.ai_protocol = IPPROTO_UDP;	// UDP

	addrinfo* UDPinfo = nullptr;
	errorCode = getaddrinfo(nullptr, UDPClientPort.c_str(), &UDPhints, &UDPinfo);
	if ((errorCode) || (UDPinfo == nullptr))
	{
		std::cerr << "getaddrinfo() failed." << std::endl;
		WSACleanup();
		return errorCode;
	}


	// Creation of UDP socket
	SOCKET UDPsocket = socket(
		UDPhints.ai_family,
		UDPhints.ai_socktype,
		UDPhints.ai_protocol);
	if (UDPsocket == INVALID_SOCKET)
	{
		std::cerr << "socket() failed." << std::endl;
		freeaddrinfo(UDPinfo);
		WSACleanup();
		return 2;
	}

	sockaddr_in  clientUDPAddr;
	memset(&clientUDPAddr, 0, sizeof(clientUDPAddr));
	clientUDPAddr.sin_family = AF_INET;
	clientUDPAddr.sin_port = htons(static_cast<u_short>(std::stoul(UDPClientPort)));
	//inet_pton(AF_INET, ServerIP.c_str(), &(clientUDPAddr.sin_addr));

	errorCode = bind(
		UDPsocket,
		(sockaddr*)&clientUDPAddr,
		sizeof(clientUDPAddr));
	if (errorCode != NO_ERROR)
	{
		std::cerr << "bind() failed." << std::endl;
		closesocket(UDPsocket);
		freeaddrinfo(UDPinfo);
		UDPsocket = INVALID_SOCKET;
		return 2;
	}
	freeaddrinfo(UDPinfo);
	// -------------------------------------------------------------------------
	// Send some text.
	//
	// send()
	// -------------------------------------------------------------------------

	// as specified in brief for quit and echo 
	 //uint8_t QUITID = 01, ECHOID = 02;
	 std::thread receiver(receive, TCPSocket, UDPsocket);
	 constexpr size_t BUFFER_SIZE = 1000;
	 std::string input{};
	 bool first = true, quit = false; //check if first iteration as it will always be an empty input in the first iteration
	 while (true) //to keep getting input from user or file
	 {
		std::getline(std::cin, input);
		std::string output{};
		if (!input.size() && first) //if input is empty, continue waiting for input
		{
			first = false; // only valid for the first iteration
			continue;
		}
 		//processing
 		else if ("/q" == input) // if user indicate to quit, just indicate quit id. no message/length required
 		{
			output += REQ_QUIT;
			std::cout << "disconnection..." << std::endl;
 		}
		else if(input == "/l")
		{
			output += REQ_LISTFILES;
		}
		else if (input.substr(0, 3) == "/d " && input.size() > 3)
		{
			// sample cmd: "/d 192.168.0.98:9010 filelist.cpp"
			output += REQ_DOWNLOAD; // cmdid 1 byte

			// Setting up of ipaddress and port number
			input = input.substr(3); // get rid of command id and preceding space
			std::istringstream iss{ input }; 
			std::string IPPortPair{}, portNum{}, filePath{};
			iss >> IPPortPair; //get IP and port number as a pair
			std::string IP{ IPPortPair.substr(0, IPPortPair.find(':')) }; //parse them to ip and UDP port number
			input = input.substr(input.find(':') + 1); //get the message, getting rid of 1 space meant to distinguish the port number and message. All preceding spaces are included in the message
			for (size_t i{}; i < input.size(); ++i) //get the port number. Some weird edge case to do it like this
			{
				if (isdigit(input[i]))
				{
					portNum += input[i];
				}
			}
			filePath = input.substr(input.find(' ') + 1); //get the message. weird edge case to match example....
			uint32_t messageSz = static_cast<uint32_t>(htonl(static_cast<u_long>(filePath.size())));
			// ip address
			sockaddr_in Ipbinary{};
			inet_pton(AF_INET, IP.c_str(), &(Ipbinary.sin_addr));
			output.append(reinterpret_cast<char*>(&Ipbinary.sin_addr.S_un.S_addr), sizeof(Ipbinary.sin_addr.S_un.S_addr));
			// port
			u_short port = ntohs(static_cast<u_short>(std::stoul(portNum)));
			output.append(reinterpret_cast<char*>(&port), sizeof(port));
			// file name length
			output.append(reinterpret_cast<char*>(&messageSz), sizeof(messageSz));
			// file name
			output += filePath;
			g_fileName = filePath;
		}
		else 
		{
			output += UNKNOWN;
		}

 		// send 		
 		int bytesSent = send(TCPSocket, output.c_str(), static_cast<int>(output.length()), 0); //send the message out
 		if (bytesSent == SOCKET_ERROR) //check for error
 		{
			int errorCode = WSAGetLastError();
 			std::cerr << "send() failed with error code: " << errorCode << std::endl;
 		}
		
		if (output[0] == REQ_QUIT || output[0] == UNKNOWN) break;
	 }

	 receiver.join();
	 errorCode = shutdown(TCPSocket, SD_SEND); //shutdown by telling it to close off from reading and writing
	 if (errorCode == SOCKET_ERROR) //error checking
	 {
		 std::cerr << "shutdown() failed." << std::endl;
	 }

	 closesocket(UDPsocket);
	 closesocket(TCPSocket); //close socket fr
	WSACleanup(); //goodnight 
}

void receive(SOCKET TCPsocket, SOCKET UDPsocket) {

	// Enable non-blocking I/O on a socket.
	u_long enable = 1;
	ioctlsocket(TCPsocket, FIONBIO, &enable);

	while (true) 
	{
		// receiving TCP
		constexpr size_t BUFFER_SIZE_TCP = 1000;
		char buffer_TCP[BUFFER_SIZE_TCP]{};
		const int bytesReceived_TCP = recv(TCPsocket, buffer_TCP, BUFFER_SIZE_TCP - 1, 0); //receive echo'ed text and header information
		std::string message{};
		if (bytesReceived_TCP == SOCKET_ERROR) //check for error
		{
			size_t errorCode = WSAGetLastError();
			if (errorCode == WSAEWOULDBLOCK)
			{
				// A non-blocking call returned no data; sleep and try again.
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(200ms);
				continue;	
			}
		}
		else if (bytesReceived_TCP == 0) //check if not receiving any messages
		{
			break;
		}
		else //receiving messages from server. to process
		{
			buffer_TCP[bytesReceived_TCP] = '\0';
			std::string text(buffer_TCP, bytesReceived_TCP);

			if (text[0] == RSP_DOWNLOAD) // request echo from server, to send back message with response echo code
			{
				
				u_long IP = Utils::StringTo_ntohl(text.substr(1, 4));
				u_short portNum = Utils::StringTo_ntohs(text.substr(5, 2));
				u_long sessionID = Utils::StringTo_ntohl(text.substr(7, 4)); // session id
				std::string fileLength = text.substr(11); // file length? brief never specify btyes

				//connect to server (optional)
				struct sockaddr_in serverAddress;
				(void)memset(&serverAddress, 0, sizeof(serverAddress));
				serverAddress.sin_family = AF_INET;
				serverAddress.sin_addr.S_un.S_addr = htonl(IP);
				serverAddress.sin_port = htons(portNum);

				if (connect(UDPsocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
					std::cerr << "connect() failed." << std::endl;
					closesocket(UDPsocket);
					WSACleanup();
					break;
				}
				sockaddr_in sin;
				int len = sizeof(sin);
				getsockname(UDPsocket, (struct sockaddr*)&sin, &len);
				char clientIP[INET_ADDRSTRLEN]; //set buffer to be a macro that decides the length based on the connection type eg ipv4, ipv6 etc etc
				inet_ntop(AF_INET, &sin.sin_addr, clientIP, INET_ADDRSTRLEN); //getting IP address of client with IPV4

				std::cout << std::endl;
				std::cout << "==========RECV START==========" << std::endl;
				std::cout << "Now listening for messages on: " << clientIP << ':' << ntohs(sin.sin_port) << '\n';
				std::cout << "Session ID: " << sessionID << std::endl;

				/// UDP SESSION START ACK
				std::cout << "Start UDP session...\n";
				// send the acknowledgement to server to start the udp session 
				const int bytesSent = sendto(UDPsocket, Packet::GetStartPacket().c_str(), static_cast<int>(Packet::GetStartPacket().size()), 0, (sockaddr*)&serverAddress, sizeof(serverAddress));
				if (bytesSent == SOCKET_ERROR)
				{
					int error = WSAGetLastError();
					std::cerr << "send() failed." << std::endl;
					break;
				}
				std::filesystem::path filePath(g_downloadPath + "\\" + g_fileName);
				std::vector<Packet> recievedPackets;
				constexpr size_t BUFFER_SIZE_UDP = PACKET_SIZE + 18;
				u_long sequenceNo{};
				auto cmp = [](Packet lhs, Packet rhs) { return lhs.SequenceNo > rhs.SequenceNo; };
				std::priority_queue < Packet, std::vector<Packet>, decltype(cmp)> packetBuffer(cmp);
				/// UDP SESSSION START
				while (true)
				{
					char buffer_UDP[BUFFER_SIZE_UDP]{};

					int size = sizeof(serverAddress);
					int bytesRecieved_UDP = recvfrom(UDPsocket, buffer_UDP, BUFFER_SIZE_UDP - 1, 0, (sockaddr*)&serverAddress, &size);
					if (bytesRecieved_UDP == SOCKET_ERROR)
					{
						std::cout << WSAGetLastError();
						std::cout << "recvfrom() failed.\n";
						break;
					}
					else if (bytesRecieved_UDP == 0) //check if not receiving any messages
					{
						std::cout << "No bytes have been recieved.\n";
						break;
					}
					else
					{
						buffer_UDP[bytesRecieved_UDP] = '\0';
						text = std::string(buffer_UDP, bytesRecieved_UDP);
						
						if (text[0] == static_cast<u_char>(FLGID::FIN))
						{
							std::cout << "End packet recieved\n";
							break;
						}
						else if (text[0] == static_cast<u_char>(FLGID::FILE))
						{
							Packet filePacket = Packet::DecodePacket_ntohl(text);

							/// RESEND ACKS in the event of packet loss
							if (filePacket.SequenceNo < sequenceNo) // if the file has been added before
							{
								std::cout << "ACK [" << filePacket.SequenceNo << "] resent.\n";
								std::string resendAkString = Packet(filePacket.SessionID, filePacket.SequenceNo).GetBuffer_htonl();
								const int bytesSent = sendto(UDPsocket, resendAkString.c_str(), static_cast<int>(resendAkString.size()), 0, (sockaddr*)&serverAddress, size);
								if (bytesSent == SOCKET_ERROR)
								{
									std::cout << WSAGetLastError();
									std::cerr << " send() failed." << std::endl;
									break;
								}
								continue;
							}
							packetBuffer.push(filePacket);
							std::cout << "Packet [" << filePacket.SequenceNo << "] with SessionID [" << filePacket.SessionID << "] recieved.\n";
							// if the sequenceNo is correct
							while (!packetBuffer.empty() && packetBuffer.top().SequenceNo == sequenceNo)
							{

								// Create ACK & Append
								recievedPackets.push_back(packetBuffer.top());
								Packet ack(packetBuffer.top().SessionID, sequenceNo);
								packetBuffer.pop();
								++sequenceNo;
								
								// Loss of acks
								if ((static_cast<float>(rand()) / RAND_MAX <= g_packLossRate) && sequenceNo == recievedPackets.size())
								{
									std::cout << "ACK [" << filePacket.SequenceNo << "] with SessionID [" << filePacket.SessionID << "] lost.\n";
									continue;
								}

								std::string ackString = ack.GetBuffer_htonl();
								const int bytesSent = sendto(UDPsocket, ackString.c_str(), static_cast<int>(ackString.size()), 0, (sockaddr*)&serverAddress, size);
								if (bytesSent == SOCKET_ERROR)
								{
									std::cout << WSAGetLastError();
									std::cerr << " send() failed." << std::endl;
									break;
								}
								std::cout << "ACK [" << filePacket.SequenceNo << "] with SessionID [" << filePacket.SessionID << "] sent.\n";
								
							}
						}
					}
				}
				UnpackToFile(recievedPackets, filePath);
				std::cout << "Download complete\n";
				std::cout << "==========RECV END==========" << std::endl;
				continue;
			}
			else if (text[0] == RSP_LISTFILES) 
			{
				u_short fileCount = Utils::StringTo_ntohs(text.substr(1, 2)); // number of files
				u_long listLength = Utils::StringTo_ntohl(text.substr(3, 4)); // total number of bytes for length+name

				message += "\n# of Files: " + std::to_string(fileCount) + '\n';
				for (size_t i{}, offset{}; i < fileCount; ++i)
				{
					u_long fileNameLength = Utils::StringTo_ntohl(text.substr(7 + offset, 4)); // offset by first 7 bytes
					std::string fileName = text.substr(11 + offset, fileNameLength); // offset by first 7 bytes + 4 bytes (fileNameLength)
					message += std::to_string(i + 1) + "-th file: " + fileName + '\n';
					offset += static_cast<size_t>(fileNameLength) + 4;
				}
			}
			else if (text[0] == DOWNLOAD_ERROR)
			{
				message = "Download error";
			}

			std::cout << "==========RECV START==========" << std::endl;
			std::cout << message;
			std::cout << "==========RECV END==========" << std::endl;
		}
	}
}
