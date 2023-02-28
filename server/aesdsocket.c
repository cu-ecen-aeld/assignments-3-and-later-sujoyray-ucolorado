
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
* @file aesdsocket.c
* @brief This file has aesdsocket socket implementations
*
* @author Sujoy Ray 
* @date February 25, 2023
* @version 1.0
* CREDIT: Header credit: University of Colorado coding standard
* CREDIT: Inspired by https://www.geeksforgeeks.org/socket-programming-cc
* CREDIT: Consulted stackoverflow 
*
*/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>



/*******************************************************************************
 * Definitions
*******************************************************************************/
#define SOCKET_PORT                 (9000)
#define MAX_SERVER_CONNECTION       (1)
#define TRUE                        (1)
#define FALSE                       (0)
#define PACKET_TIMEOUT_END          (10)
#define MSEC_2_USEC(x)              ((x) * 1000)
#define FIXED_RD_BUF_SIZE           (1024)

/*******************************************************************************
 * Prototypes
*******************************************************************************/
static int  process_and_save_data(char *buffer, char *file_buffer, 
    int rcv_data_len, int *wr_pointer, FILE *fp);

static void aesdsoc_sighandler(int signal_no);


static int aesdsocket_server(int d_mode); 


/*******************************************************************************
 * Variables and Macros
*******************************************************************************/
static volatile sig_atomic_t exit_aesd_soc = FALSE;
static volatile sig_atomic_t file_close = TRUE;
static volatile sig_atomic_t soc_close = FALSE;

/*******************************************************************************
 * Code
*******************************************************************************/


