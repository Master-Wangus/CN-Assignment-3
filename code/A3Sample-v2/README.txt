###README
Unzip Test.zip into a folder e.g. C:\Test 

Unzip A3Sample.zip into a folder e.g. C:\A3Sample 

Create a folder Test under C:

In Commmand Prompt, go to C:\A3Sample

Run server.exe by typing server.exe < server-in.txt

If you open server-in.txt, you can see the following:

9000
9001
C:\Test\
4
0.1
20

Please note that the info for Line 1-5 in server-in.txt is respectively

Server TCP Port Number
Server UDP Port Number
Download path
Sliding window size
Packet loss rate
Ack timer

Run client.exe by typing client.exe < client-in-a3-1.txt in a new command prompt.

If you open client-in-a3-1.txt, you can see the following:

192.168.0.98
9000
9001
9010
.\
4
0.2
/l
/d 192.168.0.98:9010 Server.cpp
/q

Please note that the info for Line 1-5 in client-in-a3-1.txt is respectively

Server IP Address
Server TCP Port Number
Server UDP Port Number
Client UDP port number
Path to store files
Sliding window size
Packet loss rate

The last three lines in client-in-a3-1.txt are:
 
/l
/d 192.168.0.98:9010 Server.cpp
/q

which mean 
(1) command to list files
(2) command to download Server.cpp file via ip address 192.168.0.98 port number 9010
(3) command to quit

client-in-a3-2.txt and client-in-a3-3.txt are similar to client-in-a3-1.txt. 
You can open two more command prompts to run client.exe for the above concurrently.

You should configure ip address for Clients and Server according to your network setup.

Packet loss rate is configurable. 
For server, the packet lost may occur when the packet is determined to be sent.
For client, the packet lost may occur when the ACK is determined to be sent.

Sliding window size is configurable.
If it is set to 1, sliding window control works as Stop-and-Wait.
If it is set to a number larger than, sliding window control works 
as pipeline scheme of Selective Repeat.

If you want to test your Server and Client against the sample given here, 
please take note that ACK Timer is set as 10ms 
(I have not implemented adaptive timer like TCP).

