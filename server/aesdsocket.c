/*
* Author: Malcolm McKellips
* File: aesdsocket.c
* Class: AESD
* Purpose: Open a socket for receiving data and outputing to a file
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

#define RECVBUFF_SIZE 100 //this will be the size per block of recv buffer, if it fills up new block of this size will be added on

int main(){
	//Open socket bound to port 9000 return -1 if failure
	int socket_fd = socket(PF_INET6, SOCK_STREAM, 0);
	if (socket_fd == -1){
		printf("Error creating socket!\r\n");
		return -1;
	}
	
	//TODO:Might want to set SO_REUSEADDR
	
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
 		printf("Error obtaining address info!\r\n");
 		return -1;
 	}
 	
 	
 	//bind address to socket
 	rc = bind(socket_fd, ai_result->ai_addr, sizeof(struct sockaddr));
 	
 	if (rc == -1){
 		printf("Error binding socket to address!\r\n");
 		return -1;
 	}
 	
 	//listen for connections
 	rc = listen(socket_fd, 5); //wait for connection, allow backlog of 5 waiters...
 	if (rc == -1){
 		printf("Error listening to socket\r\n");
 		return -1;
 	}
 		
 	//Open or Create file for input data
 	int packetdata_fd = open("/var/tmp/aesdsocketdata", (O_RDWR | O_CREAT | O_APPEND), 0644);
 	if (packetdata_fd == -1){
 		printf("Errror creating/opening temp data file\r\n");
 		return -1;
 	}

	
 	//forever wait for connections...
 	while(1){
 		//reference: "https://beej.us/guide/bgnet: pg. 28"
 		struct sockaddr_storage client_addr; //address of the remote client connecting. Note that sockaddr_storage can fit ipv6 or v4
 		socklen_t client_addr_size = sizeof(client_addr);
 	
 		int connection_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_addr_size);
 	
 		if(connection_fd == -1){
 			printf("Error accepting socket connection\r\n");
 			return -1;
 		}
 	
 		//reference: https://stackoverflow.com/questions/12810587/extracting-ip-address-and-port-info-from-sockaddr-storage

 		char client_ip_str[INET6_ADDRSTRLEN];
 	
 		const char* client_ip_res = inet_ntop(AF_INET6,get_in_addr((struct sockaddr *)&client_addr) ,client_ip_str,sizeof(client_ip_str));
 	
 		if (client_ip_res == NULL){
 			printf("Errror extracting IP from client address\r\n");
 			return -1;
 		}
 		
 		printf("Accepted connection from %s\r\n",client_ip_res);
 		syslog((LOG_USER | LOG_INFO),"Accepted connection from %s",client_ip_res);
 	
 		//Move this and any other deallocs to signal handler...
 		freeaddrinfo(ai_result);
 	
 		//dynamically allocate a buffer for receiving
 		//recv_buff = (char*)malloc(RECVBUFF_SIZE*sizeof(char));
 		recv_buff = (char*)malloc(1*sizeof(char));//receiving on char at a time will make logic easier, but will likely impact performance
 		
 		if (recv_buff == NULL){
 			printf("Error when allocating new recv buffer block!\r\n");
 			return -1;
 		}
 		
 		int recv_buff_size = 1;
 		int recv_block_bytes = 0;
 		int newline_recv    = 0; 
 		
 		while(!newline_recv){
 			recv_block_bytes = 0;
 			recv_block_bytes = recv(connection_fd,recv_buff+recv_buff_size-1,sizeof(recv_buff), 0);
 			if (recv_block_bytes == 0){
 				printf("Connection closed or 0 bytes received\r\n");
 				newline_recv = 1;
 			}
 			else if (recv_block_bytes == -1){
 				printf("Error in socket recv\r\n");
 				newline_recv = 1;
 				return -1;
 			}
 			
 			//Check if new byte is a newline
 			if (recv_buff[recv_buff_size - 1] == '\n'){
 				newline_recv = 1;
 			}
 			else{
 				//add new byte to buffer to be read in at the next loop execution
 				recv_buff_size ++;
 				recv_buff = realloc(recv_buff, (recv_buff_size)*sizeof(char)); //allocate
 			}
 			
 		}

	}
 	
 	
 	
}	
