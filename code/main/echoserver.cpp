/* Start Header
*****************************************************************/
/*!
\file echoserver.cpp
\author koh wei ren, weiren.koh, 2202110
\par weiren.koh@digipen.edu
\date 03/03/2024
\brief This source file implements a multithreaded server that accepts multiple connection to client but only shuts down
when told to. Disconnecting clients will not shut the server down.
Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
*******************************************************************/

constexpr size_t WINDOW_SIZE = 3;
constexpr float TIME_OUT = 10.f;
constexpr size_t BUFFER_SIZE = 1000; //arbitrary buffer size. could be 1 could be a million
#define ELAPSED_TIME(end, start) std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()

///*******************************************************************************
// * A multi-threaded TCP/IP server application
// ******************************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Tell the Visual Studio linker to include the following library in linking.
// Alternatively, we could add this file to the linker command-line parameters,
// but including it in the source code simplifies the configuration.
#pragma comment(lib, "ws2_32.lib")


#include "Windows.h"		// Entire Win32 API...
#include "ws2tcpip.h"		// getaddrinfo()

bool execute(SOCKET clientSocket);
void disconnect(SOCKET& listenerSocket);

#include "taskqueue.h"
#include "Utils.h"
#include "packet.h"
#include <fstream>
#include <chrono>

std::vector<std::pair<sockaddr_in, SOCKET>> connectedSockets{};
std::vector<ULONG> SessionsInProgress{};
uint16_t UDPPortNumber{}, TCPPortNumber{};
SOCKET listenerSocket{};
std::mutex SessionMutex;

/*!***********************************************************************
\brief
To convert the string taken as input which maybe network order and convert it to a unsigned long
for ntohl to process.
\param[in] std::vector<std::pair<sockaddr_in, SOCKET>>const& vec
Vector of sockets
\param[in] std::pair<long, uint16_t> addr
IP and port to search for
\return
The socket found else -1
*************************************************************************/
SOCKET SearchConnectedSockets(std::vector<std::pair<sockaddr_in, SOCKET>>const& vec, std::pair<long, uint16_t> addr) {
	for (std::pair<sockaddr_in, SOCKET> const& i : vec) {
		if (i.first.sin_addr.S_un.S_addr == addr.first && ntohs(i.first.sin_port) == addr.second) {
			return i.second;
		}
	}
	return -1;
}

enum CMDID 
{
	UNKNOWN = (unsigned char)0x0,//not used
	REQ_QUIT = (unsigned char)0x1,
	REQ_DOWNLOAD = (unsigned char)0x2,
	RSP_DOWNLOAD = (unsigned char)0x3,
	REQ_LISTFILES = (unsigned char)0x4,
	RSP_LISTFILES = (unsigned char)0x5,
	CMD_TEST = (unsigned char)0x20,//not used
	DOWNLOAD_ERROR = (unsigned char)0x30
};

struct Session
{
	sockaddr_in ClientSockAddess{}; // All session info required for sending/receving packets
	SOCKET ServerUDPSocket{};

	ULONG SessionID{};
	std::filesystem::path FilePath{};
	ULONG FileLength{};
	USHORT SourcePort{};
	USHORT DestPort{};

	// All session info for window
	std::chrono::high_resolution_clock::time_point SentTime[WINDOW_SIZE]{};
	bool AckMask[WINDOW_SIZE]{};
	bool SentMask[WINDOW_SIZE]{};
	int LastAckRecv{};
	int LastFrameSent{};

	std::mutex DownloadMutex;
	std::vector<Packet> PacketsToSend{};

	Session() = default;
	Session(sockaddr_in client, ULONG sessionID, std::filesystem::path filePath, ULONG fileLength, USHORT sourcePort, USHORT destPort)
		: ClientSockAddess(client), SessionID(sessionID), FilePath(filePath), FileLength(fileLength), SourcePort(sourcePort), DestPort(destPort), PacketsToSend(PackFromFile(SessionID, FilePath))

	{
		ServerUDPSocket = socket(
			AF_INET, // IPv4
			SOCK_DGRAM, // Best effort
			IPPROTO_UDP); // Could be 0 for autodetect, but best effort over IPv4 is always UDP.

		if (ServerUDPSocket == INVALID_SOCKET)
		{
			std::cerr << "udpSocket creation failed." << std::endl;
			WSACleanup();
		}

		int errorCode = bind(
			ServerUDPSocket,
			(const struct sockaddr*)&ClientSockAddess,
			sizeof(ClientSockAddess));
		if (errorCode != NO_ERROR)
		{
			std::cerr << "udBind() failed." << std::endl;
			closesocket(ServerUDPSocket);
			WSACleanup();
		}
	}

	~Session()
	{
		closesocket(ServerUDPSocket);
	}

