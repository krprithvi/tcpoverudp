/*tcpd which runs on both ftp client & server machines */
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<strings.h>
#include<string.h>
#include<sys/select.h>
#include<stdbool.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<netdb.h>
#include<sys/time.h>
// Defining the maximum segment size
#define MSS 1000
// Setting 2 bytes for checksum calculation
#define CHECKSUMSIZE 2
// Setting local port
// Ensure any change in this port number is reflected in tcpabstraction.c
#define LOCALPORT 10809
// Remote port
#define REMOTEPORT 10810
// Port to run troll on. To simulate a real network. Troll can simulate packet drops, reorder, introduce delay and garbling
#define TROLLPORT 12345
#define FTPCPORT 54321
// Port on which delta timer is running
#define TIMERPORT 1031
// Size of sequence number field
#define SEQNOSIZE 4
// Size of message length field
#define MSGLEN 4
// Server port
#define SERVERPORT 1040
// Server IP 
#define SERVERIP "164.107.113.20"
#define CLIENTIP "164.107.113.18"

// Delta Timer related variables
int timesock;
struct sockaddr_in name;

typedef struct timedout{
    char action;
    int sequence_number;
}timedout;

typedef struct timermessage{
    int action;
    int sequence_number;
    float time;
}timermessage;

void canceltimer(int sequence_number){
    timermessage msg;

    msg.action = 2;
    msg.sequence_number = sequence_number;
    msg.time = 0;
    if(sendto(timesock, (char*)&msg, sizeof msg, 0, (struct sockaddr *)&name, sizeof(name)) <0) {
        perror("sending datagram message");
        exit(4);
    }
}
void settimer(float time, int sequence_number){
    timermessage msg;

    msg.action = 1;
    msg.sequence_number = sequence_number;
    msg.time = time;
    if(sendto(timesock, (char*)&msg, sizeof msg, 0, (struct sockaddr *)&name, sizeof(name)) <0) {
        perror("sending datagram message");
        exit(4);
    }
}

// Function to compute checksum 
bool checksum_calc(unsigned char* buf, unsigned char* checksum, int len, int b)
{
    uint16_t poly,reg;
    poly = 32773 ;
    unsigned char *p = buf;

    memcpy(&reg,buf,2);
    reg = ntohs(reg);
    unsigned char tmp;
    unsigned char* ptr;
    ptr = buf+2;
    int a =b; 
    bool flag = false;
    int bits_read, bytes_remaining = len-2;

    while (bytes_remaining > 0)
    {

        bits_read = 0;
        tmp = *ptr;
        while(bits_read < 8)
        {
            flag = (reg & 0x8000);
            reg = (((reg << 1) & 0xfffe) | (tmp >> 7));
            if(flag) reg = reg ^ poly;
            tmp = (tmp << 1);
            bits_read++;
        }
        ptr++;
        bytes_remaining-- ;
    }
    if ( a == 1)
    {
        reg = htons(reg);
        memcpy(checksum,(unsigned char*)&reg,2);
    }
    else if (a == 2)
    {
        if (reg != 0){
            return false;
        }
        else{ 
            return true;
        }
    }
}

// Get current time
unsigned long gettime(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long timeu = 1000000 * tv.tv_sec + tv.tv_usec;
    return timeu;
}

// Initializing RTT,RTO & related variables
    int RTT = 0, RTO = 1;
    int SRTT, RTTVAR;
    float alpha = 1/8.0, beta = 1/4.0;

// Auxilary Data Structure definition
typedef struct ads{
    int sequence_number;
    int length;
    unsigned long timesent;
    bool ack;
    struct ads *next;
}ads;

/* Function to print the auiliary data structure */
void print_LL(ads *head);

