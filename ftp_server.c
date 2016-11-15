/* Server for accepting a file from client */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
// For boolean
#include <stdbool.h>
// Related to creation of directory
#include <sys/stat.h>
// For errors
#include <errno.h>
// For isdigit
#include <ctype.h>
// Socket related header files
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
// Header file with TCP abstraction implemented in UDP in backend
#include "tcpabstraction.h"

// Defining constants
enum { BUFFERSIZE = 1000, AL_FILESIZE = 4, AL_FILENAME = 20 };
// Directory for storing the received files
static const char DESTDIR[] = "recvd";

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

// Read n bytes from the stream. Keep waiting until then. Blocking.
void readnbytes(int sockd, char *buffer, int n, bool writetofile,
		char *filename)
{
    int total_bytes_read = 0;
    int bytes_read = 0;
    FILE *fp;
    int errnum;
    // Create a file if writetofile boolean is set
    if (writetofile) {
	fp = fopen(filename, "wb");
	// Check if file opened correctly
	if (fp == NULL) {
	    errnum = errno;
	    fprintf(stderr, "Error opening file: %s\n", strerror(errnum));
	    exit(1);
	}
    }
    // Keep asking for bytes till the n bytes are read. This is a blocking call
    while (total_bytes_read < n) {
	if ((bytes_read =
	     RECV(sockd, buffer,
		  n - total_bytes_read <
		  BUFFERSIZE ? n - total_bytes_read : BUFFERSIZE,
		  0)) < 0) {
	    printf("bytes read: %d", bytes_read);
	    perror("Error reading on the stream socket\n");
	    exit(1);
	}
	printf("Bytes read : %d\n", bytes_read);
	total_bytes_read += bytes_read;
    printf("Total bytes read : %d This time : %d\n", total_bytes_read, bytes_read);
	// Write to a file if writetofile boolean is set
	if (writetofile)
	    fwrite(buffer, sizeof(unsigned char), bytes_read, fp);
    }
    // Close file pointer if writetofile boolean is set
    if (writetofile)
	fclose(fp);

}

/* server program called with no argument */
int main(int argc, char *argv[])
{
    int errnum;
    // Check for the required arguments
    // ftps <local-port>
    if (argc != 2) {
        printf("Sorry ! Format is incorrect. Please check the syntax. \n");
        printf("ftps <local-port>\n");
        printf("Try again. Thank you\n");
        exit(0);
    }
    // Check if the port number is valid
    int port;
    if (!is_port_number(argv[1], &port))
        exit(0);

    // Check if directory exists otherwise create
    if (mkdir(DESTDIR, S_IRWXU | S_IRWXG | S_IRWXO) == 0)
        printf("Creating directory '%s'\n", DESTDIR);
    else if (errno != EEXIST) {
        errnum = errno;
        fprintf(stderr, "Error creating directory : %s\n",
            strerror(errnum));
        exit(1);
    }
    // Socket descriptors of server and client  
    int sock;			/* initial socket descriptor */
    int msgsock;		/* accepted socket descriptor,
				 * each client connection has a
				 * unique socket descriptor*/

    struct sockaddr_in sin_addr;	/* structure for socket name setup */
    char buffer[BUFFERSIZE];	/* buffer for holding read data */


    /*initialize socket connection in unix domain */
    if ((sock = SOCKET(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("error openting datagram socket");
        exit(1);
    }
    // Enable the reuse of the address binded to the program
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) <
	0)
        perror("setsockopt(SO_REUSEADDR) failed");

    /* construct name of socket to send to */
    sin_addr.sin_family = AF_INET;
    sin_addr.sin_addr.s_addr = INADDR_ANY;
    sin_addr.sin_port = htons(port);

    /* bind socket name to socket */
    if (BIND
	(sock, (struct sockaddr *) &sin_addr,
	 sizeof(struct sockaddr_in)) < 0) {
        perror("error binding stream socket");
        exit(1);
    }

    /* listen for socket connection and set max opened socket connetions to 1 */
    LISTEN(sock, 1);

    printf("File server waiting for remote connection from client.\n");
    /* accept a (1) connection in socket msgsocket */
    if ((msgsock =
	 ACCEPT(sock, (struct sockaddr *) NULL,
		(socklen_t *) NULL)) == -1) {
        perror("error connecting stream socket");
        exit(1);
    }

    /* put all zeros in buffer (clear) */
    bzero(buffer, BUFFERSIZE);

    // Format in which to read the file - 
    // First 4 Bytes - Length of file
    // Next 20 bytes - Name of file
    // Read the file

    // Read the size of the file. First 4 bytes
    uint32_t filesize = 0;
    readnbytes(msgsock, buffer, AL_FILESIZE, false, NULL);
    // Copy from the buffer to the int variable
    memcpy(&filesize, buffer, sizeof(uint32_t));
    // Changing from network order to host order
    filesize = ntohl(filesize);
    printf("Size of the file : %d \n", filesize);

    ///* put all zeros in buffer (clear) */
    bzero(buffer, BUFFERSIZE);

    // Read the name of the file. Next 20 bytes
    char filename[27];
    // To get formatted output into a buffer
    char formatted_filename[27];
    readnbytes(msgsock, buffer, AL_FILENAME, false, NULL);
    printf("Name of the file : %s \n", buffer);
    strcpy(filename, buffer);
    snprintf(formatted_filename, sizeof(formatted_filename), "%s/%s",
	     DESTDIR, filename);

    /* put all zeros in buffer (clear) */
    bzero(buffer, BUFFERSIZE);

    // Get the file and write it locally
    readnbytes(msgsock, buffer, filesize, true, formatted_filename);

    /* close all connections and remove socket file */
    close(msgsock);
    close(sock);

    return 0;
}