	void Execute();
	void ListenForAck();
};

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
	std::string TCPportString{ std::to_string(TCPPortNumber) };

	//uint16_t UDPPortNumber{};
	std::cout << "Server UDP Port Number: ";
	if (!(std::cin >> UDPPortNumber))
	{
		WSACleanup();
		return 0;
	}
	std::string UDPportString{ std::to_string(UDPPortNumber) };

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

	std::cout << "\nServer IP Address: " << hostName << std::endl;
	std::cout << "Server TCP Port Number: " << TCPportString << std::endl;
	std::cout << "Server UDP Port Number: " << UDPportString << std::endl;

	// -------------------------------------------------------------------------
	// Set a socket in a listening mode and accept 1 incoming client.
	//
	// listen()
	// accept()
	// -------------------------------------------------------------------------


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
	std::string downLoadRepo{};
	std::ifstream fs("Config.txt", std::ios::binary); // open the config file

	if (fs.is_open())
	{
		std::cout << "Loading Server paramters from config file" << std::endl;
	}

	std::string parse{};
	std::getline(fs, parse);
	downLoadRepo = parse;

	while (downLoadRepo.empty() || !std::filesystem::exists(downLoadRepo))
	{
		std::cout << "Enter a valid download repository: ";
		std::getline(std::cin, downLoadRepo);
	}
	// Enable non-blocking I/O on a socket.
	u_long enable = 1;
	ioctlsocket(clientSocket, FIONBIO, &enable);

	constexpr size_t TCPBUFFER_SIZE = 1000; //arbitrary buffer size. could be 1 could be a million
	char inputTCP[TCPBUFFER_SIZE]; //set char buffer as char = uint8_t
	bool isDownloading = false;

	while (true) //loop until client disconnects
	{
		Session* CurrentThreadSession = nullptr;
		/// UDP DOWNLOAD
		if (isDownloading && CurrentThreadSession != nullptr)
		{
			// we call the sesson download function 
			CurrentThreadSession->Execute();

			// return memory and set to false
			delete CurrentThreadSession;
			isDownloading = false;
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
			std::string clientIP{ text.substr(1, 4) }; //get the ip of the client requesting UDP file download
			USHORT ClientUDPportNum{ Utils::StringTo_ntohs(text.substr(5, 2)) }; //get the client UDP port numba in host order bytes
			ULONG fileNameLength{ Utils::StringTo_ntohl(text.substr(7, 4)) }; //get the message length in host order bytes
			std::string filename{ text.substr(11) }; //get the message

			std::string output{};
			std::filesystem::path filePath = std::filesystem::path(downLoadRepo) / filename;
			if (std::filesystem::exists(filePath)) //file exist, sending client UDP details
			{
				output += RSP_DOWNLOAD;
				sockaddr_in serverAddr{};
				int addrSize = sizeof(serverAddr);
				getsockname(listenerSocket, (struct sockaddr*)&serverAddr, &addrSize);
				
				output.append(reinterpret_cast<char*>(&serverAddr.sin_addr.S_un.S_addr), sizeof(serverAddr.sin_addr.S_un.S_addr));
				u_short clientPort = htons(UDPPortNumber);
				output.append(reinterpret_cast<char*>(&clientPort), sizeof(clientPort));

				std::string fileLength = std::to_string(std::filesystem::file_size(filePath));
				output += fileLength;

				sockaddr_in clientAddr{};
				SecureZeroMemory(&clientAddr, sizeof(clientAddr));
				socklen_t clientAddrLen = sizeof(clientAddr);
				getpeername(clientSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
				std::lock_guard<std::mutex> lock(SessionMutex);
				ULONG newSessionID = Utils::GenerateUniqueULongKey(SessionsInProgress);

				// we allocate memory for a new session
				// sessions are thread bound and will never be accessed from another thread so we will keep all responsibility 
				// to the thread running the session
				CurrentThreadSession = new Session(clientAddr, newSessionID, filePath, static_cast<ULONG>(std::filesystem::file_size(filePath)), (USHORT)UDPPortNumber, ClientUDPportNum);
				isDownloading = true;
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
			for (auto const& file : std::filesystem::directory_iterator{ downLoadRepo })
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

	sockaddr_in clientAddr{};
	socklen_t clientAddrLen = sizeof(clientAddr);
	getpeername(clientSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
	
	{
		std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
		for (size_t i{}; i < connectedSockets.size(); ++i) 
		{
			std::pair<sockaddr_in, SOCKET> tmp = connectedSockets[i];
			if ((tmp.first.sin_addr.S_un.S_addr == clientAddr.sin_addr.S_un.S_addr && tmp.first.sin_port == clientAddr.sin_port) && tmp.second == clientSocket) {
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

void Session::Execute() // send thread function
{
	if (PacketsToSend.size() <= 0) // file reading has encountered an error
	{
		return;
	}
	// Wesley: to be pushed to taskqueue if possible
	// create a thread to handle all receiving acks until download is done
	std::thread ReceiverThread(&Session::ListenForAck, this);

	// we will get packets in waves of WINDOW_SIZE
	for (int currentPacketNo{}; currentPacketNo < PacketsToSend.size(); currentPacketNo+= WINDOW_SIZE)
	{
		// these arrays are alr set sized to WINDOW_SIZE
		for (size_t i = 0; i < WINDOW_SIZE; i++)
		{
			// We set them to false to prep
			AckMask[i] = false;
			SentMask[i] = false;
		}
		// LAR guarantees all packets are successfully received up to this point
		LastAckRecv = -1;
		// LFS is the last packet sent
		LastFrameSent = LastAckRecv + WINDOW_SIZE;

		bool Sent = false; // sending the whole window
		while (!Sent)
		{
			// if acknowledged first packet in window, shift
			if (AckMask[0]) 
			{
				// we shift to the right once
				int shift = 1;
				for (int i = 1; i < WINDOW_SIZE; i++)
				{
					// we shift some more if ack
					if (!AckMask[i]) break;
					++shift;
				}

				// we set the previous ack slots to the non ack packats
				for (int i = 0; i < WINDOW_SIZE - shift; i++)
				{
					SentMask[i] = SentMask[i + shift];
					AckMask[i] = AckMask[i + shift];
					SentTime[i] = SentTime[i + shift];
				}

				// then we set the new packets after to be ready for sending
				for (int i = WINDOW_SIZE - shift; i < WINDOW_SIZE; i++) 
				{
					SentMask[i] = false;
					AckMask[i] = false;
				}
				// we guaranteed ack, so we shift LAR
				LastAckRecv += shift;
				// we have sent the packets in the window so we set LFS to the end of the window
				LastFrameSent = LastAckRecv + WINDOW_SIZE;
			}

			// now we handle packet sending
			int CurrentSequenceNo = 0;
			for (int i = 0; i < WINDOW_SIZE; i++)
			{
				CurrentSequenceNo = LastAckRecv + i + 1;

				if (CurrentSequenceNo < PacketsToSend.size())
				{
					// we send a packet only when:
					// condition 1: not sent yet
					// condition 2: packet has not been acknowledged and time elapsed has been more than timeout
					// or... look at ListenForAck
					if (!SentMask[i] || (!AckMask[i] && (ELAPSED_TIME(std::chrono::high_resolution_clock::now(), SentTime[i]) > TIME_OUT)))
					{
						Segment seggs(SourcePort, DestPort, PacketsToSend[CurrentSequenceNo]);

						sendto(ServerUDPSocket, seggs.GetNetworkBuffer().c_str(), seggs.Length, 0, (struct sockaddr*)(&ClientSockAddess), sizeof(ClientSockAddess));
						SentMask[i] = true;
						SentTime[i] = std::chrono::high_resolution_clock::now();
					}
				}
				else
					break; // we have hit the end of the packets
			}

			/* Move to next buffer if all frames in current buffer has been acked */
			if (LastAckRecv >= CurrentSequenceNo - 1)
				Sent = true;
		}
	}
	ReceiverThread.detach();
}

void Session::ListenForAck()
{
	while (true) 
	{
		char input[BUFFER_SIZE]; //set char buffer as char = uint8_t
		int clientSize = sizeof(ClientSockAddess);
		const int bytesRecieved = recvfrom(ServerUDPSocket, input, BUFFER_SIZE,
			MSG_WAITALL, (struct sockaddr*)&ClientSockAddess,
			&clientSize);
		bool isChecksumBroken = false;
		bool isAcked = false; // determines if this packet is ACK or NAK
		ULONG sequenceNo = 0;

		std::lock_guard<std::mutex> lock(SessionMutex);
		if (IfAckReturnSequence(std::string(input, bytesRecieved), isChecksumBroken, isAcked, sequenceNo))
		{
			if (sequenceNo > static_cast<ULONG>(LastAckRecv) && sequenceNo <= static_cast<ULONG>(LastFrameSent) && !isChecksumBroken)
			{
				if (isAcked)
				{
					AckMask[sequenceNo - (LastAckRecv + 1)] = true; // acknowledged means that this bit will be flipped to true

				}
				else 
				{
					// condition 3 checksum is wrong on client side so we send another packet (NAK)
					SentTime[sequenceNo - (LastAckRecv + 1)] = std::chrono::high_resolution_clock::now(); // else we will send another packet
				}
			}
		}
	}
}