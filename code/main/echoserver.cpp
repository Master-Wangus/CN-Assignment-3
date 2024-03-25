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

///*******************************************************************************
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
#include "Utils.h"
#include "packet.h"


/*!***********************************************************************
\brief
Changes the message length in network order to its string representation in hex form through binary manipulation.
\param[in, out] long
the long that is the message length to be converted to string
\return
The string representing the message length in network order
*************************************************************************/
std::string htonlToString(u_long input) {
	std::string output(sizeof(unsigned long), '\0'); // Initialize string with the size of u_long, filled with '\0'
	std::memcpy(&output[0], &input, sizeof(unsigned long)); // Copy the binary data of val into the string
	return output;
}


std::string htonsToString(short input) {
	std::string output(sizeof(short), '\0'); // Initialize string with the size of u_long, filled with '\0'
	std::memcpy(&output[0], &input, sizeof(short)); // Copy the binary data of val into the string
	return output;
}

/*!***********************************************************************
\brief
To convert the string taken as input which maybe network order and convert it to a unsigned long
for ntohl to process.
\param[in, out] string
the input string representing the length of the message in network order
\return
the long representing the system order
*************************************************************************/
u_long StringTontohl(std::string const& input) {
	u_long ret = 0;
	std::memcpy(&ret, input.data(), sizeof(u_long)); // copy binary data into the string
	return ret;
}

/*!***********************************************************************
\brief
To convert the string taken as input which maybe network order and convert it to a short
for ntohs to process.
\param[in, out] string
the input string representing the length of the message in network order
\return
the long representing the system order
*************************************************************************/
uint16_t StringTontohs(std::string const& input) {
	uint16_t ret = 0;
	std::memcpy(&ret, input.data(), sizeof(uint16_t)); // copy binary data into the string
	return ret;
}

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

/*!***********************************************************************
\brief
Converts a hex string into human readable string
\param[in, out] inputstring
the hex string to be converted
\return
the human readable string
*************************************************************************/
std::string HexToString(const std::string& inputstring) {
	std::string output{};
	for (size_t i = 0; i < inputstring.length(); i += 2) {
		std::string byteString = inputstring.substr(i, 2);
		//convert to unsigned long in hex format then cast to char
		char byte = static_cast<char>(std::stoul(byteString, nullptr, 16));
		output.push_back(byte); //append to message
	}
	return output;
}


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
SOCKET listenerSocket{};
SOCKET udpSocket{};

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
	std::string downLoadRepo{};
	std::ifstream fs("Config.txt", std::ios::binary); // open the config file

	if (fs.is_open())
	{
		std::cout << "Loading Server paramters from config file" << std::endl;
	}

	std::string parse{};
	std::getline(fs, parse);
	//downLoadRepo = parse.substr(parse.find_first_of(" ") + 1);
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

	constexpr size_t UDPBUFFER_SIZE = 1000; //arbitrary buffer size. could be 1 could be a million
	char inputUDP[UDPBUFFER_SIZE]; //set char buffer as char = uint8_t

	bool isDownloading = false;

	while (true) //loop until client disconnects
	{

		/// UDP DOWNLOAD
		if (isDownloading)
		{
			sockaddr clientAddr{}; // Client address
			SecureZeroMemory(&clientAddr, sizeof(clientAddr));
			int clientAddrSize = sizeof(clientAddr);

			const int bytesRecieved = recvfrom(udpSocket,
				inputUDP,
				UDPBUFFER_SIZE - 1,
				0,
				&clientAddr,
				&clientAddrSize);
			if (bytesRecieved == SOCKET_ERROR)
			{
				std::cerr << "recvfrom() failed." << std::endl;
				break;
			}
			else if (bytesRecieved == 0)
			{
				std::cerr << "Graceful shutdown." << std::endl;
				break;
			}
			inputUDP[bytesRecieved] = '\0';
			std::cout << inputUDP << '\n';
			std::string text(inputUDP, bytesRecieved - 1);
			Packet packet = Packet::DecodePacketNetwork(text);

			if (packet.isACK()) // Client has recieved the packet
			{
				// 1. store the ack no. in a vector or smth
				// 2. check if packet is out of order
				// 3. 
			}
			// Need to keep track of the ack to quit download state for thread
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
			uint16_t ClientUDPportNum{ Utils::StringTo_ntohs(text.substr(5, 2)) }; //get the client UDP port numba in host order bytes
			u_long fileNameLength{ Utils::StringTo_ntohl(text.substr(7, 4)) }; //get the message length in host order bytes
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
				ULONG sessionID{}; /// WESLEY TO INCORPORATE SESSION ID

				//output += htonl(sessionID);

				std::string fileLength = std::to_string(std::filesystem::file_size(filePath));
				output += fileLength;

				//// Setting up of client address
				//SecureZeroMemory(&clientAddr, sizeof(clientAddr));
				//clientAddr.sin_family = AF_INET;
				//memcpy(&clientAddr.sin_addr.S_un.S_addr, &clientIP, 4);
				//clientAddr.sin_port = htons(ClientUDPportNum);

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

			size_t NumOfFiles{}, lengthOFFileList{};
			std::vector<std::string>FileNames{};
			for (auto const& file : std::filesystem::directory_iterator{ downLoadRepo })
			{
				++NumOfFiles;
				std::string fileName = file.path().filename().string();
				FileNames.emplace_back(fileName);
				lengthOFFileList += fileName.size() + 4; // calculate the length of file list
			}

			listOfFiles += htonsToString(htons(static_cast<uint16_t>(NumOfFiles)));
			listOfFiles += htonlToString(htonl(static_cast<uint16_t>(lengthOFFileList)));

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
		for (size_t i{}; i < connectedSockets.size(); ++i) {
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