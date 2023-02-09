/*****************************************************************************
* Copyright (C) 2023
*
* Redistribution, modification or use of this software in source or binary
* forms is permitted as long as the files maintain this copyright. Users are
* permitted to modify this and use it to learn about the field of embedded
* software. Sujoy Ray and the University of Colorado are not liable for
* any misuse of this material.
*
*****************************************************************************/
/**
* @file main.c
* @brief This program has various systemcall implementations
*
* @author Unknown, modified by Sujoy Ray for Assignment 3
* @date February 5, 2023
* @version 1.0
* CREDIT: Header credit: University of Colorado coding standard
* CREDIT: Inspired by https://linux.die.net/man/2/waitpid
* CREDIT: Consulted stackoverflow and LSP book for studying various
* implementations
*
*/
#include "systemcalls.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdint.h>
/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    int ret = 0;
    
/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    ret = system(cmd);
    if( ret != 0) {
        perror("ASssignment3:do_system:system:");
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
    pid_t process_id = -1;
    pid_t wait_id = -1;
    int32_t sys_status = -1;
    int rc = -1;
    
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
    process_id = fork();
    if (process_id == -1) {
        perror("Assignment3:do_exec:fork:");
        rc = -1;
        goto exit_prog;
    }
    /* Child process*/
    if (process_id == 0) {
        if(execv(command[0], command) == -1) {
            perror("Assignment3:do_exec:execv:");
            exit(1);
        }
    }
    else {
            wait_id = waitpid(process_id, &sys_status, 0);
            if (wait_id== -1) {
                perror("Assignment3:do_exec:waitpid:");
                rc = -1;
                goto exit_prog;
            }
            if(wait_id != process_id) {
                printf("Assignment3:do_exec:waitpid: ProcessID read is %d and exepected %d \n",
                    wait_id, process_id);
                rc = -1;
                goto exit_prog;
            }
            if (WIFEXITED (sys_status)) {
                if(WEXITSTATUS (sys_status)) {
                    printf("Assignment3:do_exec:waitpid: Child process error code: %d \n",
                        WEXITSTATUS(sys_status));
                    rc = -1;
                    goto exit_prog;
                }
                else {
                    /* Program ended properly*/
                    rc = 0;
                    goto exit_prog;
                }
            }
        }
    exit_prog:
    va_end(args);
    return (rc != -1);
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
    pid_t process_id = -1;
    pid_t wait_id = -1;
    int32_t sys_status = -1;
    int rc = -1;
    char * command[count+1];
    int i;
    for(i=0; i<count; i++) {
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
    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0) {
        perror("Assignment3:do_exec_redirect:open:");
        return false; 
    }
    process_id = fork();
    if (process_id == -1) {
        perror("Assignment3:do_exec_redirect:fork:");
        return false;
    }
    if (process_id == 0) {
        
        if (dup2(fd, 1) < 0) { 
            perror("Assignment3:do_exec_redirect:dup2:");
            exit(1); 
        }
        close(fd);
        if(execv(command[0], &command[0]) == -1)
        {
                perror("Assignment3:do_exec_redirect:execv:");
                exit(1);
        }
    }
    else {
        wait_id = waitpid(process_id, &sys_status, 0);
        if (wait_id== -1) {
            perror("Assignment3:do_exec:waitpid:");
            rc = -1;
            goto exit_prog;
        }
        if(wait_id != process_id) {
            printf("Assignment3:do_exec:waitpid: ProcessID read is %d and exepected %d \n",
                wait_id, process_id);
            rc = -1;
            goto exit_prog;
        }
        if (WIFEXITED (sys_status)) {
            if(WEXITSTATUS (sys_status)) {
                printf("Assignment3:do_exec:waitpid: Child process error code: %d \n",
                    WEXITSTATUS(sys_status));
                rc = -1;
                goto exit_prog;
            }
            else {
                /* Program ended properly*/
                rc = 0;
                goto exit_prog;
            }
        }
        close(fd);
    }
    exit_prog:
    va_end(args);
    return (rc != -1);
}
