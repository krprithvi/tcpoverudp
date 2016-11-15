/* ftpc.c to transfer a file using TCP stream sockets */
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
//Socket related header files
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// For boolean
#include <stdbool.h>
// For errors
#include <errno.h>
// For isdigit
#include <ctype.h>
// Header file with TCP abstraction implemented in UDP in backend
#include "tcpabstraction.h"

#define BUFFER_SIZE 1000
#define CLIENTPORT 54321

// Check if the port number is valid
bool is_port_number(char number[], int *port)
{
    int i = 0;

    // Checking for negative numbers
    if (number[0] == '-') {
        printf("Please choose a port number in the range 1024 - 65535\n");
        return false;
    }
    // Check if it is a valid number
    for (; number[i] != 0; i++) {
	if (!isdigit(number[i])) {
	    printf
		("Incorrect format for port number. Please choose a number in the range 1024 - 65535\n");
	    return false;
	}
    }
    // Check if it is in valid range
    *port = atoi(number);
    if (*port < 1024 || *port > 65535) {
        printf("Please choose a port number in the range 1024 - 65535\n");
        return false;
    }
    return true;
}

/* ftp client program called with <remote-IP> <remote-port> <local-file-to-transfer> to transfer a file to the server */
int main(int argc, char *argv[])
{
    int sock;			/* initial socket descriptor */
    struct sockaddr_in sin_addr;	/* structure for socket name setup */
    struct sockaddr_in sin_addr_client;	/* structure for socket name setup */
    char *buffer = NULL;
    char *file_name;
    struct hostent *hp;
    FILE *fp;
    int total_bytes, size, no_bytes, test;

    if (argc != 4) {
        printf("\n number of arguments are incorrect \n");
        printf
            ("\n Please start ftpc in this way:\n \t ftpc <remote-IP> <remote-port> <local-file-to-transfer> \n");
        return -1;
    }

    /* initialize socket connection in unix domain */
    if ((sock = SOCKET(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("error opening socket");
        exit(1);
    }
    // Getting host info. & checking if the IP is valid
    hp = gethostbyname(argv[1]);
    if (hp == NULL) {
        fprintf(stderr, "%s: Invalid IP/host name \n", argv[1]);
        exit(2);
    }
    // Check if the port number is valid
    int port;
    if (!is_port_number(argv[2], &port))
        exit(0);

    /* Define client socket */
    sin_addr_client.sin_family = AF_INET;
    sin_addr_client.sin_addr.s_addr = INADDR_ANY;
    sin_addr_client.sin_port = htons(CLIENTPORT);

    /* bind socket name to socket */
    if (BIND
	(sock, (struct sockaddr *) &sin_addr_client,
	 sizeof(struct sockaddr_in)) < 0) {
        perror("error binding stream socket");
        exit(1);
    }


    /* construct name of socket to send to */
    bcopy((void *) hp->h_addr, (void *) &sin_addr.sin_addr, hp->h_length);
    sin_addr.sin_family = AF_INET;
    sin_addr.sin_port = htons(port);	/* fixed by adding htons() */
    memset(sin_addr.sin_zero, '\0', sizeof(sin_addr.sin_zero));

    /* establish connection with server */
    if (CONNECT(sock, (struct sockaddr *) &sin_addr,
		sizeof(struct sockaddr_in)) < 0) {
        close(sock);
        perror("error connecting stream socket");
        exit(1);
    }

    file_name = argv[3];
    fp = fopen(file_name, "r");	// opening input-file specified by argv[3]
    if (fp == NULL) {
	printf
	    ("\n input-filename %s doesn't exist. Please enter a valid filename\n",
	     argv[1]);
	printf
	    ("\n Please invoke the program in this way:\n \t count <input-filename> <search-string> <output-filename> \n");
	return -1;
    }
    // Finding the size of the file
    fseek(fp, 0, SEEK_END);	// setting fp to the end of file
    total_bytes = (ftell(fp));
    printf("Total File Size : %d", total_bytes);
    size = htonl(total_bytes);
    fseek(fp, 0, SEEK_SET);	// setting fp to the start of file

    // Allocating the buffer
    buffer = malloc(sizeof(unsigned char) * (BUFFER_SIZE));	//allocating buffer of size 1000 bytes to read the file

    printf("Starting the transfer of the file %s\n", file_name);

    do {
        memset(buffer, '\0', BUFFER_SIZE);
        if (ftell(fp) != 0)	// For all iterations of the while-loop except the first one
        {
            no_bytes = fread(buffer, 1, (BUFFER_SIZE), fp);	// copy 1000 bytes from the position pointed by fp to the buffer
            printf("Reading the next no of btyes : %d %d\n", no_bytes, total_bytes);
            total_bytes -= no_bytes;
        } else			// For the 1st iteration of the loop
        {
            memcpy(buffer, &size, 4);
            memcpy(buffer + 4, file_name, strlen(file_name));
            // copy 1000-24 bytes from the position pointed by fp to buffer 
            test = (fread(buffer + 24, 1, (BUFFER_SIZE - 24), fp));
            no_bytes = test + 24; 
            printf("Reading the first time :%d  %d %d\n", test, no_bytes, total_bytes);
            total_bytes -= test;
        }
        // write buffer to sock
        if ((SEND(sock, buffer, no_bytes, 0)) < 0) {
            perror("error writing on stream socket");
            exit(1);
        }
        //usleep(1000);
        //sleep(1);
    } while (total_bytes > 0);
    printf("Transfer complete \n");

    free(buffer);
    fclose(fp);
    close(sock);
    return 0;
}
