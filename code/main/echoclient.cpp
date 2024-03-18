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

uint16_t StringTontohs(std::string const& input) {
	uint16_t ret = 0;
	std::memcpy(&ret, input.data(), sizeof(uint16_t)); // copy binary data into the string
	return ret;
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

void receive(SOCKET);

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

// This program requires one extra command-line parameter: a server hostname.
int main(int argc, char** argv)
{
	std::string host{}, portString{};
	std::cout << "Server IP Address: ";
	std::cin >> host;
	std::cout << "\nServer Port Number: ";
	std::cin >> portString;
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
	errorCode = getaddrinfo(host.c_str(), portString.c_str(), &hints, &info);
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
			output += REQ_LISTUSERS;
		}
		else if (input.substr(0, 3) == "/e " && input.size() > 3)
		{
			output += REQ_ECHO;
			input = input.substr(3); // get rid of command id and preceding space
			std::istringstream iss{ input }; 
			std::string IPPortPair{}, portNum{}, message{};
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
			message = input.substr(input.find(' ') + 1); //get the message. weird edge case to match example....
			uint32_t messageSz = static_cast<uint32_t>(htonl(static_cast<u_long>(message.size())));

			sockaddr_in Ipbinary{};
			inet_pton(AF_INET, IP.c_str(), &(Ipbinary.sin_addr));
			output.append(reinterpret_cast<char*>(&Ipbinary.sin_addr.S_un.S_addr), sizeof(Ipbinary.sin_addr.S_un.S_addr));
			uint16_t port = htons(std::stoi(portNum));

			output.append(reinterpret_cast<char*>(&port), sizeof(port));
			output.append(reinterpret_cast<char*>(&messageSz), sizeof(messageSz));

			output += message;
		}
 		else if (input.substr(0, 3) == "/t " && input.size() > 3) //if user indicate to message is in raw hexidecimal form, convert to human readable message
 		{
 			input = input.substr(3); //get rid of leading /t and space
			uint8_t commandID = static_cast<uint8_t>(std::stoul(input.substr(0,2), nullptr, 16)); //get the command id
 			if (commandID == REQ_ECHO) //check if command id is not quit as raw data may just tell us to quit
 			{
				output += REQ_ECHO;
				input = input.substr(4); // get rid of command id and preceding space
				std::istringstream iss{ input };
				std::string IPPortPair{}, portNum{}, message{};
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
				message = input.substr(input.find(' ') + 1); //get the message. weird edge case to match example....
				uint32_t messageSz = htonl(static_cast<u_long>(message.size()));

				sockaddr_in Ipbinary{};
				inet_pton(AF_INET, IP.c_str(), &(Ipbinary.sin_addr));
				output.append(reinterpret_cast<char*>(&Ipbinary.sin_addr.S_un.S_addr), sizeof(Ipbinary.sin_addr.S_un.S_addr));
				uint16_t port = htons(std::stoi(portNum));

				output.append(reinterpret_cast<char*>(&port), sizeof(port));
				output.append(reinterpret_cast<char*>(&messageSz), sizeof(messageSz));

				output += message;
 			}
			else if (commandID == REQ_LISTUSERS) //if the hex code is telling us to quit
			{
				output += REQ_LISTUSERS;
			}
			else if (commandID == REQ_QUIT)
			{
				output += REQ_QUIT;
			}
			else
			{
				output += UNKNOWN; //invalid code that isnt quit or echo. Put it as the echoid + 1 which would be invalid
			}
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

			if (text[0] == REQ_ECHO || text[0] == RSP_ECHO) // request echo from server, to send back message with response echo code
			{
				std::string IP = text.substr(1, 4);
				std::string portNum = text.substr(5, 2);
				u_long msgLength = ntohl(StringTontohl(text.substr(7, 4))); //message length start. No command ID so start from the beginning
				std::string msg = text.substr(11); //message after the message length

				char str[INET_ADDRSTRLEN];
				if (inet_ntop(AF_INET, IP.c_str(), str, INET_ADDRSTRLEN)) //convert the IP to human readable string
				{
					message += std::string(str) + ':' + std::to_string(ntohs(StringTontohs(portNum))) + '\n';  //append the port number and ip to the final message
				}

				while (msg.length() < msgLength) //if the message is shorter than the indicated message length
				{
					//to receive the rest of the message
					char buffer[BUFFER_SIZE];
					int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
					if (!bytesReceived) { // error checking or to break out of loop
						break;
					}

					buffer[bytesReceived] = '\0';
					msg += std::string(buffer, bytesReceived); //append the message together
				}
				message += msg;

				if (text[0] == REQ_ECHO) 
				{
					text[0] = RSP_ECHO;
					send(clientSocket, text.c_str(), static_cast<int>(text.length()), 0);
				}
			}
			else if (text[0] == RSP_LISTUSERS) 
			{
				std::string sz = text.substr(1, 2); //get the number of IP/port pairs
				size_t numOfPairs = ntohs(StringTontohs(sz)); //convert to size_t
				text = text.substr(3); //get rid of command id and size
				message += "Users:\n";
				for (size_t i{}; i < numOfPairs; ++i)
				{
					std::string IP = text.substr(0, 4); //get the IP in binary
					text = text.substr(4); //remove the IP that was just gotten
					std::string portNum = text.substr(0, 2); //get the port number in binary
					text = text.substr(2); //remove the port number that was just gotten

					char str[INET_ADDRSTRLEN];
					if (inet_ntop(AF_INET, IP.c_str(), str, INET_ADDRSTRLEN)) //convert the IP to human readable string
					{
						message += std::string(str) + ':' + std::to_string(ntohs(StringTontohs(portNum)));  //append the port number and ip to the final message
					}
					if (i + 1 != numOfPairs) //if its not the last IP/port number pair, add a new line
					{
						message += '\n';
					}
				}
			}
			else if (text[0] == ECHO_ERROR)
			{
				message = "Echo error";
			}

			std::cout << "==========RECV START==========" << std::endl;
			std::cout << message << std::endl;
			std::cout << "==========RECV END==========" << std::endl;
		}
	}
}
