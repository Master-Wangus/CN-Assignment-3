/* Start Header
*****************************************************************/
/*!
\file echoserver.cpp
\author Koh Wei Ren, weiren.koh, 2202110
		Pang Zhi Kai, p.zhikai, 2201573
		
\par weiren.koh@digipen.edu
	 p.zhikai@digipen.edu
\date 03/03/2024
\brief This source file implements a multithreaded server that accepts multiple connection to client but only shuts down
when told to. Disconnecting clients will not shut the server down.
Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
*******************************************************************/

//*******************************************************************************
// * A multi-threaded TCP/IP server application
// ******************************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Windows.h"		// Entire Win32 API...
 // #include "winsock2.h"	// ...or Winsock alone
#include "ws2tcpip.h"		// getaddrinfo()
#include <thread>
#include "taskqueue.h"
#include <filesystem>
#include <iostream>
#include <fstream>
 
bool execute(SOCKET clientSocket);
void disconnect(SOCKET& listenerSocket);

// Tell the Visual Studio linker to include the following library in linking.
// Alternatively, we could add this file to the linker command-line parameters,
// but including it in the source code simplifies the configuration.
#pragma comment(lib, "ws2_32.lib")

#include <iostream>			// cout, cerr
#include <string>			// string
#include <vector>
#include <queue>
#include <unordered_map>
#include "Utils.h"
#include "packet.h"


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

std::vector<std::pair<sockaddr_in, SOCKET>> connectedSockets{};
uint16_t UDPPortNumber{}, TCPPortNumber{};
SOCKET listenerSocket{}, udpSocket{};
std::string g_DownloadRepo{};
static u_long g_SessionID{};
float g_PackLossRate{};
size_t g_WindowSize{};
DWORD g_AckTimer{};
std::unordered_map<u_long, std::priority_queue<u_long, std::vector<u_long>, std::greater<u_long>>> g_Packets;