/* Function to delete a node from the auxiliary Data Structure */
void auxilarydsdelete(ads **head, ads **tail, int seq_no, int *windowsize, int *start){
    if(*head == NULL) {
    printf("Auxiliary LL is empty\n");
    return;
    }
    ads *temp = NULL, *previous = NULL;
    if((*head)->sequence_number == seq_no){
        temp = *head;
        (*head) = (*head)->next;
        *windowsize -= 1;
        *start += temp->length;
        free(temp);
        while((*head) != NULL && (*head)->ack == true){
            temp = *head;
            (*head) = (*head)->next;
            *windowsize -= 1;
            *start += temp->length;
            free(temp);
        }
        return;
    }
    else{
        temp = *head;
        while(temp != NULL && temp->sequence_number != seq_no){
            previous = temp;
            temp = temp->next;
        }

        if(temp == NULL){
            printf("Sequence no not found \n");
            return;
        }
        else{
            temp->ack = true;
        }
    }
}
// Search for an element in the auxiliary data structure
int findll(ads *head, int sequence_number){
    if(head == NULL){
        printf("Error because head is null\n");
        print_LL(head);
        return -1;
    }
    ads *temp = head;
    while(temp != NULL && temp->sequence_number != sequence_number)
        temp = temp->next;
    if(temp == NULL){
        printf("Error because could not find sequence number\n");
        print_LL(head);
        return -1;
    }
    temp->timesent = gettime();
    return temp->length;
}
// Search for an element in auxiliary data structure in server side
bool findlls(ads *head, int sequence_number){
    if(head == NULL){
        return true;
    }
    ads *temp = head;
    while(temp != NULL && temp->sequence_number != sequence_number)
        temp = temp->next;
    if(temp == NULL){
        return true;
    }
    return false;
}

/* Function to add/modify a node to/in the auxiliary Data Structure */
void auxilarydscontrol(ads **head, ads **tail, int middle, int length){
    if(*head == NULL){
        (*head) = (ads*)malloc(sizeof(ads));
        (*head)->sequence_number = middle;
        (*head)->length = length;
        (*head)->ack = false;
        (*head)->timesent = gettime();
        (*head)->next = NULL;
        (*tail) = (*head);
    }
    else{
        ads *temp = *head, *previous = NULL;
        while(temp != NULL && temp->sequence_number < middle){
            previous = temp;
            temp = temp->next; 
        }

        ads *newnode;
        newnode = (ads*)malloc(sizeof(ads));
        newnode->sequence_number = middle;
        newnode->length = length;
        newnode->ack = false;
        newnode->timesent = gettime();

        if(previous == NULL){
            newnode->next = (*head);
            (*head) = newnode;
        }
        else{
            newnode->next = previous->next;
            previous->next = newnode;
        }
    }
}

/* Function to print the auxiliary Data Structure */
void print_LL(ads *head) {

    if(head == NULL) {
    printf("Auxiliary LL is empty\n");
    return;
    }

    printf("Aux : ");
    while((head) !=NULL )
    {
        printf("%d -> ",head->sequence_number);
        head=head->next;
    }
    printf("NULL\n");
}

/* Function to check if there are bytes that can be sent to the ftp server */
int checkifcanbesent(ads **head, int start, int req_bytes){
    ads *temp = NULL;
    int canbesent = 0;
    if((*head) != NULL && (*head)->sequence_number == start){
        if(req_bytes >= (*head)->length){
            temp = *head;
            (*head) = (*head)->next;
            canbesent = temp->length;
            free(temp);
            return canbesent;
        }
        else{
            (*head)->sequence_number += req_bytes;
            (*head)->length -= req_bytes;
            return req_bytes;
        }
    }
    return 0;
}

/* Function to find the RTT of a packet and its acknowledgment */
int findRTT(ads *head,int seqno) {
    if(head == NULL) {
        printf("Auxiliary LL is empty\n");
        return 0;
    }
    while((head) !=NULL )
    {
        if(head->sequence_number == seqno)
                return(gettime()-(head->timesent));
        head=head->next;
    }
    return 0;
}

bool firstime = true;
/* Function to update RTO */
void calculateRTTandRTO(int seqno,ads* head){
    // Calculate RTT
    int temp;
    temp = findRTT(head,seqno);
    if(temp > 0){
    RTT = temp;
    // If it is the first packet
    if(firstime){
        RTO = 1;
        SRTT = RTT;
        RTTVAR = RTT/2;
        RTO = SRTT + 4*RTTVAR;
        firstime = false;
    }
    // For rest of the transmissions
    else{
        RTTVAR = (1-beta)*RTTVAR + beta*abs(SRTT - RTT);
        SRTT = (1-alpha)*SRTT + alpha*RTT;
        RTO = SRTT + 4*RTTVAR;
    }
    // Printing RTT / SRTT / RTTVAR / Retransmission Time out values
    printf("-------------------------------------\n");
    printf("RTT: %d, SRTT: %d, RTTVAR: %d\n", RTT, SRTT, RTTVAR);
    printf("RTO after computation: %d\n", RTO);
    printf("-------------------------------------\n");
    }
}


