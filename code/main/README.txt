########################################READ ME#############################################

Unzip Folder with executable

Edit the ServerConfig.txt file to select download repository for the server and input paramters.
Consequently, you can also leave it blank. Server will prompt users it there is no/invalid
config file.

Edit the ClientConfig.txt file to edit the input parameters for the client.

########################################INPUT PARAMTERS#############################################
Input paramter for server: 
DownLoad repository: Download (Default)

If download repository is invalid, user will be prompted to enter a valid download repository.

Input paramters for both client and server: 
a) Sliding Window size	(Range: 10 - 100)
   Size of the sliding window for (Flow control mechanism here).
b) Packet loss rate	(Range: 0.0 - 1.0)
   Determines the simulated packet loss
   - For server, the packet lost may occur when the packet is determined to be sent.
   - For client, the packet lost may occur when the ACK is determined to be sent.

c) Ack timer		(Range: 10ms - 500ms)
   Amount of time before a timeout is triggered.

########################################CLIENT COMMANDS#############################################
Commands for client:

1) /l - list all the files in the download repository
2) /d - download command to be sent to server. Format to follow is
	/d "CLIENT IP ADDRESS":"CLIENT UDP PORT NUMBER" "FILENAME"
	an example is:
	/d 192.168.0.98:9010 Server.cpp
3) /q - Quit the client, disconnecting it from the server

