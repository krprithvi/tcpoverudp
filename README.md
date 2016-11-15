TCP functionalities over UDP 
----------------------------
This is a file transfer protocol which builds and demonstrates 
functionality of TCP. We use the UDP sockets and reliably transfer
the file from client to server by implementing functionalities of 
TCP like checksumming and rotating window.

tcpd should be a daemon (Not yet daemonized) which runs on both client and server. 
When it is running on the client, it listens on local port to receive data from ftpc, 
stores the data in the wraparound buffer and sends it to troll on the local machine 
which sends it to server's tcpd. Once an ACK has been received from the server, 
it removes the packet from its buffer.

When it is running on the server, it listens on the remote port and the local port.
When it receives a request from the ftp\_server it enqueues it and sends the data
when it receives data from the client on the remote port. Once a packet has been 
received from ftp client, it sends an ACK by sending it through troll on the local machine.

ftp\_client is a client program which sends all the bytes of the local file 
to the remote ftps server

ftps is a server program which receives a file from a client and stores it 
in a folder "recvd" locally.

Timer process takes care of maintaining the delta list of the timers of the packets in 
the window of tcpd client buffer and notifying the tcpd client when a timer expires. 

Steps to compile and clean the code
-----------------------------------
To compile the code, use the following command: make

To clean the code, invoke the following command: make clean

Steps to run the program
-----------------------------
To run the tcpd,  use the following command:
	tcpd 

To run the server, use the following command:
	ftp_server <local-port>

To run the client, use the following command:
	ftp_client <remote-IP> <remote-port> <local-file-to-transfer>

To run the timer, use the following command:
	valgrind --leak-check=yes timer