/*
* Application entry point
* 
* Parameters:
*   argc: Argument count
*   argv: Argument vector
*
* Returns: 0 if the function executed without any error. Otherwise, error code 
*          is logged in syslog and the program exists with code 1
*   
*/
int main(int argc, char **argv)
{
    int d_mode = 0;
    int rc = -1;
    /* Open log and set log level */
    openlog ("aesdsocket", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
    setlogmask (LOG_UPTO (LOG_DEBUG));

    syslog(LOG_INFO,"**** Starting AESDSOCKET application ****");

    if(argc > 1) {
        if (strcmp(argv[1],"-d") !=0) {
            syslog(LOG_INFO,"aesdsocket:-d is NOT detected \n");
            d_mode = 0;
        }
        else {     
            d_mode = 1;
            syslog(LOG_INFO,"aesdsocket: -d is detected \n");
        }
    }
    rc = aesdsocket_server(d_mode);
    closelog();
    return rc;
    
}

/*
* Socket server API
* 
* Parameters:
*   d_mode: Flag stating if daemon mode is enabled or not, 1= daemon mode
*
* Returns: 0 if the function executed without any error. Otherwise, error code 
*          is logged in syslog and the program exists with code non zero value
*   
*/
static int aesdsocket_server(int d_mode) {
    int soc_server = -1;
    int soc_client = -1;
    int rc = -1;
    int cmd_option = 1;
    struct sockaddr_in aesdsoc_addr;
    int aesdsoc_addr_len = sizeof(struct sockaddr_in);
    int rcv_data_len = 0;
    int wr_pointer = 0;
    int buffer_size = FIXED_RD_BUF_SIZE;
    FILE *fp;
    struct sigaction signal_action;
    char *buffer = NULL;
    char *file_buffer = NULL;
    char *tx_buf = NULL;
    struct timespec start;
    struct timespec now;
    int diff_time = 0;


    if(d_mode == TRUE) {
        rc = daemon(0,0);
        if (rc < 0) {
            syslog(LOG_ERR, "aesdsocket: daemon creation error %s", strerror(errno));
            goto  return_func;
        }
    }

    fp =  fopen("/var/tmp/aesdsocketdata", "a+");
    if (fp == NULL) {
        syslog(LOG_ERR, "aesdsocket: File open Error %s", strerror(errno));
        goto  return_func;
    }

    file_close = TRUE;

    syslog(LOG_INFO, "aesdsocket:Starting aesdsocket_server in daemon = %s", 
        ((d_mode ==1)? "TRUE" : "FALSE")); 

    memset(&signal_action, 0, sizeof(signal_action));
    signal_action.sa_handler = aesdsoc_sighandler;
    rc = sigaction(SIGINT,  &signal_action, 0);
    if (rc < 0 ) {
        syslog(LOG_ERR, "aesdsocket: Sigaction failed for SIGINT %s", strerror(errno));
        goto error_1;
    }
    rc = sigaction(SIGTERM, &signal_action, 0);
    if (rc < 0 ) {
        syslog(LOG_ERR, "aesdsocket: Sigaction failed for SIGTERM %s", strerror(errno));
        goto error_1;
    }

    soc_server = socket(AF_INET, SOCK_STREAM, 0);
    if (soc_server < 0) {
        rc = -1;
        syslog(LOG_ERR, "aesdsocket: Socket creation failed %s", strerror(errno));
        goto error_1;
    }

    rc = setsockopt(soc_server, SOL_SOCKET, SO_REUSEPORT, &cmd_option, sizeof(cmd_option));
    if (rc < 0) {
        syslog(LOG_ERR, "aesdsocket: API setsockopt failed %s", strerror(errno));
        goto exit_shutdown;
    }

    aesdsoc_addr.sin_family = AF_INET;
    aesdsoc_addr.sin_addr.s_addr = INADDR_ANY;
    aesdsoc_addr.sin_port = htons(SOCKET_PORT);

    rc = bind(soc_server, (struct sockaddr*)&aesdsoc_addr, aesdsoc_addr_len);
    if (rc < 0) {
        syslog(LOG_ERR, "aesdsocket: API bind failure %s", strerror(errno));
        goto exit_shutdown;
    }

    while( exit_aesd_soc == FALSE) {
        rc = listen(soc_server, MAX_SERVER_CONNECTION);
        if (rc < 0) {
            syslog(LOG_ERR, "aesdsocket: API listen failed %s", strerror(errno));
            goto exit_shutdown;
        }

        soc_client = accept(soc_server, (struct sockaddr*)&aesdsoc_addr, 
            (socklen_t*)&aesdsoc_addr_len);
        if (soc_client < 0 ) {
            rc = -1;
            syslog(LOG_ERR, "aesdsocket: API accept failed %s", strerror(errno));
            goto exit_shutdown;
        }

        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(aesdsoc_addr.sin_addr)); 

                
        rc = fcntl(soc_client, F_SETFL, O_NONBLOCK);
        if (rc < 0) {
            syslog(LOG_ERR, "aesdsocket: fcntl failed %s", strerror(errno));
            goto exit_client;
        }
        rc = clock_gettime(CLOCK_MONOTONIC, &start);
        if (rc < 0) {
            syslog(LOG_ERR, "aesdsocket: clock_gettime failed %s", strerror(errno));
            goto exit_client;
        }

        int total_bytes_received = 0;
        buffer = (char*)malloc(buffer_size);
        if(buffer == NULL) {    
            rc = -1;
            syslog(LOG_ERR, "aesdsocket: malloc failed %s", strerror(errno));
            goto exit_client;
        }
        while(1) {
            rcv_data_len = recv(soc_client, buffer + total_bytes_received, buffer_size - total_bytes_received, 0);
            if (rcv_data_len < 0) {
                rc = clock_gettime(CLOCK_MONOTONIC, &now);
                if (rc < 0) {            
                    syslog(LOG_ERR, "aesdsocket: clock_gettime failed %s", strerror(errno));
                    goto goto_mem_cleanup;
                }
                diff_time = ((now.tv_sec - start.tv_sec) * 1000000) + ((now.tv_nsec - start.tv_nsec) / 1000); 
                if(diff_time > MSEC_2_USEC(PACKET_TIMEOUT_END)) {   
                    file_buffer = (char*)malloc(total_bytes_received+1);
                    if(file_buffer == NULL) {
                        rc = -1;
                        if(buffer) free(buffer);
                        syslog(LOG_ERR, "aesdsocket: malloc failed %s", strerror(errno));
                        goto goto_mem_cleanup;
                    }
                    if (process_and_save_data(buffer, file_buffer, 
                            total_bytes_received, &wr_pointer, fp) < 0) {
                        rc = -1;
                        syslog(LOG_ERR, "aesdsocket: process_and_save_data");
                        goto goto_mem_cleanup;
                    }
                    int file_len = ftell(fp);
                    /* 
                    ** Timeout reached and if not data is saved, discard the packet.
                    ** If file consists of data, send /replay the buffer and then wait for 
                    ** new connection.
                    */
                    if (file_len == 0) {
                        syslog(LOG_ERR, "aesdsocket: No data saved, after 100 ms timeout rejecting packet");
                        goto discard_packet;
                    }
                    tx_buf = (char *) malloc(file_len);
                    if (tx_buf == NULL) {
                        rc = -1;
                        syslog(LOG_ERR, "aesdsocket: malloc failed %s", strerror(errno));
                        goto goto_mem_cleanup;
                    }
                    if(fseek(fp, 0, SEEK_SET)) {  
                        rc = -1;
                        syslog(LOG_ERR, "aesdsocket: FSEEK failed %s", strerror(errno));
                        goto goto_mem_cleanup;
                    }
                    while(fgets(tx_buf, file_len, fp) != NULL)  {
                        if(send(soc_client, tx_buf, strlen(tx_buf), 0)== -1)  {
                            rc = -1;
                            syslog(LOG_ERR, "aesdsocket: SEND failed %s", strerror(errno)); 
                            goto goto_mem_cleanup; 
                        }
                    }
                    
                    if(fseek(fp, 0, SEEK_END)) {
                        rc = -1;
                        syslog(LOG_ERR, "aesdsocket: fseek failed %s", strerror(errno)); 
                        goto goto_mem_cleanup; 
                    }
                    discard_packet :
                    if(buffer) { free(buffer); buffer = NULL;}
                    if(file_buffer) { free(file_buffer); file_buffer = NULL;}
                    if(tx_buf) { free(tx_buf); tx_buf = NULL;}
                    total_bytes_received = 0;
                    buffer_size =  FIXED_RD_BUF_SIZE;
                    wr_pointer = 0;
                    close(soc_client);
                    break;
                }
            }
            else if (rcv_data_len == 0) {
                if(buffer) { free(buffer); buffer = NULL;}
                if(file_buffer) { free(file_buffer); file_buffer = NULL;}
                if(tx_buf) { free(tx_buf); tx_buf = NULL;} 
                fclose(fp);
                file_close = FALSE;
                close(soc_client);
                syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(aesdsoc_addr.sin_addr)); 
                break;
            }
            else {
                rc = clock_gettime(CLOCK_MONOTONIC, &start);
                if (rc < 0) {            
                    syslog(LOG_ERR, "aesdsocket: clock_gettime failed %s", strerror(errno));
                    goto goto_mem_cleanup;
                }
                total_bytes_received += rcv_data_len;
                buffer_size *= 2;
                buffer = (char*)realloc(buffer, buffer_size);
                if (buffer == NULL) {                    
                    syslog(LOG_ERR, "aesdsocket: realloc failed %s", strerror(errno));
                    goto goto_mem_cleanup;
                }
            }
        }

    }
    goto_mem_cleanup:
    if(buffer) { free(buffer); buffer = NULL;}
    if(file_buffer) { free(file_buffer); file_buffer = NULL;}
    if(tx_buf) { free(tx_buf); tx_buf = NULL;}

    goto exit_shutdown;
    exit_client:
    if(soc_close) close(soc_client);
    exit_shutdown:
    shutdown(soc_server, SHUT_RDWR);
    error_1:
    if(file_close) {
        fclose(fp);
    }
    if(remove("/var/tmp/aesdsocketdata")) {
        printf("Error file removal\n");
    }
    return_func:
    rc = (exit_aesd_soc == TRUE) ? 0 : rc;
    return rc;
}

