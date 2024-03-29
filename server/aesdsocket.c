/*
* Author: Malcolm McKellips
* File: aesdsocket.c
* Class: AESD
* Purpose: Open a socket for receiving data and outputing to a file.
* 
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>
#include "freebsdqueue.h"
#include <sys/time.h>
#include "../aesd-char-driver/aesd_ioctl.h"

#define MAX_TIMESTR_SIZE 100
#define USE_AESD_CHAR_DEVICE (1)
#define AESD_COMMAND_STR "AESDCHAR_IOCSEEKTO:"
#define AESD_COMMAND_SIZE (19)

//#define RECVBUFF_SIZE 100 //this will be the size per block of recv buffer, if it fills up new block of this size will be added on
//Note: Current implementation instead recv's byte at a time to simplify recv logic

//-------------------------------------Globals-------------------------------------
//Reference for signal handler strategy with flag: https://www.jmoisio.eu/en/blog/2020/04/20/handling-signals-correctly-in-a-linux-application/
static volatile sig_atomic_t signal_flag = 0; //this flag will be set if sigterm or sigint are received
#if USE_AESD_CHAR_DEVICE == 0
pthread_mutex_t pdfile_lock; //mutex for aesdsocketdata file (packet data file) protection
#endif
int g_datafd; //global file descriptor to allow timestamp interval timer to access data file

//-------------------------------------Signal Handlers-------------------------------------
void handle_sigint_sigterm(int sigval){
    syslog((LOG_USER | LOG_INFO),"Caught signal, exiting");
    signal_flag = 1;
}

#if USE_AESD_CHAR_DEVICE == 0
//time stamp alarm handler (handle sigalrm)
void ts_alarm_handler(int signo){
    time_t time_now;
    time_t ret = time(&time_now);
    if(ret == -1){
        syslog((LOG_USER | LOG_INFO),"Error getting timestamp\r\n");
        return;
    }

    struct tm tm_now;


    localtime_r(&time_now,&tm_now);
    //2822 format: "%a, %d %b %Y %T %z"
    //weekday, day of month, month, year, 24hr time (H:M:S), numeric timezone
    char time_buff[MAX_TIMESTR_SIZE];
    size_t time_str_size = strftime(time_buff,MAX_TIMESTR_SIZE,"timestamp:%a, %d %b %Y %T %z\n",&tm_now);
    if (!time_str_size){
        syslog((LOG_USER | LOG_INFO),"timestamp not generated\r\n");
        return;
    }

    ssize_t bytes_written = 0;
    while (bytes_written != time_str_size){
        
        #if USE_AESD_CHAR_DEVICE == 0
        pthread_mutex_lock(&pdfile_lock);
        #endif
        bytes_written = write(g_datafd, time_buff,(time_str_size-bytes_written));
        #if USE_AESD_CHAR_DEVICE == 0
        pthread_mutex_unlock(&pdfile_lock);
        #endif
        
        if (bytes_written == -1){
            syslog((LOG_USER | LOG_INFO),"Error writing timestamp!");
            return;
        }
    }


    return;
}
#endif


//-------------------------Thread Organization and Functions----------------------
struct threadParams_s{
    pthread_t thread;    
    int packetdata_fd;
    int connection_fd;
    char client_ip_str[INET6_ADDRSTRLEN];
    int thread_complete;
    SLIST_ENTRY(threadParams_s) qEntries;
};

typedef struct threadParams_s threadParams_t; 

struct threadParams_s* getThreadParams(struct sockaddr_storage client_addr, int pd_fd, int c_fd){
    struct threadParams_s *newP = malloc(sizeof(struct threadParams_s));
    
    const char* client_ip_res = inet_ntop(AF_INET6,&(((struct sockaddr_in6*)((struct sockaddr *)&client_addr))->sin6_addr) ,newP->client_ip_str,sizeof(newP->client_ip_str)); //reference: https://stackoverflow.com/questions/12810587/extracting-ip-address-and-port-info-from-sockaddr-storage
             
             
    if (client_ip_res == NULL){
        syslog((LOG_USER | LOG_INFO),"Errror extracting IP from client address");
        return NULL;
    }
    else{
        syslog((LOG_USER | LOG_INFO),"Accepted connection from %s",client_ip_res);
        newP->thread_complete = 0; 
        newP->packetdata_fd = pd_fd;
        newP->connection_fd = c_fd;
    }
    
    return newP;

}

//----------------------New to A9: Handle Write Commands--------------------------
//Function which takes a string and compares it against expected aesdchar seek string format
//com:          command string
//com_len:      length of command string 
//seekto_vals:  pointer to aesd_seekto structure to update with parsed write values 
//Returns:
//          -> 0 if error or command not matching expected format
//          -> 1 if command matches expected format

static int parse_aesd_write(char * com, ssize_t com_len, struct aesd_seekto* seekto_vals){
    
    if (com_len < AESD_COMMAND_SIZE)
        return 0;

    //Check if first 19 characters match expected command format
    for (ssize_t i = 0; i < AESD_COMMAND_SIZE; i++){
        if (com[i] != AESD_COMMAND_STR[i])
            return 0; 
    }

    //Ensure that the string is null terminated before performing string operations
    //This is probably unnecessary, because strtol scans from start of the string, 
    //but after last assignment, better safe than sorry on null terminators
    char null_termed_com[com_len+1];
    for (ssize_t i = 0; i < com_len; i++){
        null_termed_com[i] = com[i];
    }
    null_termed_com[com_len] = '\0';

    //Now validate that there is a number as the next value: 
    char *rem_str; //string remaining after first digit extraction
    uint32_t X_write_com = (uint32_t)strtol(null_termed_com+AESD_COMMAND_SIZE, &rem_str, 10);
    if(rem_str == null_termed_com+AESD_COMMAND_SIZE)
        return 0; //First number was not found

    if(rem_str == NULL)
        return 0; //There was nothing after the first number

    if (rem_str[0] != ',')
        return 0; //next character after found string wasn't , 

    if (strlen(rem_str) < 2)
        return 0; //there is nothing after the comma

    char *end_str; //the string contents after the second number 
    uint32_t Y_write_offs = (uint32_t)strtol(rem_str+1, &end_str, 10); //+1 to account for comma
    if(end_str == rem_str+1)
        return 0; //Second number was not found
    

    //If we've made it here, parsing was successful and a seekto command was recvd
    seekto_vals->write_cmd = X_write_com;
    seekto_vals->write_cmd_offset = Y_write_offs;

    return 1;
}

//-------------------------Reading and Writing Functionality----------------------
//Note, assumes that fd is open and connection_fd is connected
int write_file_to_socket(int fd, int connection_fd){
    //TODO: might want to add EINTR error check in case seek is interrupted by signal
    #if USE_AESD_CHAR_DEVICE == 0
    off_t old_pos = lseek(fd,0,SEEK_CUR);
    lseek(fd, 0 , SEEK_CUR);

    if (old_pos == (off_t)-1){
         syslog((LOG_USER | LOG_INFO),"Error in old seek");
         return -1;
    }


    lseek(fd, 0, SEEK_SET);
    syslog((LOG_USER | LOG_INFO),"Setting pos to 0");

    //now read from the start of the file
    off_t new_pos = lseek(fd,0,SEEK_SET);
    if (new_pos == (off_t)(-1)) {
         syslog((LOG_USER | LOG_INFO),"Error in new seek");
         return -1;
    }    
    #endif
    

    //write character by character from file to socket. likely inefficient, but easy to wrap my head around
    char read_buff[1];
    ssize_t bytes_read;
    int bytes_sent;
    
    while((bytes_read = read(fd,read_buff,1)) != 0){
        if (bytes_read == -1){
            if ((errno == EINTR) && signal_flag) {
                 syslog((LOG_USER | LOG_INFO),"file read interrupted by signal, finishing file operations then begin clean termination...");
             }
             else{
                 syslog((LOG_USER | LOG_INFO),"Error in file read");
                 return -1;
             }    
        }
        else{
            //successfully read byte
            retry_send:
            bytes_sent = send(connection_fd,read_buff,1,0);
            if (bytes_sent != 1){
                if ((errno == EINTR) && signal_flag) {
                     syslog((LOG_USER | LOG_INFO),"socket_write interrupted by signal, finishing file operations then begin clean termination...");
                     goto retry_send; //we were interrupted by signal handler, retry send as per https://beej.us/guide/bgnet/pdf pg.77
                 }
                 else{
                     syslog((LOG_USER | LOG_INFO),"Error in socket write");
                     return -1;
                 }                
            }
        }
    }
    
    //return the file position to where it was...
    #if USE_AESD_CHAR_DEVICE == 0
    if (old_pos == (off_t)-1){
         syslog((LOG_USER | LOG_INFO),"Error in resetting seek");
         return -1;
    }
    #endif

    return 0;    
}

void *connectionThreadWork(void *threadParamsIn){

     threadParams_t *threadParams = (threadParams_t *)threadParamsIn;
     
     char* recv_buff = (char*)malloc(1*sizeof(char));
     if (recv_buff == NULL){
         syslog((LOG_USER | LOG_INFO),"Error when allocating initial recv buffer block!");
         threadParams->thread_complete = 1;
         pthread_exit(NULL);
     }
     
     int recv_buff_size    = 1;  //will track how much mem is allocated to buffer of received characters
     int recv_block_bytes  = 0;  //will track the number of bytes read each iteration of the loop (should be 0 or 1)
     int newline_recv      = 0;  //acting as a boolean for whether or not the newest character was a newline
     int connection_closed = 0;  //acting as a bollean for whether or not connection has closed
     

     while (!connection_closed && !signal_flag){
         //Receive bytes from connection, one byte at a time until newline
         newline_recv = 0;
         while((!newline_recv) && (!connection_closed) && (!signal_flag)){
             recv_block_bytes = recv(threadParams->connection_fd,recv_buff+recv_buff_size-1,1,0);
             if (recv_block_bytes == 0){
                 syslog((LOG_USER | LOG_INFO),"Connection closed");
                 recv_buff_size--; 
                 connection_closed = 1;
             }
             else if (recv_block_bytes == -1){
                 if (signal_flag) {
                     syslog((LOG_USER | LOG_INFO),"recv interrupted by signal, begin clean termination...\r\n");
                 }
                 else{
                     syslog((LOG_USER | LOG_INFO),"Error in socket recv");
                     threadParams->thread_complete = 1;
                     pthread_exit(NULL);
                 }
                     
             }
             else{
                 //Check if new byte is a newline
                 if (recv_buff[recv_buff_size - 1] == '\n'){
                     newline_recv = 1;
                 }
                 //If can't assume command string has a newline, must process seek command detection here
                 else{
                     //add new byte to buffer to be read in at the next loop execution
                     recv_buff_size ++;
                     recv_buff = realloc(recv_buff, (recv_buff_size)*sizeof(char)); //allocate
                     if (recv_buff == NULL){
                         syslog((LOG_USER | LOG_INFO),"Error current packet received is larger than heap size!\r\n");
                         threadParams->thread_complete = 1;
                         pthread_exit(NULL);
                     }
                 }
             }
         }
             
         if (newline_recv){
             //if we've reached here, its time to write to file, newline recvd

            //Variables for tracking ioctl command
            struct aesd_seekto seekto;
            int    ioctl_packetdata_fd; //fd to use when ioctl command recvd (don't close and reopen after seek or fpos lost)
            int    seekcom_recv = 0; //bool of if valid seekto ioctl comm recvd or not

            //If not using our char device, never attempt to execute ioctl etc.
            #if USE_AESD_CHAR_DEVICE == 0
                seekcom_recv = 0;
            #else
                seekcom_recv = parse_aesd_write(recv_buff, recv_buff_size, &seekto); //should put correct offsets in seekto
            #endif

            if (seekcom_recv){
                //We have received a valid command string
                ioctl_packetdata_fd = open("/dev/aesdchar", (O_RDWR  | O_APPEND));
                int ioctl_ret = ioctl(ioctl_packetdata_fd, AESDCHAR_IOCSEEKTO, &seekto);
                if (ioctl_ret){
                    syslog((LOG_USER | LOG_INFO),"ERROR IN IOCTL, killing thread...\r\n");
                    threadParams->thread_complete = 1;
                    pthread_exit(NULL);
                }
                //don't write into device. 
                //write the file to the connection with same fd as ioctl above
            }

            //We can reset our recieve buffer to a single char in preparation for recieving next packet
            recv_buff_size = 1;
            recv_buff = realloc(recv_buff, (recv_buff_size)*sizeof(char)); //allocate
            if (recv_buff == NULL){
                syslog((LOG_USER | LOG_INFO),"Error resetting recv buffer size!");
                threadParams->thread_complete = 1;
                pthread_exit(NULL);
            }
                 
            //write the file to the connection
            //Note: might want more granular locking within the function, but since file pos will be moved throughout, it might be best to lock entire function.
            #if USE_AESD_CHAR_DEVICE == 0
                pthread_mutex_lock(&pdfile_lock);
            #else
                //if seekcom was received, we already have an open fd to use for reading and writing file out to socket
                if (!seekcom_recv)
                    threadParams->packetdata_fd = open("/dev/aesdchar", (O_RDWR  | O_APPEND));
                else
                    threadParams->packetdata_fd = ioctl_packetdata_fd; 
            #endif
            int f2sRes = write_file_to_socket(threadParams->packetdata_fd, threadParams->connection_fd);
            #if USE_AESD_CHAR_DEVICE == 0
                pthread_mutex_unlock(&pdfile_lock);
            #else
                close(threadParams->packetdata_fd);
            #endif

            if (f2sRes == -1){
                syslog((LOG_USER | LOG_INFO),"Error writting file to socket!");
                threadParams->thread_complete = 1;
                pthread_exit(NULL);
            }
        }
    }
    
    //If we reach here, either the connection was closed or sigint or sigterm were recvd
    close(threadParams->connection_fd); //might wanna check return value
    syslog((LOG_USER | LOG_INFO),"Closed connection from %s",threadParams->client_ip_str);

    free(recv_buff);
    
    threadParams->thread_complete = 1;     
    pthread_exit(NULL);
}

int main(int argc, char*argv[]){
    
    //reference:https://www.jmoisio.eu/en/blog/2020/04/20/handling-signals-correctly-in-a-linux-application/
    //Add signal handler for sig int and sigterm
    sigset_t blocked_signals;
    struct sigaction sa;
    sigemptyset(&blocked_signals);
    sa.sa_handler = &handle_sigint_sigterm;
    sa.sa_mask = blocked_signals;
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
   
    
    //References: AESD course slides "Sockets pg. 15: Getting Sockaddr", "https://beej.us/guide/bgnet: pg. 21"
    //The below was pulled almost directly from above resources
     
     //Get address info for localhost port 9000
     struct addrinfo hints;
     memset(&hints, 0, sizeof(hints));
     
     hints.ai_flags    = AI_PASSIVE;
     hints.ai_socktype = SOCK_STREAM;
     hints.ai_family   = AF_INET6;
     
     struct addrinfo *ai_result;
     
     int rc = getaddrinfo(NULL, "9000", &hints, &ai_result);
     
     if (rc != 0){
         syslog((LOG_USER | LOG_INFO),"Error obtaining address info!");
         return -1;
     }


     //bind address to socket
     //reference:https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf pg. 35
     int socket_fd;
     struct addrinfo *current_node; //current node of LL containing addr info results
     
     //Traverse linked list looking for valid address info to create and bind socket
     for (current_node = ai_result; current_node != NULL; current_node=current_node->ai_next){
         socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
        if (socket_fd == -1){
            syslog((LOG_USER | LOG_INFO),"socket creation failed, trying next");
            continue;
        }
        
        int reuse_value=1; //boolean value to set SO_REUSEADDR: 
        rc = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_value, sizeof(reuse_value));
        if (rc == -1){
            syslog((LOG_USER | LOG_INFO),"Error setting socket address to reusable!");
            return -1;
        }
        
        rc = bind(socket_fd, current_node->ai_addr, current_node->ai_addrlen);
        if (rc == -1){
            close(socket_fd);
             syslog((LOG_USER | LOG_INFO),"socket bind failed, trying next");
             continue;
         }
         break;
     }
     
     freeaddrinfo(ai_result); //no longer need address info after binding
     
     if (current_node == NULL){
         syslog((LOG_USER | LOG_INFO),"Error binding socket! No socket binded, exiting program...");
        return -1;
     }
     else{
         syslog((LOG_USER | LOG_INFO),"Socket successfully binded!");
         if (argc > 1){
             if (!strcmp(argv[1],"-d")){
                 //we are goin demon mode
                if ( daemon(0,1) == -1){
                    syslog((LOG_USER | LOG_INFO),"Error binding socket! No socket binded, exiting program...");
                    return -1;
                }
                
             }
         }
     }
     
     //listen for connections
     rc = listen(socket_fd, 5); //wait for connection, allow backlog of 5 waiters...
     if (rc == -1){
         syslog((LOG_USER | LOG_INFO),"Error listening to socket");
         return -1;
     }
         
     //Open or Create file for input data
     #if USE_AESD_CHAR_DEVICE == 1   
        syslog((LOG_USER | LOG_INFO),"Using char driver rather than data file");
        //int packetdata_fd = open("/dev/aesdchar", (O_RDWR  | O_APPEND));
    #else
        syslog((LOG_USER | LOG_INFO),"Using data file rather than char driver");
        int packetdata_fd = open("/var/tmp/aesdsocketdata", (O_RDWR | O_CREAT | O_APPEND), 0644);
    
     
     if (packetdata_fd == -1){
         syslog((LOG_USER | LOG_INFO),"Errror creating/opening temp data file");
         return -1;
     }

     g_datafd = packetdata_fd;

    #endif
    //Initialize mutex for packet data file
     #if USE_AESD_CHAR_DEVICE == 0
    pthread_mutex_init(&pdfile_lock,NULL);
    #endif


    //Signal handler for sigalrm and 10 second interval timer setup
    //Note only register this AFTER datafile has opened.
    #if USE_AESD_CHAR_DEVICE == 0
    signal(SIGALRM, ts_alarm_handler);

    struct itimerval ts_delay;
    ts_delay.it_value.tv_sec        = 10;  //first delay: 
    ts_delay.it_value.tv_usec       = 0;   //first dalay us
    ts_delay.it_interval.tv_sec     = 10;  //repeated delay
    ts_delay.it_interval.tv_usec    = 0;   //repeated delay us

    tzset();

    int ret = setitimer(ITIMER_REAL, &ts_delay, NULL);
    if (ret){
        perror("settimer failure");
        return 1;
    }
    #endif

     //Set up connection LL
     int num_connections = 0;
     SLIST_HEAD(slisthead,threadParams_s) head = SLIST_HEAD_INITIALIZER(head);
     SLIST_INIT(&head);
     //Used for joining dead threads/iterating over threads
     threadParams_t *curThread = NULL;
     threadParams_t *tmpPtr = NULL;
     
     while(!signal_flag){ //forever wait for connections
         //reference: "https://beej.us/guide/bgnet: pg. 28"
         struct sockaddr_storage client_addr; //address of the remote client connecting. Note that sockaddr_storage can fit ipv6 or v4
         socklen_t client_addr_size = sizeof(client_addr);

         int connection_fd;
         //char client_ip_str[INET6_ADDRSTRLEN]; //has been moved into thread parameters as of A6

         
         connection_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_addr_size);
         if(connection_fd == -1){
             if ((errno == EINTR) && signal_flag) {
                 syslog((LOG_USER | LOG_INFO),"socket accept interrupted by signal, begin clean termination...\r\n");
             }
             else{
                 syslog((LOG_USER | LOG_INFO),"Error accepting socket connection");
                 return -1;
             }
         }
         else{
             //successful connection!
            #if USE_AESD_CHAR_DEVICE == 0
             threadParams_t *newThreadParams = getThreadParams(client_addr,packetdata_fd, connection_fd);
            #else
             threadParams_t *newThreadParams = getThreadParams(client_addr,-1, connection_fd);
            #endif
             if (newThreadParams == NULL){
                syslog((LOG_USER | LOG_INFO),"Errror initializing thread parameters");
                return -1;
             }
             
             if (pthread_create(&(newThreadParams->thread),NULL,connectionThreadWork,(void*)newThreadParams)){
                 syslog((LOG_USER | LOG_INFO),"Errror initializing thread parameters");
                 return -1;           
             }

             
             //Define head if this is our first connection
             if (num_connections == 0){
                 SLIST_INIT(&head);
                 SLIST_INSERT_HEAD(&head, newThreadParams, qEntries);
             }
             //Add thread info to LL
             else{
                 SLIST_INSERT_HEAD(&head, newThreadParams, qEntries);
             }
             
             num_connections++;
             syslog((LOG_USER | LOG_INFO),"Current Number of Connections: %d",num_connections);
             
             //Join dead threads           
             SLIST_FOREACH_SAFE(curThread, &head, qEntries, tmpPtr){
                 if (curThread->thread_complete){
                     syslog((LOG_USER | LOG_INFO),"Killing connection thread");
                     pthread_join(curThread->thread,NULL); //TODO:might want to check retval rather than NULL
                     SLIST_REMOVE(&head, curThread, threadParams_s, qEntries);
                     free(curThread);
                     num_connections --;
                     syslog((LOG_USER | LOG_INFO),"Current Number of Connections: %d",num_connections);
                 }
             }     
         }
    }
    
    
   //Wait for threads to finish up 
   SLIST_FOREACH_SAFE(curThread, &head, qEntries, tmpPtr){
         syslog((LOG_USER | LOG_INFO),"Killing connection thread");
         pthread_kill(curThread->thread,SIGINT);
         pthread_join(curThread->thread,NULL); //TODO:might want to check retval rather than NULL
         SLIST_REMOVE(&head, curThread, threadParams_s, qEntries);
         free(curThread);
         num_connections--;
         syslog((LOG_USER | LOG_INFO),"Current Number of Connections: %d",num_connections);
    }


    
    #if USE_AESD_CHAR_DEVICE == 0
    close(packetdata_fd);
    
    //remove the data file
    if (remove ("/var/tmp/aesdsocketdata") !=0 ){
        syslog((LOG_USER | LOG_INFO),"Issue deleting data file!");
        return -1;
    } 
    #endif
    close(socket_fd);
    
    return 0;    
}    
