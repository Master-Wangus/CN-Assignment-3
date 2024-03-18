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
 
 bool execute(SOCKET clientSocket);
void disconnect(SOCKET& listenerSocket);

// Tell the Visual Studio linker to include the following library in linking.
// Alternatively, we could add this file to the linker command-line parameters,
// but including it in the source code simplifies the configuration.
#pragma comment(lib, "ws2_32.lib")

#include <iostream>			// cout, cerr
#include <string>			// string
#include <vector>


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
	UNKNOWN = (unsigned char)0x0,
	REQ_QUIT = (unsigned char)0x1,
	REQ_ECHO = (unsigned char)0x2,
	RSP_ECHO = (unsigned char)0x3,
	REQ_LISTUSERS = (unsigned char)0x4,
	RSP_LISTUSERS = (unsigned char)0x5,
	CMD_TEST = (unsigned char)0x20,
	ECHO_ERROR = (unsigned char)0x30
};

std::vector<std::pair<sockaddr_in, SOCKET>> connectedSockets{};

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
	uint16_t portNumber{};
	std::cout << "Server Port Number: ";
	if (!(std::cin >> portNumber))
	{
		WSACleanup();
		return 0;
	}
	std::string portString{ std::to_string(portNumber) };

	


	// -------------------------------------------------------------------------
	// Resolve own host name into IP addresses (in a singly-linked list).
	//
	// getaddrinfo()
	// -------------------------------------------------------------------------

	// Object hints indicates which protocols to use to fill in the info.
	addrinfo hints{};
	SecureZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;			// IPv4
	// For UDP use SOCK_DGRAM instead of SOCK_STREAM.
	hints.ai_socktype = SOCK_STREAM;	// Reliable delivery
	// Could be 0 for autodetect, but reliable delivery over IPv4 is always TCP.
	hints.ai_protocol = IPPROTO_TCP;	// TCP
	// Create a passive socket that is suitable for bind() and listen().
	hints.ai_flags = AI_PASSIVE;

	char ip[INET_ADDRSTRLEN]{};
	gethostname(ip, INET_ADDRSTRLEN);
	addrinfo* info = nullptr;
	errorCode = getaddrinfo(ip, portString.c_str(), &hints, &info);
	if ((errorCode) || (info == nullptr))
	{
		std::cerr << "getaddrinfo() failed." << std::endl;
		WSACleanup();
		return errorCode;
	}
	
	inet_ntop(AF_INET, &(reinterpret_cast<sockaddr_in *>(info->ai_addr))->sin_addr, ip, INET_ADDRSTRLEN);

	// -------------------------------------------------------------------------
	// Create a socket and bind it to own network interface controller.
	//
	// socket()
	// bind()
	// -------------------------------------------------------------------------

	SOCKET listenerSocket = socket(
		hints.ai_family,
		hints.ai_socktype,
		hints.ai_protocol);
	if (listenerSocket == INVALID_SOCKET)
	{
		std::cerr << "socket() failed." << std::endl;
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	errorCode = bind(
		listenerSocket,
		info->ai_addr,
		static_cast<int>(info->ai_addrlen));
	if (errorCode != NO_ERROR)
	{
		//std::cerr << "bind() failed." << std::endl;
		closesocket(listenerSocket);
		listenerSocket = INVALID_SOCKET;
	}

	freeaddrinfo(info);

	if (listenerSocket == INVALID_SOCKET)
	{
		//std::cerr << "bind() failed." << std::endl;
		WSACleanup();
		return 2;
	}

	std::cout << "\nServer IP Address: " << ip << std::endl;
	std::cout << "Server Port Number: " << portString << std::endl;

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

	constexpr size_t BUFFER_SIZE = 1000; //arbitrary buffer size. could be 1 could be a million
	char input[BUFFER_SIZE]; //set char buffer as char = uint8_t
	while (true) //loop until client disconnects
	{
		const int bytesReceived = recv(clientSocket, input, BUFFER_SIZE - 1, 0);
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

		input[bytesReceived] = '\0';
		std::string text(input, bytesReceived);
		if (text[0] == REQ_QUIT) //check 1st byte == quit
		{
			break;
		}
		else if (text[0] == REQ_ECHO || text[0] == RSP_ECHO) //check 1st byte  == echo
		{
			std::string destinationIP{ text.substr(1, 4) }; //get the ip
			uint16_t portNum{ ntohs(StringTontohs(text.substr(5, 2))) }; //get the port numba in host order bytes
			u_long messageLength{ ntohl(StringTontohl(text.substr(7, 4))) }; //get the message length in host order bytes
			std::string message{ text.substr(11) }; //get the message

			std::string output{};

			std::pair<long, uint16_t> addr = std::make_pair(*reinterpret_cast<ULONG*>(destinationIP.data()), (portNum));
			SOCKET destinationSocket{};

			if ((destinationSocket = SearchConnectedSockets(connectedSockets, addr)) == -1) {
				output += ECHO_ERROR;
				send(clientSocket, output.c_str(), static_cast<int>(output.size()), 0);
				continue;
			}

			fd_set readSet; //to check for activity reference here /https://inst.eecs.berkeley.edu/~ee122/sp05/sockets-select_function.pdf
			FD_ZERO(&readSet);
			FD_SET(clientSocket, &readSet);

			// timeout to check if anymore activity at client socket. If not, terminate
			timeval timeout;
			timeout.tv_sec = 2;  // 2 sec delay. Give the client some time to send in case of lag
			timeout.tv_usec = 0;

			// Loop to receive the rest of the message
			while (message.length() < messageLength){
				int activity = select(static_cast<int>(clientSocket/* + 1 */), &readSet, nullptr, nullptr, &timeout); //checking for activity by selecting client socket
				if (activity < 0 || !activity){ //error or no activity detected
					break;
				}

				//to receive the rest of the message
				char buffer[BUFFER_SIZE];
				int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
				if (!bytesReceived){ // error checking or to break out of loop
					break;
				}

				buffer[bytesReceived] = '\0';
				message += std::string(buffer, bytesReceived); //append the message together
			}

			if (messageLength != message.length()){ //message length and actual message length don't match
				std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
				std::cout << "Error invalid message length" << std::endl;
				break;
			}

			sockaddr_in clientAddr{};
			socklen_t clientAddrLen = sizeof(clientAddr);
			getpeername(clientSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
			memcpy(text.data() + 1, &clientAddr.sin_addr.S_un.S_addr, sizeof(clientAddr.sin_addr.S_un.S_addr));
			short nStr = clientAddr.sin_port;
			memcpy(text.data() + 5, &nStr, 2);

			if (text[0] == REQ_ECHO) {
				std::lock_guard<std::mutex> usersLock{ _stdoutMutex };
				std::cout << "==========RECV START==========" << std::endl;
				char ipStr[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, destinationIP.c_str(), ipStr, INET_ADDRSTRLEN);
				std::cout << ipStr << ':' << portNum << std::endl;
				std::cout << message << std::endl;
				std::cout << "==========RECV END==========" << std::endl;
			}

			const int bytesSent = send(destinationSocket, text.c_str(), static_cast<int>(text.length()), 0); //send the message back to client

			if (bytesSent == SOCKET_ERROR){
				std::cerr << "send() failed." << std::endl;
				break;
			}
		}
		else if(text[0] == REQ_LISTUSERS)
		{
			std::string listOfUsers{};
			listOfUsers = RSP_LISTUSERS;
			listOfUsers += htonsToString(htons(static_cast<uint16_t>(connectedSockets.size())));
			for (auto const& pair : connectedSockets) 
			{	
				sockaddr_in addr = pair.first;
				listOfUsers.append(reinterpret_cast<char*>(&addr.sin_addr.S_un.S_addr), sizeof(addr.sin_addr.S_un.S_addr));
				listOfUsers.append(reinterpret_cast<char*>(&addr.sin_port), sizeof(addr.sin_port));
			}
			send(clientSocket, listOfUsers.c_str(), static_cast<int>(listOfUsers.size()), 0);
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