/*
* File: writer.c 
* Purpose: Source file for application that writes a string to a file at the system level.
* Author: Malcolm McKellips
* Notes:  Assumes that directory structure of file to write has already been created by the caller.
*
*/



#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[]){
	
	if (argc < 2){
		printf("Error, no args provided. Please retry with the following 2 args: [path to file] [string to write]\r\n");
		syslog((LOG_USER | LOG_ERR),"Error, no args provided. Please retry with the following 2 args: [path to file] [string to write]");
		return 1;
	}
	
	if (argc == 2){
		printf("Only first argument (path of file to write) specified. Please also specify second argument (string to write)\r\n");
		syslog((LOG_USER | LOG_ERR),"Only first argument (path of file to write) specified. Please also specify second argument (string to write)");
		return 1;	
	}
	
	int fd = -1; //init to -1 same as err so will be caught if not changed
	char *writefile = argv[1];
	char *writestr = argv[2];
	int flags = 0;
	int permissions = 0;

	flags = (O_CREAT | O_WRONLY | O_TRUNC); //open file for writing, create if doesn't exist...
	permissions = (S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);
	
	fd = open (writefile,flags,permissions); 
	if (fd == -1){
		printf("Error, issue opening file %s. Ensure its parent directory exists and it has correct permissions.\r\n", writefile);
		syslog((LOG_USER | LOG_ERR),"Error, issue opening file %s. Ensure its parent directory exists and it has correct permissions.", writefile);
		return 1; //TODO: might not want a return here.
	}
	
	
	printf("Writing %s to %s\r\n", writestr, writefile);
	syslog((LOG_USER | LOG_DEBUG), "Writing %s to %s", writestr, writefile);
		
	ssize_t nr = -1;
	nr = write(fd, writestr,strlen(writestr));
	
	if (nr == -1){
		printf("Error, issue writing file %s with content %s.\r\n", writefile, writestr);
		syslog((LOG_USER | LOG_ERR),"Error, issue writing file %s with content %s.", 		writefile, writestr);
		return 1; //TODO: might not want a return here.
	}

	return 0;
}