int main()
{
	// -------------------------------------------------------------------------
	// Start up Winsock, asking for version 2.2.
	//
	// WSAStartup()
	// -------------------------------------------------------------------------

	// This object holds the information about the version of Winsock that we
	// are using, which is not necessarily the version that we requested.
	WSADATA wsaData{};

	// Initialize Winsock. You must call WSACleanup when you are finished.
	// As this function uses a reference counter, for each call to WSAStartup,
	// you must call WSACleanup or suffer memory issues.
	int errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (errorCode != NO_ERROR)
	{
		std::cerr << "WSAStartup() failed." << std::endl;
		return errorCode;
	}

	// to ask user for port number and convert it to string
	//uint16_t TCPPortNumber{};
	std::cout << "Server TCP Port Number: ";
	if (!(std::cin >> TCPPortNumber))
	{
		WSACleanup();
		return 0;
	}
	std::cout << std::endl;
	std::string TCPportString{ std::to_string(TCPPortNumber) };

	//uint16_t UDPPortNumber{};
	std::cout << "Server UDP Port Number: ";
	if (!(std::cin >> UDPPortNumber))
	{
		WSACleanup();
		return 0;
	}
	std::cout << std::endl;
	std::string UDPportString{ std::to_string(UDPPortNumber) };

	std::ifstream fs("Config.txt", std::ios::binary); // open the config file

	if (fs.is_open())
	{
		std::cout << "Loading Server paramters from config file" << std::endl;
	}

	//std::string parse{};
	//std::getline(fs, parse);
	////downLoadRepo = parse.substr(parse.find_first_of(" ") + 1);
	//g_DownloadRepo = parse;

	while (!std::filesystem::exists(g_DownloadRepo))
	{
		std::getline(std::cin, g_DownloadRepo);
		if (std::filesystem::exists(g_DownloadRepo)) break;
		std::cout << "Enter a valid download repository: ";
	}
	std::cout << std::endl;

	std::cout << "Window Size [1, 100]: ";
	std::cin >> g_WindowSize;
	std::cout << std::endl;


	std::cout << "Packet loss rate [0.f, 1.f]: ";
	std::cin >> g_PackLossRate;
	std::cout << std::endl;

	std::cout << "ACK Timer [10ms, 500ms]: ";
	std::cin >> g_AckTimer;
	std::cout << std::endl;

	// -------------------------------------------------------------------------
	// Resolve own host name into IP addresses (in a singly-linked list).
	//
	// getaddrinfo()
	// -------------------------------------------------------------------------

	// Object hints indicates which protocols to use to fill in the info.
	addrinfo TCPhints{};
	SecureZeroMemory(&TCPhints, sizeof(TCPhints));
	TCPhints.ai_family = AF_INET;			// IPv4
	// For UDP use SOCK_DGRAM instead of SOCK_STREAM.
	TCPhints.ai_socktype = SOCK_STREAM;	// Reliable delivery
	// Could be 0 for autodetect, but reliable delivery over IPv4 is always TCP.
	TCPhints.ai_protocol = IPPROTO_TCP;	// TCP
	// Create a passive socket that is suitable for bind() and listen().
	TCPhints.ai_flags = AI_PASSIVE;

	char hostName[INET_ADDRSTRLEN]{};
	gethostname(hostName, INET_ADDRSTRLEN);
	addrinfo* TCPinfo = nullptr;
	errorCode = getaddrinfo(hostName, TCPportString.c_str(), &TCPhints, &TCPinfo);
	if ((errorCode) || (TCPinfo == nullptr))
	{
		std::cerr << "getaddrinfo() failed." << std::endl;
		WSACleanup();
		return errorCode;
	}

	inet_ntop(AF_INET, &(reinterpret_cast<sockaddr_in *>(TCPinfo->ai_addr))->sin_addr, hostName, INET_ADDRSTRLEN);

	// -------------------------------------------------------------------------
	// Create a socket and bind it to own network interface controller.
	//
	// socket()
	// bind()
	// -------------------------------------------------------------------------

	listenerSocket = socket(
		TCPhints.ai_family,
		TCPhints.ai_socktype,
		TCPhints.ai_protocol);
	if (listenerSocket == INVALID_SOCKET)
	{
		std::cerr << "socket() failed." << std::endl;
		freeaddrinfo(TCPinfo);
		WSACleanup();
		return 1;
	}

	errorCode = bind(
		listenerSocket,
		TCPinfo->ai_addr,
		static_cast<int>(TCPinfo->ai_addrlen));
	if (errorCode != NO_ERROR)
	{
		//std::cerr << "bind() failed." << std::endl;
		closesocket(listenerSocket);
		freeaddrinfo(TCPinfo);
		listenerSocket = INVALID_SOCKET;
	}

	freeaddrinfo(TCPinfo);

	if (listenerSocket == INVALID_SOCKET)
	{
		//std::cerr << "bind() failed." << std::endl;
		WSACleanup();
		return 2;
	}

	// Object hints indicates which protocols to use to fill in the info.
	addrinfo UDPhints{};
	SecureZeroMemory(&UDPhints, sizeof(UDPhints));
	UDPhints.ai_family = AF_INET;			// IPv4
	// For TCP use SOCK_STREAM instead of SOCK_DGRAM.
	UDPhints.ai_socktype = SOCK_DGRAM;		// Best effort
	// Could be 0 for autodetect, but best effort over IPv4 is always UDP.
	UDPhints.ai_protocol = IPPROTO_UDP;	// UDP
	
	addrinfo* UDPinfo = nullptr;
	errorCode = getaddrinfo(hostName, UDPportString.c_str(), &UDPhints, &UDPinfo);
	if ((errorCode) || (UDPinfo == nullptr))
	{
		std::cerr << "getaddrinfo() failed." << std::endl;
		WSACleanup();
		return errorCode;
	}


	udpSocket = socket(
		UDPhints.ai_family,
		UDPhints.ai_socktype,
		UDPhints.ai_protocol);
	if (udpSocket == INVALID_SOCKET)
	{
		std::cerr << "udpSocket creation failed." << std::endl;
		freeaddrinfo(UDPinfo);
		WSACleanup();
		return 1;
	}

	errorCode = bind(
		udpSocket,
		UDPinfo->ai_addr,
		static_cast<int>(UDPinfo->ai_addrlen));
	if (errorCode != NO_ERROR)
	{
		std::cerr << "udBind() failed." << std::endl;
		closesocket(udpSocket);
		freeaddrinfo(UDPinfo);
		WSACleanup();
		return 2;
	}
	freeaddrinfo(UDPinfo);

	std::cout << "\nServer IP Address: " << hostName << std::endl;
	std::cout << "Server TCP Port Number: " << TCPportString << std::endl;
	std::cout << "Server UDP Port Number: " << UDPportString << std::endl;
	std::cout << "Download Repository: " << g_DownloadRepo << std::endl;

	// -------------------------------------------------------------------------
	// Set a socket in a listening mode and accept 1 incoming client.
	//
	// listen()
	// accept()
	// -------------------------------------------------------------------------

	//start from here

	SOCKET clientSocket{};
	sockaddr_in* clientAddr{};
	const auto onDisconnect = [&]() { disconnect(listenerSocket); };
	auto tq = TaskQueue<SOCKET, decltype(execute), decltype(onDisconnect)>{ 10, 20, execute, onDisconnect };
	while (true) //loop until server shutsdown
	{
		errorCode = listen(listenerSocket, SOMAXCONN); // listen for any connections
		if (errorCode != NO_ERROR)
		{
			std::cerr << "listen() failed." << std::endl;
			closesocket(listenerSocket);
			WSACleanup();
			return 3;
		}

		// keep on accepting new client
		sockaddr clientAddress{};
		SecureZeroMemory(&clientAddress, sizeof(clientAddress));
		int clientAddressSize = sizeof(clientAddress);
		clientSocket = accept(listenerSocket, &clientAddress, &clientAddressSize); //acept new client 
		if (clientSocket == INVALID_SOCKET) //error checking
		{
			std::cerr << "accept() failed." << std::endl;
			closesocket(listenerSocket);
			WSACleanup();
			return 4;
		}
		tq.produce(clientSocket);
		
		//print the client IP and port number
		char clientIP[INET_ADDRSTRLEN]; //set buffer to be a macro that decides the length based on the connection type eg ipv4, ipv6 etc etc
		uint16_t clientPort{};
		clientAddr = reinterpret_cast<sockaddr_in*>(&clientAddress);
		inet_ntop(AF_INET, &(clientAddr)->sin_addr, clientIP, INET_ADDRSTRLEN); //getting IP address of client with IPV4
		clientPort = htons(clientAddr->sin_port); //getting client port number
		std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
		std::cout << "\nClient IP Address: " << clientIP << std::endl; //print client ip
		std::cout << "Client Port Number: " << clientPort << std::endl; //print client port number
		connectedSockets.emplace_back(std::make_pair(*clientAddr, clientSocket));
	}


	// -------------------------------------------------------------------------
	// Shut down and close sockets.
	//
	// shutdown()
	// closesocket()
	// -------------------------------------------------------------------------

	shutdown(listenerSocket, SD_BOTH); //close server 
	closesocket(udpSocket);
	closesocket(listenerSocket);


	// -------------------------------------------------------------------------
	// Clean-up after Winsock.
	//
	// WSACleanup()
	// -------------------------------------------------------------------------

	WSACleanup(); // good day
}
 
