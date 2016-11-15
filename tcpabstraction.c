#include <string.h>
#include <stdio.h>
//Socket related header files
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
// Socket details for TCPD
#define LOCALIP "127.0.0.1"
#define LOCALPORT 10809

// Define the SEND function 
ssize_t SEND(int sockfd, const void *buf, size_t len, int flags) {
    // Define local variables
    struct sockaddr_in sin_addr;
    struct hostent *hp;
    int ret;

    // Define the socket for TCPD
    hp = gethostbyname(LOCALIP);
    bcopy((void *)hp->h_addr, (void *)&sin_addr.sin_addr, hp->h_length);
    sin_addr.sin_family = AF_INET;
    sin_addr.sin_port = htons(LOCALPORT);    /* fixed by adding htons() */
    memset(sin_addr.sin_zero, '\0', sizeof(sin_addr.sin_zero));

    char *send_buf = NULL;
    send_buf = malloc(sizeof(unsigned char) * (len+1)); //allocating buffer of specific size 
    memset(send_buf, '\0', (len+1)); //Setting the buffer to null 
    send_buf[0] = '1'; //Setting the first byte to "1" signify that the message is from client
    memcpy(send_buf+1, buf, len); // Copying the message

    ret = (sendto(sockfd,send_buf,len+1,flags,(struct sockaddr *) &sin_addr, sizeof(struct sockaddr_in))); // Sending the message to TCPD
    free(send_buf); // Freeing the memory space
    
    // Acknowledgment to signal to send the next part of the message
    char ackbuf[4];
    memset(ackbuf, 0, sizeof ackbuf);
    recvfrom(sockfd, ackbuf, sizeof ackbuf, flags, (struct sockaddr*)NULL, NULL);
    // Run in a loop until receive an acknowledgment
    while(strcmp(ackbuf, "ack") != 0){
    recvfrom(sockfd, ackbuf, sizeof ackbuf, flags, (struct sockaddr*)NULL, NULL);
    }
    return ret;
}


ssize_t RECV(int sockfd, void *buf, size_t len, int flags) {
    // Define local variables
    struct sockaddr_in name;
    struct hostent *hp;
    int ret;

    // Define the socket for TCPD
    hp = gethostbyname(LOCALIP);
    bcopy((void *)hp->h_addr, (void *)&name.sin_addr, hp->h_length);
    name.sin_family = AF_INET;
    name.sin_port = htons(LOCALPORT);
    memset(name.sin_zero, '\0', sizeof(name.sin_zero));
    int addr_len = sizeof(struct sockaddr_in);

    char *send_buf = NULL;
    int size_req = len;
    size_req = htonl(size_req);
    int headersize = 5;

    send_buf = malloc(sizeof(unsigned char) * (5)); // Ask for a specific no of bytes. 4 bytes are allocated to specify the length of the message requested
    memset(send_buf, '\0', headersize);
    send_buf[0] = '2'; // Setting the first byte to '2' to signify that message is sent from server
    memcpy(send_buf+1, &size_req, 4); // Setting the buffer
    
    // Requesting a specific no of bytes
    sendto(sockfd,send_buf, 5, flags,(struct sockaddr *) &name, sizeof(struct sockaddr_in));
    // Receiving the message
    int receivedbytes = recvfrom(sockfd, buf, len, flags, (struct sockaddr *)NULL, NULL); 
    free(send_buf); // Freeing the memory space

    return receivedbytes;

}

// Since the implementation is UDP the following functions have no meaning
// To ensure compatibility with TCP we define the holder functions 
int BIND(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return (bind(sockfd, addr, addrlen)); 
}

int SOCKET(int domain, int type, int protocol) {
    return (socket(domain, SOCK_DGRAM, protocol));	
}

int CONNECT(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return 0;
}

int ACCEPT(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return sockfd;
}

int LISTEN(int sockfd, int backlog) {
    return 0;
}

