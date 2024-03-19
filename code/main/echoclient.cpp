/* Start Header
*****************************************************************/
/*!
\file echoclient.cpp
\author Koh Wei Ren, weiren.koh, 2202110
\par weiren.koh@digipen.edu
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

#include "Utils.h"			// helper file


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

void receive(SOCKET);

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


// This program requires one extra command-line parameter: a server hostname.
int main(int argc, char** argv)
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	std::string host{}, TCPServer{}, UDPServer, UDPClient, downloadPath{};
	std::cout << "Server IP Address: ";
	std::cin >> host;
	std::cout << std::endl;

	std::cout << "Server TCP Port Number: ";
	std::cin >> TCPServer;
	std::cout << std::endl;

	std::cout << "Server UDP Port Number: ";
	std::cin >> UDPServer;
	std::cout << std::endl;

	std::cout << "Client UDP Port Number: ";
	std::cin >> UDPClient;
	std::cout << std::endl;

	std::cout << "Path to store files: ";
	std::cin >> downloadPath;
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
	addrinfo hints{};
	SecureZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;			// IPv4
	hints.ai_socktype = SOCK_STREAM;	// Reliable delivery
	// Could be 0 to autodetect, but reliable delivery over IPv4 is always TCP.
	hints.ai_protocol = IPPROTO_TCP;	// TCP


	addrinfo* info = nullptr;
	errorCode = getaddrinfo(host.c_str(), TCPServer.c_str(), &hints, &info);
	if ((errorCode) || (info == nullptr))
	{
		std::cerr << "getaddrinfo() failed." << std::endl;
		WSACleanup();
		return errorCode;
	}


	// -------------------------------------------------------------------------
	// Create a socket and attempt to connect to the first resolved address.
	//
	// socket()
	// connect()
	// -------------------------------------------------------------------------

	SOCKET clientSocket = socket(
		info->ai_family,
		info->ai_socktype,
		info->ai_protocol);
	if (clientSocket == INVALID_SOCKET)
	{
		std::cerr << "socket() failed." << std::endl;
		freeaddrinfo(info);
		WSACleanup();
		return 2;
	}

	errorCode = connect(
		clientSocket,
		info->ai_addr,
		static_cast<int>(info->ai_addrlen));
	if (errorCode == SOCKET_ERROR)
	{
		std::cerr << "connect() failed." << std::endl;
		freeaddrinfo(info);
		closesocket(clientSocket);
		WSACleanup();
		return 3;
	}


	// -------------------------------------------------------------------------
	// Send some text.
	//
	// send()
	// -------------------------------------------------------------------------

	// as specified in brief for quit and echo 
	 //uint8_t QUITID = 01, ECHOID = 02;
	 std::thread receiver(receive, clientSocket);
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
			std::string IP{ IPPortPair.substr(0, IPPortPair.find(':')) }; //parse them to ip and port number
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
			uint16_t port = Utils::StringTo_ntohs(portNum);
			output.append(reinterpret_cast<char*>(&port), sizeof(port));
			// file name length
			output.append(reinterpret_cast<char*>(&messageSz), sizeof(messageSz));
			// file name
			output += filePath;
		}
		else 
		{
			output += UNKNOWN;
		}

 		// send 		
 		int bytesSent = send(clientSocket, output.c_str(), static_cast<int>(output.length()), 0); //send the message out
 		if (bytesSent == SOCKET_ERROR) //check for error
 		{
			int errorCode = WSAGetLastError();
 			std::cerr << "send() failed with error code: " << errorCode << std::endl;
 		}
		
		if (output[0] == REQ_QUIT || output[0] == UNKNOWN) break;
	 }

	 receiver.join();
	 errorCode = shutdown(clientSocket, SD_SEND); //shutdown by telling it to close off from reading and writing
	 if (errorCode == SOCKET_ERROR) //error checking
	 {
		 std::cerr << "shutdown() failed." << std::endl;
	 }
	 closesocket(clientSocket); //close socket fr
	 CoUninitialize();
	WSACleanup(); //goodnight 
}

void receive(SOCKET clientSocket) {
	while (true) 
	{
		// receiving text
		constexpr size_t BUFFER_SIZE = 1000;
		char buffer[BUFFER_SIZE]{};
		const int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0); //receive echo'ed text and header information
		std::string message{};
		if (bytesReceived == SOCKET_ERROR) //check for error
		{
			std::cerr << "recv() failed." << std::endl;
			break;
		}
		else if (bytesReceived == 0) //check if not receiving any messages
		{
			break;
		}
		else //receiving messages from server. to process
		{
			buffer[bytesReceived] = '\0';
			std::string text(buffer, bytesReceived);

			if (text[0] == RSP_DOWNLOAD) // request echo from server, to send back message with response echo code
			{
				std::string IP = text.substr(1, 4);
				std::string portNum = text.substr(5, 2);
				u_long sessionID = Utils::StringTo_ntohl(text.substr(7, 4)); // session id
				std::string msg = text.substr(11); // file length? brief never specify btyes


				/// INSERT UDP HERE
				//char str[INET_ADDRSTRLEN];
				//if (inet_ntop(AF_INET, IP.c_str(), str, INET_ADDRSTRLEN)) //convert the IP to human readable string
				//{
				//	message += std::string(str) + ':' + std::to_string(Utils::StringTo_ntohs(portNum)) + '\n';  //append the port number and ip to the final message
				//}
				//while (msg.length() < msgLength) //if the message is shorter than the indicated message length
				//{
				//	//to receive the rest of the message
				//	char buffer[BUFFER_SIZE];
				//	int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
				//	if (!bytesReceived) { // error checking or to break out of loop
				//		break;
				//	}

				//	buffer[bytesReceived] = '\0';
				//	msg += std::string(buffer, bytesReceived); //append the message together
				//}
				//message += msg;

				//if (text[0] == REQ_DOWNLOAD) 
				//{
				//	text[0] = REQ_DOWNLOAD;
				//	send(clientSocket, text.c_str(), static_cast<int>(text.length()), 0);
				//}
			}
			else if (text[0] == RSP_LISTFILES) 
			{
				u_short fileCount = Utils::StringTo_ntohs(text.substr(1, 2)); // number of files
				u_long listLength = Utils::StringTo_ntohl(text.substr(3, 4)); // total number of bytes for length+name

				message += "List of Files:\n";
				for (size_t i{}, offset{}; i < fileCount; ++i)
				{
					u_long fileNameLength = Utils::StringTo_ntohl(text.substr(i + 7 + offset, 4)); // offset by first 7 bytes
					std::string fileName = text.substr(i + 11 + offset, fileNameLength); // offset by first 7 bytes + 4 bytes (fileNameLength)
					message += fileName + '\n';
					offset = static_cast<size_t>(fileNameLength) + 4;
				}
			}
			else if (text[0] == DOWNLOAD_ERROR)
			{
				message = "Download error";
			}

			std::cout << "==========RECV START==========" << std::endl;
			std::cout << message << std::endl;
			std::cout << "==========RECV END==========" << std::endl;
		}
	}
}