int main(int argc, char *argv)
{
    // TCP Header structure with troll header
    struct Message
    {
        struct sockaddr_in msg_header;
        unsigned char body[MSS];
        int msg_length;
        int sequence_number;
        bool fromclient;
        unsigned char checksum[CHECKSUMSIZE];
    } MyMessage;

    // Initialzie variables
    struct sockaddr_in request_address;
    struct hostent *hp, *server;
    // Server details
    hp = gethostbyname ("127.0.0.1");
    inet_aton("127.0.0.1", &(request_address.sin_addr));
    request_address.sin_family = AF_INET;
    request_address.sin_port = htons(SERVERPORT);
    int request_addr_length;

    // Creating remote socket
    struct sockaddr_in tcpd_remote;

    tcpd_remote.sin_family = AF_INET;
    tcpd_remote.sin_addr.s_addr = INADDR_ANY;
    tcpd_remote.sin_port = htons (REMOTEPORT);

    int remote_sockd;
    remote_sockd = socket (AF_INET, SOCK_DGRAM, 0);
    bind (remote_sockd, (struct sockaddr *) &tcpd_remote,
    sizeof (struct sockaddr_in));

    // Creating local socket
    struct sockaddr_in tcpd_local;

    tcpd_local.sin_family = AF_INET;
    tcpd_local.sin_addr.s_addr = INADDR_ANY;
    tcpd_local.sin_port = htons (LOCALPORT);

    int local_sockd;
    local_sockd = socket (AF_INET, SOCK_DGRAM, 0);
    bind (local_sockd, (struct sockaddr *) &tcpd_local,
    sizeof (struct sockaddr_in));

    // Filling in details of machine on which troll is running
    struct sockaddr_in trolladdr;
    bzero ((char *) &trolladdr, sizeof trolladdr);
    bcopy ((void *) hp->h_addr, (void *) &trolladdr.sin_addr, hp->h_length);
    trolladdr.sin_family = AF_INET;
    trolladdr.sin_port = htons (TROLLPORT);

    // Preparing the troll header
    MyMessage.msg_header.sin_family = htons (AF_INET);
    // Update Aux DS
    inet_aton(SERVERIP, &(MyMessage.msg_header.sin_addr));
    MyMessage.msg_header.sin_port = htons (REMOTEPORT);

    // Filling in details of ftpc to send Ack
    struct sockaddr_in ftpcaddr;
    bzero ((char *) &ftpcaddr, sizeof ftpcaddr);
    bcopy ((void *) hp->h_addr, (void *) &ftpcaddr.sin_addr, hp->h_length);
    ftpcaddr.sin_family = AF_INET;
    ftpcaddr.sin_port = htons (FTPCPORT);
    
    // Time header
    /* create socket for connecting to server */
    timesock = socket(AF_INET, SOCK_DGRAM,0);
    if(timesock < 0) {
	perror("opening datagram socket");
	exit(2);
    }

    /* construct name for connecting to server */
    name.sin_family = AF_INET;
    name.sin_port = htons(TIMERPORT);

    /* convert hostname to IP address and enter into name */
    hp = gethostbyname("127.0.0.1");
    if(hp == 0) {
	exit(3);
    }
    bcopy((char *)hp->h_addr, (char *)&name.sin_addr, hp->h_length);


    // Initialize variables
    unsigned char localbuffer_receive[1024];
    unsigned char localbuffer_send[1024];
    unsigned char remotebuffer[1024];
    unsigned char clientbuffer[64000];
    unsigned char serverbuffer[64000];
    memset (clientbuffer, 0, sizeof(clientbuffer));
    memset (serverbuffer, 0, sizeof(serverbuffer));
    // Circular buffer variables
    int start=0, middle=0, end=0;
    int start_s=0;
    bool isfull = false;
    bool isfull_s = false;
    int wraparoundend;
    char tempbuf[1024];
    // Aux DS variables
    ads *head=NULL, *tail=NULL;
    ads *head_s=NULL, *tail_s=NULL;
    int windowsize = 0;
    int windowsize_s = 0;
    // Logic variables
    bool datapresent = false;
    uint32_t requestedbytes = 0;
    int receivedbytes = 0, bytesleft;
    char *cp;

    // 'select' related variables
    fd_set rfds;
    FD_ZERO (&rfds);

    // Set ftpc ack variables
    bool is_full = false;
    int overflowbufsize;
    unsigned char ftpc_ack[4];
    unsigned char overflowbuf[1024];
    memset (overflowbuf, 0, sizeof(overflowbuf));
    memcpy (ftpc_ack,"ack" , sizeof(ftpc_ack));

    FD_SET (local_sockd, &rfds);
    FD_SET (remote_sockd, &rfds);
    while (1)
    {
        printf ("Start \n");
        if (select(local_sockd > remote_sockd ? local_sockd + 1 : remote_sockd + 1,
            &rfds, NULL, NULL, NULL) < 0)
        {
            perror ("Select");
            exit (1);
        }
    // Detected an event from select
    printf ("Detected an event\n");

        // monitoring local port
        if (FD_ISSET (local_sockd, &rfds))
        {
            printf("Detected an event on local tcpd \n");
            receivedbytes = recvfrom (local_sockd, localbuffer_receive, 1024, 0,
                  (struct sockaddr *) NULL, (socklen_t *) NULL);
            // If the packet is from ftpc
            if (localbuffer_receive[0] == '1')
            {
                  printf("Client : Bytes received : %d\n", receivedbytes);
                  is_full = ((end + receivedbytes - 1 - start) > 64000) ? true : false;
                  if(!is_full) {
                  
                  // Put it into circular buffer
                  wraparoundend = 64000 - (end % 64000);
                  if(wraparoundend >= receivedbytes-1)
                      memcpy(clientbuffer + (end % 64000), localbuffer_receive + 1, receivedbytes - 1);
                  else{
                      memcpy(clientbuffer + (end % 64000), localbuffer_receive + 1, wraparoundend);
                      memcpy(clientbuffer, localbuffer_receive + wraparoundend + 1, receivedbytes - 1 - wraparoundend);
                  }
                  // Update end pointer
                  end += receivedbytes - 1;
                  // Check for windowsize and if data is available
                  printf("Window size : %d, Start : %d, Middle : %d, End : %d\n", windowsize, start, middle, end);
                  if(windowsize < 5 && middle < end){
                  int sentbytes = 1000<=end-middle ? 1000:end-middle;
                  // Update Aux DS
                  
                  auxilarydscontrol(&head, &tail, middle, sentbytes);
                  print_LL(head);
                  // Update window size
                  windowsize += 1;
                  memset (MyMessage.body, 0, sizeof(MyMessage.body));
                  memset (MyMessage.checksum, 0, sizeof(MyMessage.checksum));
                  wraparoundend = 64000 - (middle % 64000);
                  if(wraparoundend >= sentbytes)
                      memcpy (MyMessage.body, clientbuffer + (middle % 64000), sentbytes);
                  else{
                      memcpy (MyMessage.body, clientbuffer + (middle % 64000), wraparoundend);
                      memcpy (MyMessage.body + wraparoundend, clientbuffer, sentbytes - wraparoundend);
                  }
                  printf("Client side : Bytes sent : %d\n", sentbytes);
                  MyMessage.msg_length = sentbytes;
                  // Set sequence number
                  MyMessage.sequence_number = middle;
                  // Update middle
                  middle += sentbytes;
                  MyMessage.fromclient=true;
                  checksum_calc((unsigned char *)&MyMessage.body, (unsigned char*)&MyMessage.checksum, MSS+MSGLEN+SEQNOSIZE+CHECKSUMSIZE+1, 1);
                  sendto (remote_sockd, (char *) &MyMessage, sizeof MyMessage, 0,
                      (struct sockaddr *) &trolladdr,
                      (socklen_t) sizeof (struct sockaddr_in));
                  settimer(RTT/1000000.0, MyMessage.sequence_number);
                  //settimer(1000/1000000.0, MyMessage.sequence_number);
                  }
                  // Send an Ack to ftpc
                  sendto (local_sockd, (char *) ftpc_ack, sizeof ftpc_ack, 0,
                      (struct sockaddr *) &ftpcaddr,
                      (socklen_t) sizeof (struct sockaddr_in));
                  }
                  else{
                      memset(overflowbuf, 0, sizeof overflowbuf);
                      memcpy(overflowbuf, localbuffer_receive + 1, receivedbytes - 1);
                      overflowbufsize = receivedbytes - 1;
                  }
            }
            // If the packet is from ftps
            else if (localbuffer_receive[0] == '2')
            {
                printf ("Server\n");
                // Get the requested no of bytes
                //printf ("Port number : %d \n",
                //    ntohs (request_address.sin_port));
                char str[INET_ADDRSTRLEN];
                inet_ntop (AF_INET, &(request_address.sin_addr), str,
                   INET_ADDRSTRLEN);
                //printf ("IP : %s %d \n", str, INET_ADDRSTRLEN);
                //
                memcpy (&requestedbytes, localbuffer_receive + 1,
                    sizeof (uint32_t));
                // Changing from network order to host order
                requestedbytes = ntohl (requestedbytes);
                int canbesent = 0;
                printf("Triggered when server requests data\n");
                canbesent = checkifcanbesent(&head_s, start_s, requestedbytes);
                if(head_s != NULL)
                if(canbesent > 0){
                  wraparoundend = 64000 - (start_s % 64000);
                  if(wraparoundend >= canbesent)
                      memcpy(tempbuf, serverbuffer + (start_s % 64000), canbesent);
                  else{
                      memcpy(tempbuf, serverbuffer + (start_s % 64000), wraparoundend);
                      memcpy(tempbuf + wraparoundend, serverbuffer, canbesent - wraparoundend);
                  }
                    sendto (local_sockd, tempbuf, canbesent, 0, (struct sockaddr *) &request_address, (socklen_t) sizeof (request_address));
                    start_s += canbesent;
                    requestedbytes = 0;
                    canbesent = 0;
                }
            }
            else{
                timedout msg;
                memcpy(&msg, localbuffer_receive, receivedbytes);
                printf("Timer ran out : %c %d\n", msg.action, msg.sequence_number);
                canceltimer(msg.sequence_number);
                int resend_length = findll(head, msg.sequence_number);
                if(resend_length != -1){
                    memset (MyMessage.body, 0, sizeof(MyMessage.body));
                    memset (MyMessage.checksum, 0, sizeof(MyMessage.checksum));
                    wraparoundend = 64000 - (msg.sequence_number % 64000);
                    if(wraparoundend >= resend_length)
                        memcpy (MyMessage.body, clientbuffer + (msg.sequence_number % 64000),resend_length);
                    else{
                        memcpy (MyMessage.body, clientbuffer + (msg.sequence_number % 64000), wraparoundend);
                        memcpy (MyMessage.body + wraparoundend, clientbuffer,resend_length - wraparoundend);
                    }
                    printf("Client bytes sent : %d\n", resend_length);
                    MyMessage.msg_length = resend_length;
                    // Set sequence number
                    MyMessage.sequence_number = msg.sequence_number;
                    MyMessage.fromclient=true;
                    checksum_calc((unsigned char *)&MyMessage.body, (unsigned char*)&MyMessage.checksum, MSS+MSGLEN+SEQNOSIZE+CHECKSUMSIZE+1, 1);
                    sendto (remote_sockd, (char *) &MyMessage, sizeof MyMessage, 0,
                        (struct sockaddr *) &trolladdr,
                        (socklen_t) sizeof (struct sockaddr_in));
                      settimer(RTT/1000000.0, MyMessage.sequence_number);
                    printf("Retransmitting %d\n", msg.sequence_number);
                }
            }
        }
        // monitoring remote port & copying data into localbuffer when there is an event
        if (FD_ISSET (remote_sockd, &rfds))
        {
            struct Message test;
            recvfrom (remote_sockd, (char*)&test, sizeof test, 0, 
                (struct sockaddr *) NULL, (socklen_t *) NULL);
            memset (localbuffer_send, 0, sizeof(localbuffer_send));
            memcpy (localbuffer_send, test.body, MSS +MSGLEN+ SEQNOSIZE + CHECKSUMSIZE + 1);
	    // checking if the received packet is garbled
            if(checksum_calc(localbuffer_send, test.checksum, MSS+MSGLEN+SEQNOSIZE+CHECKSUMSIZE+1, 2) != true)
                printf("Packet Garbled \n");
            else{
                if(test.fromclient == true)
                {
                  print_LL(head_s);
                  wraparoundend = 64000 - (test.sequence_number % 64000);
                  if(wraparoundend >= test.msg_length)
                      memcpy(serverbuffer + (test.sequence_number % 64000), &test.body,test.msg_length);
                  else{
                      memcpy(serverbuffer + (test.sequence_number % 64000), &test.body,wraparoundend);
                      memcpy(serverbuffer, &test.body + wraparoundend, test.msg_length - wraparoundend);
                  }
                  if(findlls(head_s, test.sequence_number) && test.sequence_number >= start_s){
                      auxilarydscontrol(&head_s, &tail_s, test.sequence_number, test.msg_length);
                  }

                  test.fromclient=false;
                  memset (test.checksum, 0, sizeof(test.checksum));
                  memset (test.body, 0, sizeof(test.body));
                  checksum_calc((unsigned char *)&test.body, (unsigned char*)&test.checksum, MSS+MSGLEN+SEQNOSIZE+CHECKSUMSIZE+1, 1);
                  inet_aton(CLIENTIP, &(test.msg_header.sin_addr));
                  sendto (remote_sockd, (char *) &test, sizeof test, 0,
                      (struct sockaddr *) &trolladdr,
                      (socklen_t) sizeof (struct sockaddr_in));

                int canbesent = 0;
                printf("Triggered when data received from client\n");
                canbesent = checkifcanbesent(&head_s, start_s, requestedbytes);
                if(head_s != NULL)
                    if(canbesent > 0){
                        wraparoundend = 64000 - (start_s % 64000);
                        if(wraparoundend >= canbesent)
                            memcpy(tempbuf, serverbuffer + (start_s % 64000), canbesent);
                        else{
                            memcpy(tempbuf, serverbuffer + (start_s % 64000), wraparoundend);
                            memcpy(tempbuf + wraparoundend, serverbuffer, canbesent - wraparoundend);
                        }
                        sendto (local_sockd, tempbuf, canbesent, 0, (struct sockaddr *) &request_address, (socklen_t) sizeof (request_address));
                        start_s += canbesent;
                        requestedbytes = 0;
                        canbesent = 0;
                    }
                fflush (stdout);
                } 
                else {
                   printf("Acknowledged Seq. No %d\n",test.sequence_number);
                   canceltimer(test.sequence_number);
                   calculateRTTandRTO(test.sequence_number,head);
                   auxilarydsdelete(&head,&tail,test.sequence_number, &windowsize, &start);
                   print_LL(head);

                   if(is_full){
                      is_full = ((end + overflowbufsize - start) > 64000) ? true : false;
                      if(!is_full){
                          wraparoundend = 64000 - (end % 64000);
                          if(wraparoundend >= overflowbufsize)
                              memcpy(clientbuffer + (end % 64000), overflowbuf, overflowbufsize);
                          else{
                              memcpy(clientbuffer + (end % 64000), overflowbuf, wraparoundend);
                              memcpy(clientbuffer, overflowbuf + wraparoundend, overflowbufsize - wraparoundend);
                          }
                          // Update end pointer
                          end += overflowbufsize;
                          // Check for windowsize and if data is available
                          printf("Window size : %d, Start : %d, Middle : %d, End : %d\n", windowsize, start, middle, end);
                          // Send an Ack to ftpc
                          sendto (local_sockd, (char *) ftpc_ack, sizeof ftpc_ack, 0,
                              (struct sockaddr *) &ftpcaddr,
                              (socklen_t) sizeof (struct sockaddr_in));
                      }
                   }
                   // checking if it can be sent
                   if(windowsize < 5 && middle < end){
                   int sentbytes = 1000<=end-middle ? 1000:end-middle;
                   // Update Aux DS
                   
                   auxilarydscontrol(&head, &tail, middle, sentbytes);
                   print_LL(head);
                   // Update window size
                   windowsize += 1;
                   memset (MyMessage.body, 0, sizeof(MyMessage.body));
                   memset (MyMessage.checksum, 0, sizeof(MyMessage.checksum));
                   wraparoundend = 64000 - (middle % 64000);
                   if(wraparoundend >= sentbytes)
                       memcpy (MyMessage.body, clientbuffer + (middle % 64000), sentbytes);
                   else{
                       memcpy (MyMessage.body, clientbuffer + (middle % 64000), wraparoundend);
                       memcpy (MyMessage.body + wraparoundend, clientbuffer, sentbytes - wraparoundend);
                   }
                   printf("Client : Bytes sent : %d\n", sentbytes);
                   MyMessage.msg_length = sentbytes;
                   // Set sequence number
                   MyMessage.sequence_number = middle;
                   // Update middle
                   middle += sentbytes;
                   MyMessage.fromclient=true;
                   checksum_calc((unsigned char *)&MyMessage.body, (unsigned char*)&MyMessage.checksum, MSS+MSGLEN+SEQNOSIZE+CHECKSUMSIZE+1, 1);
                   sendto (remote_sockd, (char *) &MyMessage, sizeof MyMessage, 0,
                       (struct sockaddr *) &trolladdr,
                       (socklen_t) sizeof (struct sockaddr_in));
                  settimer(RTT/1000000.0, MyMessage.sequence_number);
                   }
                }
            }
        }
        FD_ZERO (&rfds);
        FD_SET (local_sockd, &rfds);
        FD_SET (remote_sockd, &rfds);
        printf ("Going back\n");
}


    close (local_sockd);
    close (remote_sockd);

    return 0;
}
