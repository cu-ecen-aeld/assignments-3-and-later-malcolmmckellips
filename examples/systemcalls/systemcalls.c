#include "systemcalls.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#define _XOPEN_SOURCE
#include <stdlib.h>

#include <sys/stat.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    int sys_ret; 
    	
    sys_ret = system(cmd);
    if (sys_ret == -1){
    	printf("Error invoking %s command with system!\r\n",cmd);
    	return false;
    }
    	
    else
    	if (sys_ret != 0){
    		printf("Command %s invoked with system returned non-zero value\r\n",cmd);
    		return false;
    	}
    		
    
    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/ 
    
    //Reference: Linux System Programming page 161 
    pid_t pid; 
    int status; 
    
    pid = fork();
    if (pid == -1){
    	printf("Issue forking to create child process\r\n");
    	va_end(args);
    	return false;
    }
    
    else if (pid ==0){
    	//we are the child
    	execv(command[0],command);
    	
    	printf("Issue creating process with execv\r\n");
    	exit (-1); //should not return from execv if successful
    }
    
    //we're parent if we made it here
    if (waitpid (pid, &status, 0) == -1){
    	printf("Error while waiting for child process\r\n");
    	va_end(args);
    	return false;
    }

    else if (WIFEXITED (status)){
    	if (WEXITSTATUS(status) != 0){
    		printf("Child process terminated with non-zero exit code!\r\n");
    		va_end(args);
    		return false;
    	}
    }	
    
    va_end(args);
    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    int out_fd = open(outputfile, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (out_fd == -1){
    	printf("Error, issue opening output file!\r\n");
    	va_end(args);
    	return false;
    }
    	
    int pid; 
    int status; 
    pid = fork();
    
    if (pid == -1){
    	printf("Issue forking to create child process\r\n");
    	va_end(args);
    	return false;
    }
    
    else if (pid == 0){
    	//we are the child
    	
    	int dup_ret = 0; 
    	
    	dup_ret = dup2(out_fd, 1); //make file descriptor of our out_file mimic that of stdout (fd=1)
    	if (dup_ret == -1){
    		printf("Error redirecting stdout\r\n");
    		va_end(args);
    		return false;
    	}
    	
    	
    	execv(command[0],command);
    	
    	printf("Issue creating process with execv\r\n");
    	exit (-1); //should not return from execv if successful
    }
    
    if (waitpid (pid, &status, 0) == -1){
    	printf("Error while waiting for child process\r\n");
    	va_end(args);
    	return false;
    }
    
    
    //we're parent if we made it here
    close(out_fd);
    if (waitpid (pid, &status, 0) == -1){
    	printf("Error while waiting for child process\r\n");
    	va_end(args);
    	return false;
    }

    else if (WIFEXITED (status)){
    	if (WEXITSTATUS(status) != 0){
    		printf("Child process terminated with non-zero exit code!\r\n");
    		va_end(args);
    		return false;
    	}
    }	
    
    va_end(args);
    return true;
}
