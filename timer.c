#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define TIMERPORT 1031
#define TCPDPORT 10809

bool lockres = false;
// Creating tcpd socket
struct sockaddr_in tcpd_socket;
int sock;


typedef struct timedout{
    char action;
    int sequence_number;
}timedout;

typedef struct timermessage{
    int action;
    int sequence_number;
    float time;
}timermessage;

typedef struct timernode{
    int sequence_number;
    float time;
    struct timernode* next;
}timernode;

int timernodesize = sizeof(timernode);

timernode *head = NULL;
float TimerGet(){
    struct itimerval it_val;
    getitimer(ITIMER_REAL, &it_val);
    float time = it_val.it_value.tv_sec + (it_val.it_value.tv_usec / 1000000.0);
    return time;
}

void canceltimer(timernode**, int);
void TimerSet(int, int);

/* Function to stop the timer */
void TimerStop(int signum) {
    if(head == NULL) return;
    printf("\nTimer ran out! Stopping timer for sequence no %d\n", head->sequence_number);
    timedout msg;
    msg.action = '3';
    msg.sequence_number = head->sequence_number;
    sendto (sock, (char *) &msg, sizeof msg, 0,
        (struct sockaddr *) &tcpd_socket,
        (socklen_t) sizeof (struct sockaddr_in));
    return;
}

/* Function to set the timer */
void TimerSet(int interval, int microseconds) {
    struct itimerval it_val;

    it_val.it_value.tv_sec = interval;
    it_val.it_value.tv_usec = microseconds + 10;
    it_val.it_interval.tv_sec = 0;
    it_val.it_interval.tv_usec = 0;

    if (signal(SIGALRM, TimerStop) == SIG_ERR) {
        perror("Unable to catch SIGALRM");
        exit(1);
    }
    if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
        perror("error calling setitimer()");
        exit(1);
    }
}


/* Function to print the entire list of delta timer */
void printdeltatimer(timernode *head){
    if(head == NULL){
        printf("Printing delta timer : Delta timer is empty \n");
        return;
    }
    head->time = TimerGet();
    printf("Delta timer : ");
    while(head){
        printf("%d|%02f -> ", head->sequence_number, head->time);
        head = head->next;
    }
    printf("NULL\n");
}

/* Function to add a new node to the delta timer and set it */
void settimer(timernode **head, int sequence_number, float time){
    lockres = true;
    int seconds;
    float useconds;
    if(*head == NULL){
        (*head) = (timernode*)malloc(sizeof(timernode));
        (*head)->sequence_number = sequence_number;
        (*head)->time = time;
        (*head)->next = NULL;
        // Set the timer
        seconds = (int)time;
        useconds = time - seconds;
        printf("Checking time : %d %f\n", seconds, useconds);
        TimerSet(seconds, useconds*1000000);
        printdeltatimer(*head);
        lockres = false;
        return;
    }
    (*head)->time = TimerGet();
    timernode *cnode = *head, *cnodenext, *previous = NULL;
    bool changehead = true;
    while(cnode != NULL && cnode->time < time){
        changehead = false;
        time = time - cnode->time;
        if(cnode->next == NULL){
            break;
        }
        previous = cnode;
        cnode = cnode->next;
    }
    printf("Error over here for malloc\n");
    timernode *newtn = (timernode *)malloc(timernodesize);
    newtn->sequence_number = sequence_number;
    newtn->time= time;
    if(changehead){
        newtn->next = (*head);
        (*head)->time -= newtn->time;
        (*head) = newtn;
        time = (*head)->time;
        seconds = (int)time;
        useconds = time - seconds;
        TimerSet(seconds, useconds*1000000);
    }
    else{
        fflush(stdout);
        if(cnode->next == NULL && cnode->time < time){
            newtn->time -= cnode->time;
            newtn->next = NULL;
            cnode->next = newtn;
        }
        else if(previous == NULL){
            cnode->next = newtn;
            newtn->next = NULL;
        }
        else{
            previous->next = newtn;
            newtn->next = cnode;
            cnode->time -= newtn->time;
        }
        lockres = false;
    }
    printdeltatimer(*head);
    return;
}

/* Function to remove a node from the delta timer and cancel it's timer */
void canceltimer(timernode **head, int sequence_number){
    if(*head == NULL){
        printf("Cancelling timer : Delta timer is empty \n");
        return;
    }
    int seconds;
    float useconds;
    timernode *cnode = *head, *dnode, *previous = NULL; float timetobeadded;
    if((*head)->sequence_number == sequence_number){
        dnode = *head;
        (*head) = (*head)->next;
        free(dnode);
        if(*head != NULL){
        (*head)->time = (*head)->time + TimerGet();
        seconds = (int)((*head)->time);
        useconds = ((*head)->time) - seconds;
        TimerSet(seconds, useconds*1000000);
        }
        else{
        printf("Setting to zero\n");
        seconds = 0;
        useconds = 0;
        TimerSet(seconds, useconds*1000000);
        }
        printdeltatimer(*head);
        return;
    }
    else{
        while(cnode->sequence_number != sequence_number){
            if(cnode->next == NULL){
                break;
            }
            previous = cnode;
            cnode = cnode->next;
        }
        if(cnode->next == NULL) {
            if(cnode->sequence_number == sequence_number){
                previous->next = NULL;
                free(cnode);
            }
            else{
                printf("Sorry. Sequence no does not exist. \n");
            }
        }
        else{
            previous->next = cnode->next;
            cnode->next->time += cnode->time;
            free(cnode);
        }
        printdeltatimer(*head);
    }
    return;
}

int main(int argc, char *argv[]){
    int namelen;
    struct sockaddr_in name;
    //Tcpd related
    tcpd_socket.sin_family = AF_INET;
    tcpd_socket.sin_addr.s_addr = INADDR_ANY;
    tcpd_socket.sin_port = htons (TCPDPORT);

    /*create socket*/
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) {
	perror("opening datagram socket");
	exit(1);
    }

    /* create name with parameters and bind name to socket */
    name.sin_family = AF_INET;
    name.sin_port = htons(TIMERPORT);
    name.sin_addr.s_addr = INADDR_ANY;
    if(bind(sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
        perror("getting socket name");
        exit(2);
    }
    namelen=sizeof(struct sockaddr_in);
    /* Find assigned port value and print it for client to use */
    if(getsockname(sock, (struct sockaddr *)&name, &namelen) < 0){
        perror("getting sock name");
        exit(3);
    }
    printf("Server waiting on port # %d\n", ntohs(name.sin_port));

    /* waiting for connection from client on name and print what client sends */
    namelen = sizeof(name);
    timermessage rmsg;
    int test=0;
    while(1){
    test++;
    if(recvfrom(sock, &rmsg, sizeof rmsg, 0, (struct sockaddr *)&name, &namelen) < 0) {
        perror("error receiving"); 
        exit(4);
    }
    if(rmsg.action == 1){
        printf("\n%d : Setting the timer for %f seconds with sequence no %d\n", test, rmsg.time, rmsg.sequence_number);
        settimer(&head, rmsg.sequence_number, rmsg.time);
    }
    else if(rmsg.action == 2){
        printf("\n%d : Cancelling timer sequence no %d\n", test, rmsg.sequence_number);
        canceltimer(&head, rmsg.sequence_number);
    }
    }

    /* server terminates connection, closes socket, and exits */
    close(sock);
    exit(0);
    return 0;
}