bool execute(SOCKET clientSocket)
{
	bool stay = true;
	
	// Enable non-blocking I/O on a socket.
	u_long enable = 1;
	ioctlsocket(clientSocket, FIONBIO, &enable);

	constexpr size_t TCPBUFFER_SIZE = 1000; //arbitrary buffer size. could be 1 could be a million
	char inputTCP[TCPBUFFER_SIZE]; //set char buffer as char = uint8_t

	constexpr size_t UDPBUFFER_SIZE = PACKET_SIZE + 18; //arbitrary buffer size. could be 1 could be a million
	char inputUDP[UDPBUFFER_SIZE]; //set char buffer as char = uint8_t


	// UDP 
	std::vector<Packet> filePackets;
	size_t index{};
	u_long currSequence{}, threadSessionID{static_cast<u_long>(-1)};
	std::unordered_map<size_t, std::chrono::high_resolution_clock::time_point> timerBuffer;
	DWORD timeout{}; // in milli
	sockaddr_in clientAddr{}; // Client address UDP
	int clientAddrSize = sizeof(clientAddr);
	int sent = 0; // set sent packets to 0

	while (true) //loop until client disconnects
	{
		/// UDP DOWNLOAD
		if (!filePackets.empty())
		{
			int errorCode = setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
			if (errorCode != NO_ERROR)
			{
				std::cerr << "setsockopt() failed." << std::endl;
				break;
			}

			sockaddr_in randomAddr{}; // Client address UDP
			int randomAddrSize = sizeof(randomAddr);
			const int bytesRecieved = recvfrom(udpSocket,
				inputUDP,
				UDPBUFFER_SIZE - 1,
				0,
				(sockaddr*)&randomAddr,
				&randomAddrSize);
			if (bytesRecieved == SOCKET_ERROR)
			{
				// no response from client
				if (WSAGetLastError() == WSAETIMEDOUT)
				{
					if (currSequence < filePackets.size())
					{
						std::string filePacket = filePackets[currSequence].GetBuffer_htonl();
						/// RETRANSMISSION
						std::cout << "[TIMEOUT] Retransmitting Packet [" << currSequence <<  "] SessionID [" << threadSessionID << "]\n";
						++sent;
						const int bytesSent = sendto(udpSocket, filePacket.c_str(), static_cast<int>(filePacket.size()), 0, (sockaddr*)&clientAddr, clientAddrSize);
						if (bytesSent == SOCKET_ERROR)
						{
							std::cout << WSAGetLastError();
							std::cerr << " send() failed." << std::endl;
							break;
						}
						++currSequence;
					}
					else
					{
						// Tell the client that the download is complete
						std::string endPacket = Packet::GetEndPacket();
						++sent;
						const int bytesSent = sendto(udpSocket, endPacket.c_str(), static_cast<int>(endPacket.size()), 0, (sockaddr*)&clientAddr, clientAddrSize);
						if (bytesSent == SOCKET_ERROR)
						{
							std::cerr << "send() failed." << std::endl;
							break;
						}

						index = 0;
						currSequence = 0;
						timeout = 0;						
						filePackets.clear(); // Reset the download Packets
						std::cout << "Packets Sent in Total: " << sent << std::endl;
						sent = 0;
						std::cout << "==========DOWNLOAD[" << threadSessionID << "] END==========" << std::endl;
					}
					continue;
				}
				std::cout << WSAGetLastError();
				std::cerr << " recvfrom() failed." << std::endl;
				break;
			}
			else if (bytesRecieved == 0)
			{
				std::cerr << "Graceful shutdown." << std::endl;
				break;
			}
			inputUDP[bytesRecieved] = '\0';
			std::string text(inputUDP, bytesRecieved);
			Packet packet = Packet::DecodePacket_ntohl(text);
			if (packet.isACK()) // Client has recieved the packet
			{
				// buffer to check acknolwdgement
				timerBuffer.erase(packet.SequenceNo);
				g_Packets[packet.SessionID].push(packet.SequenceNo);
				std::cout << "Recieved ACK [" << packet.SequenceNo << "] SessionID [" << packet.SessionID << "]\n";
			}
			// Check if there is any pending acknowldgement
			if (!g_Packets[threadSessionID].empty() && currSequence == g_Packets[threadSessionID].top())
			{
				g_Packets[threadSessionID].pop();
				++currSequence;
			}
			else if (!g_Packets[threadSessionID].empty() && currSequence < g_Packets[threadSessionID].top()) // potential packet loss occured
			{
				// check for packet loss
				if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timerBuffer[currSequence]).count() >= timeout)
				{
					g_Packets[threadSessionID].pop();
					std::string filePacket = filePackets[currSequence].GetBuffer_htonl();
					/// RETRANSMISSION
					std::cout << "Retransmitting Packet [" << currSequence << "] SessionID [" << threadSessionID << "]\n";

					++sent;
					const int bytesSent = sendto(udpSocket, filePacket.c_str(), static_cast<int>(filePacket.size()), 0, (sockaddr*)&clientAddr, clientAddrSize);
					if (bytesSent == SOCKET_ERROR)
					{
						std::cout << WSAGetLastError();
						std::cerr << " send() failed." << std::endl;
						break;
					}
					timerBuffer[index] = std::chrono::high_resolution_clock::now();
					continue;
				}
			}
			// Replace filePackets to window size
			if (index < currSequence + g_WindowSize && index < filePackets.size())
			{
				if (static_cast<float>(rand()) / RAND_MAX <= g_PackLossRate) // packet loss check
				{
					std::cout << "Packet [" << index << "] with SessionID [" << threadSessionID << "] lost.\n";
					index++;
					continue;
				}
				std::string filePacket = filePackets[index].GetBuffer_htonl();
				++sent;
				const int bytesSent = sendto(udpSocket, filePacket.c_str(), static_cast<int>(filePacket.size()), 0, (sockaddr*)&clientAddr, clientAddrSize);
				if (bytesSent == SOCKET_ERROR)
				{
					std::cout << WSAGetLastError();
					std::cerr << " send() failed." << std::endl;
					break;
				}
				timerBuffer[index] = std::chrono::high_resolution_clock::now();
				index++;
			}
			

			/// END DOWNLOAD
			if (currSequence == filePackets.size()) // recieved all acks
			{
				// Tell the client that the download is complete
				std::string endPacket = Packet::GetEndPacket();
				++sent;
				const int bytesSent = sendto(udpSocket, endPacket.c_str(), static_cast<int>(endPacket.size()), 0, (sockaddr*)&clientAddr, clientAddrSize);
				if (bytesSent == SOCKET_ERROR)
				{
					std::cerr << "send() failed." << std::endl;
					break;
				}

				timeout = 0;
				index = 0;
				currSequence = 0;
				std::cout << "Packets Sent in Total: " << sent << std::endl;
				sent = 0;
				filePackets.clear(); // Reset the download Packets
				std::cout << "==========DOWNLOAD[" << threadSessionID << "] END==========" << std::endl;
			}
			continue; // loops through the UDP section
		}

		/// TCP reciever
		const int bytesReceived = recv(clientSocket, inputTCP, TCPBUFFER_SIZE - 1, 0);
		if (bytesReceived == SOCKET_ERROR)
		{
			size_t errorCode = WSAGetLastError();
			if (errorCode == WSAEWOULDBLOCK)
			{
				// A non-blocking call returned no data; sleep and try again.
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(200ms);
				continue;
			}
			std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
			std::cerr << "Graceful shutdown." << std::endl;
			break;
		}
		if (bytesReceived == 0)
		{
			break;
		}

		inputTCP[bytesReceived] = '\0';
		std::string text(inputTCP, bytesReceived);
		if (text[0] == REQ_QUIT) //check 1st byte == quit
		{
			break;
		}
		else if (text[0] == REQ_DOWNLOAD) //check 1st byte  == echo
		{
			/// Save UDP proporties
			u_long clientIP = Utils::StringTo_htonl(text.substr(1, 4)); //get the ip of the client requesting UDP file download
			u_short ClientUDPPortNum = Utils::StringTo_htons(text.substr(5, 2));

			// File properties
			u_long fileNameLength{ Utils::StringTo_ntohl(text.substr(7, 4)) }; //get the message length in host order bytes
			std::string filename{ text.substr(11) }; //get the message

			std::string output{};
			std::filesystem::path filePath = std::filesystem::path(g_DownloadRepo) / filename;
			if (std::filesystem::exists(filePath)) //file exist, sending client UDP details
			{
				output += RSP_DOWNLOAD;
				sockaddr_in serverAddr{};
				int addrSize = sizeof(serverAddr);
				getsockname(listenerSocket, (struct sockaddr*)&serverAddr, &addrSize);
				
				// IP
				output.append(reinterpret_cast<char*>(&serverAddr.sin_addr.S_un.S_addr), sizeof(serverAddr.sin_addr.S_un.S_addr));
				u_short serverPort = htons(UDPPortNumber);
				// Port Number
				output.append(reinterpret_cast<char*>(&serverPort), sizeof(serverPort));
				// Session ID
				u_long sessionID = htonl(g_SessionID);
				output.append(reinterpret_cast<char*>(&sessionID), sizeof(sessionID));
				// FileLength
				std::string fileLength = std::to_string(std::filesystem::file_size(filePath));
				output += fileLength;

				// Ready all UDP variables
				filePackets =  PackFromFile(g_SessionID, filePath);
				threadSessionID = g_SessionID;
				timeout = g_AckTimer;
				++g_SessionID;
				

				SecureZeroMemory(&clientAddr, sizeof(clientAddr));
				clientAddr.sin_family = AF_INET;
				clientAddr.sin_addr.S_un.S_addr = htonl(clientIP);
				clientAddr.sin_port = htons(ClientUDPPortNum);

				// Print out ip and Session
				char clientIp_Print[INET_ADDRSTRLEN]; //set buffer to be a macro that decides the length based on the connection type eg ipv4, ipv6 etc etc
				inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp_Print, INET_ADDRSTRLEN); //set buffer to be a macro that decides the length based on the connection type eg ipv4, ipv6 etc etc
				std::cout << "==========DOWNLOAD[" << threadSessionID << "] START==========" << std::endl;
				std::cout << clientIp_Print << ':' << ntohs(clientAddr.sin_port) << " SessionID [" << threadSessionID << "]\n";
				std::cout << std::endl;

			}
			else // file does not exist
			{
				output += DOWNLOAD_ERROR; 
			}

			send(clientSocket, output.c_str(), static_cast<int>(output.size()), 0);
		}
		else if(text[0] == REQ_LISTFILES)
		{
			std::string listOfFiles{ RSP_LISTFILES };

			u_short NumOfFiles{};
			u_long lengthOfFileList{};
			std::vector<std::string>FileNames{};
			for (auto const& file : std::filesystem::directory_iterator{ g_DownloadRepo })
			{
				++NumOfFiles;
				std::string fileName = file.path().filename().string();
				FileNames.emplace_back(fileName);
				lengthOfFileList += static_cast<u_long>(fileName.size()) + 4; // calculate the length of file list
			}

			listOfFiles += Utils::htonsToString(NumOfFiles);
			listOfFiles += Utils::htonlToString(lengthOfFileList);

			for (std::string const& i : FileNames)
			{
				u_long fileNameLength = htonl(static_cast<u_long>(i.size()));
				listOfFiles.append(reinterpret_cast<char*>(&fileNameLength), sizeof(fileNameLength));;
				listOfFiles += i;
			}

			send(clientSocket, listOfFiles.c_str(), static_cast<int>(listOfFiles.size()), 0);
		}
		else
		{
			std::cout << "Graceful shutdown" << std::endl;
			break;
		}
	}

	sockaddr_in clientAddress{};
	socklen_t clientAddrLen = sizeof(clientAddress);
	getpeername(clientSocket, (struct sockaddr*)&clientAddress, &clientAddrLen);
	
	{
		std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
		for (size_t i{}; i < connectedSockets.size(); ++i) {
			std::pair<sockaddr_in, SOCKET> tmp = connectedSockets[i];
			if ((tmp.first.sin_addr.S_un.S_addr == clientAddr.sin_addr.S_un.S_addr && tmp.first.sin_port == clientAddress.sin_port) && tmp.second == clientSocket) {
				connectedSockets.erase(connectedSockets.begin() + i);
			}
		}
	}

	return stay;
}

void disconnect(SOCKET& listenerSocket)
{
	//std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
	//std::cout << "Graceful shutdown" << std::endl;
	if (listenerSocket != INVALID_SOCKET)
	{
		shutdown(listenerSocket, SD_BOTH);
		closesocket(listenerSocket);
		listenerSocket = INVALID_SOCKET;
	}
}