/*
* process_and_save_data
* 
* Parameters:
*   buffer:         Pointer to data buffer containing client data. 
*                   Freed by the caller.
*   file_buffer:    Pointer to file buffer, freed by the caller.
*   rcv_data_len:   Size of data buffer in bytes
*   write_ptr:      Index of file buffer
*   fp:             File pointer
*
* Returns: None
        0 : For sucsess, 
        < 0 for error
*/
static int process_and_save_data(char *buffer, char *file_buffer, 
    int rcv_data_len, int *wr_pointer, FILE *fp) {

    int buf_rd_ptr = 0;    
    int pos = 0;
    int rc = 0;
    char *start = &buffer[buf_rd_ptr];
    char *end = &buffer[buf_rd_ptr];
    int len_to_copy = rcv_data_len;  
    end  = strstr(buffer, "\n");
    while(1) {
        if(end == NULL) {
            memcpy(&file_buffer[*wr_pointer], &buffer[buf_rd_ptr], len_to_copy);
            *wr_pointer += len_to_copy;
            buf_rd_ptr = 0;
            break;
        }
        else {
            pos = (unsigned int)(end - start+1);
            memcpy(&file_buffer[*wr_pointer], &buffer[buf_rd_ptr], pos);  
            *wr_pointer += pos;
            file_buffer[*wr_pointer] ='\n';            
            rc = fwrite (file_buffer , sizeof(char), *wr_pointer, fp);
            if (rc < 0) {
                syslog(LOG_ERR, "aesdsocket: fwrite failed %s", strerror(errno));
                return rc;
            } 
            fflush(fp);
            *wr_pointer = 0;
            len_to_copy = len_to_copy - pos;
            buf_rd_ptr += pos;
            if(len_to_copy == 0) break;
        }
        start = end+1;
        end  = strstr(start, "\n");
    }
    return 0;
}

/*
* aesdsoc_sighandler
* 
* Parameters:
*   signal_no:      Signal number. 
*
* Returns: None
*/
static void aesdsoc_sighandler(int signal_no) {

    syslog(LOG_INFO, "Caught signal, exiting"); 
    if (signal_no == SIGINT || signal_no == SIGTERM) {
       exit_aesd_soc = TRUE;
       soc_close = TRUE;
    }
    if (signal_no == SIGINT )
       file_close = FALSE;     
}

