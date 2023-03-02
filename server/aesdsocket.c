/*
* Author: Malcolm McKellips
* File: aesdsocket.c
* Class: AESD
* Purpose: Open a socket for receiving data and outputing to a file.
* 
*/
#include <stdio.h>
#include <stdlib.h>
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

//#define RECVBUFF_SIZE 100 //this will be the size per block of recv buffer, if it fills up new block of this size will be added on
//Note: Current implementation instead recv's byte at a time to simplify recv logic

//-------------------------------------Globals-------------------------------------
//Reference for signal handler strategy with flag: https://www.jmoisio.eu/en/blog/2020/04/20/handling-signals-correctly-in-a-linux-application/
static volatile sig_atomic_t signal_flag = 0; //this flag will be set if sigterm or sigint are received
pthread_mutex_t pdfile_lock; //mutex for aesdsocketdata file (packet data file) protection

//-------------------------------------Signal Handler-------------------------------------
void handle_sigint_sigterm(int sigval){
    syslog((LOG_USER | LOG_INFO),"Caught signal, exiting");
    signal_flag = 1;
}

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


//-------------------------Reading and Writing Functionality----------------------
//Note, assumes that fd is open and connection_fd is connected
int write_file_to_socket(int fd, int connection_fd){
    //TODO: might want to add EINTR error check in case seek is interrupted by signal
    off_t old_pos = lseek(fd,0,SEEK_CUR);
    if (old_pos == (off_t)-1){
         syslog((LOG_USER | LOG_INFO),"Error in seek");
         return -1;
    }
    //now read from the start of the file
    off_t new_pos = lseek(fd,0,SEEK_SET);
    if (new_pos == (off_t)-1){
         syslog((LOG_USER | LOG_INFO),"Error in seek");
         return -1;
    }    
    
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
    if (old_pos == (off_t)-1){
         syslog((LOG_USER | LOG_INFO),"Error in seek");
         return -1;
    }
    
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
            ssize_t bytes_written = 0;
            while (bytes_written != recv_buff_size){
                
                pthread_mutex_lock(&pdfile_lock);
                bytes_written = write(threadParams->packetdata_fd, recv_buff,recv_buff_size);
                pthread_mutex_unlock(&pdfile_lock);
                
                if (bytes_written == -1){
                    syslog((LOG_USER | LOG_INFO),"Error writing packet to tmp data file!");
                    threadParams->thread_complete = 1;
                    pthread_exit(NULL);
                }
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
            pthread_mutex_lock(&pdfile_lock);
            int f2sRes = write_file_to_socket(threadParams->packetdata_fd, threadParams->connection_fd);
            pthread_mutex_unlock(&pdfile_lock);       
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
     int packetdata_fd = open("/var/tmp/aesdsocketdata", (O_RDWR | O_CREAT | O_APPEND), 0644);
     if (packetdata_fd == -1){
         syslog((LOG_USER | LOG_INFO),"Errror creating/opening temp data file");
         return -1;
     }
    
    //Initialize mutex for packet data file
    pthread_mutex_init(&pdfile_lock,NULL);

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
             threadParams_t *newThreadParams = getThreadParams(client_addr,packetdata_fd, connection_fd);
             
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


    
    close(packetdata_fd);
    //remove the data file
    if (remove ("/var/tmp/aesdsocketdata") !=0 ){
        syslog((LOG_USER | LOG_INFO),"Issue deleting data file!");
        return -1;
    } 
    close(socket_fd);
    
    return 0;    
}    
