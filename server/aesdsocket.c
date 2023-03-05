
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
#include "queue.h"
#include <pthread.h>



/*******************************************************************************
 * Definitions
*******************************************************************************/
#define SOCKET_PORT                 (9000)
#define MAX_SERVER_CONNECTION       (10)
#define TRUE                        (1)
#define FALSE                       (0)
#define PACKET_TIMEOUT_END          (1)
#define MSEC_2_USEC(x)              ((x) * 1000)
#define FIXED_RD_BUF_SIZE           (1024)
#define MAX_TIME_ENTRY              (100)

/*******************************************************************************
 * Prototypes
*******************************************************************************/
static int  process_and_save_data(char *buffer, char *file_buffer, 
    int rcv_data_len, int *wr_pointer, FILE *fp);
static void aesdsoc_sighandler(int signal_no);
static int aesdsocket_server(int d_mode); 
static void *soc_thread(void *argument);
static void asesd_soc_timer_handler(int signum);
static void asesd_soc_timer_init(void);

typedef struct ASEDSocThread {    
    pthread_t thread;
    int soc_client;
    int done;
    struct sockaddr_in aesdsoc_addr;
    SLIST_ENTRY(ASEDSocThread) entries;
} ASEDSocThread_t;


/*******************************************************************************
 * Variables and Macros
*******************************************************************************/
static volatile sig_atomic_t exit_aesd_soc = FALSE;
static volatile sig_atomic_t file_close = TRUE;
static volatile sig_atomic_t soc_close = FALSE;
static volatile sig_atomic_t mutex_close = FALSE;
static volatile sig_atomic_t write_time = 0;


SLIST_HEAD(thread_head, ASEDSocThread) head = SLIST_HEAD_INITIALIZER(head);
static pthread_mutex_t link_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
static timer_t timerid;
static time_t time_value[MAX_TIME_ENTRY];
FILE *fp;



/*******************************************************************************
 * Code
*******************************************************************************/

void asesd_soc_write_time_stamp(void) {
    syslog(LOG_INFO,"**** Hello timer handler ****");
    time_t now;
    struct tm* cur_time;
    char time_stamp[100];

    for (int i = 0; i < write_time; i++ ) {
        now = time_value[i];
        cur_time = localtime(&now);
        strftime(time_stamp, sizeof(time_stamp), "%Y-%m-%d %H:%M:%S", cur_time);
        fprintf(fp, "timestamp: %s\n", time_stamp);
        fflush(fp);    
        syslog(LOG_INFO,"**** Wrote TS AESDSOCKET timer handler ****");
     }
     write_time = 0;
}


void asesd_soc_timer_handler(int signum) {
    time_t now;
    
    syslog(LOG_INFO,"**** AESDSOCKET timer handler ****");
    time_value[write_time++] = time(&now);
    if (( write_time % MAX_TIME_ENTRY) == 0) {
        write_time = 1;
    }
 
}


void asesd_soc_timer_init(void)
{
    struct sigevent aesd_soc_tmr_event;
    struct itimerspec aesd_soc_tmr_timer_spec;

    aesd_soc_tmr_event.sigev_signo = SIGALRM;
    aesd_soc_tmr_event.sigev_value.sival_ptr = &timerid;
    aesd_soc_tmr_event.sigev_notify = SIGEV_SIGNAL;


    if (timer_create(CLOCK_REALTIME, &aesd_soc_tmr_event, &timerid) != 0) {
        syslog(LOG_INFO,"**** Unable to create timer ****");
        //Todo: handler error
        return;
    }


    aesd_soc_tmr_timer_spec.it_value.tv_sec = 10;
    aesd_soc_tmr_timer_spec.it_value.tv_nsec = 0;
    aesd_soc_tmr_timer_spec.it_interval.tv_sec = 10; 
    aesd_soc_tmr_timer_spec.it_interval.tv_nsec = 0; 


    if (timer_settime(timerid, 0, &aesd_soc_tmr_timer_spec, NULL) != 0) {
        syslog(LOG_INFO,"**** Unable to set the timer ****");
        //Todo: handler error
        return;
    }

    // Set up the signal handler for the timer
    signal(SIGALRM, asesd_soc_timer_handler);

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
    if (signal_no == SIGINT || signal_no == SIGTERM ) {
       exit_aesd_soc = TRUE;
       soc_close = TRUE;
    }
    if (signal_no == SIGINT )
       file_close = FALSE;    
}


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
*          is logged in syslog and the program exists with non-zero value
*   
*/
static int aesdsocket_server(int d_mode) {
    int soc_server = -1;
    int rc = -1;
    int cmd_option = 1;
    int aesdsoc_addr_len = sizeof(struct sockaddr_in);

    struct sigaction signal_action;
    
    int soc_client = -1;
    struct sockaddr_in aesdsoc_addr;


    soc_server = socket(AF_INET, SOCK_STREAM, 0);
    if (soc_server < 0) {
        rc = -1;
        syslog(LOG_ERR, "aesdsocket: Socket creation failed %s", strerror(errno));
        goto error_1;
    }

    syslog(LOG_INFO,"**** AESDSOCKET application: setsockopt ****");

    rc = setsockopt(soc_server, SOL_SOCKET, SO_REUSEPORT, &cmd_option, sizeof(cmd_option));
    if (rc < 0) {
        syslog(LOG_ERR, "aesdsocket: API setsockopt failed %s", strerror(errno));
        goto exit_shutdown;
    }

    aesdsoc_addr.sin_family = AF_INET;
    aesdsoc_addr.sin_addr.s_addr = INADDR_ANY;
    aesdsoc_addr.sin_port = htons(SOCKET_PORT);

    syslog(LOG_INFO,"**** AESDSOCKET application: bind ****");

    rc = bind(soc_server, (struct sockaddr*)&aesdsoc_addr, aesdsoc_addr_len);
    if (rc < 0) {
        syslog(LOG_ERR, "aesdsocket: API bind failure %s", strerror(errno));
        goto exit_shutdown;
    }

    syslog(LOG_INFO,"**** AESDSOCKET application: listen ****");

    rc = listen(soc_server, MAX_SERVER_CONNECTION);
    if (rc < 0) {
        syslog(LOG_ERR, "aesdsocket: API listen failed %s", strerror(errno));
        goto exit_shutdown;
    }


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
    
    asesd_soc_timer_init();

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
    

    syslog(LOG_INFO,"**** AESDSOCKET application: socket ****");
    

    while( exit_aesd_soc == FALSE) {
                
        syslog(LOG_INFO,"**** AESDSOCKET application: accept ****");

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


        ASEDSocThread_t *thread_struct;
        pthread_t thread;

        thread_struct = (ASEDSocThread_t *)malloc(sizeof(ASEDSocThread_t));
        if(thread_struct == NULL) {
            syslog(LOG_ERR, "aesdsocket: Malloc failed");
            goto exit_client;
        }
            
        thread_struct->done = 0;
        thread_struct->aesdsoc_addr = aesdsoc_addr;
        thread_struct->soc_client = soc_client;
        pthread_mutex_lock(&link_list_mutex);
        SLIST_INSERT_HEAD(&head, thread_struct, entries);
        pthread_mutex_unlock(&link_list_mutex);

        if(write_time) asesd_soc_write_time_stamp();
        
        if (pthread_create(&thread, NULL, soc_thread, thread_struct) != 0) {
            perror("pthread_create failed");
            goto exit_client;
        }

        syslog(LOG_INFO, "Pthread created thread  %lu", thread);

        
        ASEDSocThread_t *temp;
        ASEDSocThread_t *ptr;
        pthread_mutex_lock(&link_list_mutex);            
        SLIST_FOREACH_SAFE(ptr, &head, entries, temp) {
            if (ptr->done == 1) {
                syslog(LOG_INFO, "Pthread join for %lu", ptr->thread);
                pthread_join(ptr->thread, NULL);
                SLIST_REMOVE(&head, ptr, ASEDSocThread, entries);
                free(ptr);
                continue;
            }
        }
        pthread_mutex_unlock(&link_list_mutex);
        if (SLIST_EMPTY(&head)) {
            //break;
        }
       
    }

    //Todo need better strategy for now let's hack it
    ASEDSocThread_t *temp;
    ASEDSocThread_t *ptr;
    //while (1) {
        pthread_mutex_lock(&link_list_mutex);
        SLIST_FOREACH_SAFE(ptr, &head, entries, temp) {
            if (ptr->done == 1) {
                syslog(LOG_INFO, "Pthread join for %lu", ptr->thread);
                pthread_join(ptr->thread, NULL);
                SLIST_REMOVE(&head, ptr, ASEDSocThread, entries);
                free(ptr);
                continue;
            }
        }
        pthread_mutex_unlock(&link_list_mutex);
        if (SLIST_EMPTY(&head)) {
            //break;
        }
    //}

    goto exit_shutdown;
    exit_client:
    close(soc_client);
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
    timer_delete(timerid);
    rc = (exit_aesd_soc == TRUE) ? 0 : rc;
    return rc;
}

int prev_len = 0;

int sendpacket(int soc_client)
{
    char *tx_buf = NULL;
    int data_wrote = 0;

    pthread_mutex_lock(&file_mutex);

    
    int file_len = ftell(fp);
    data_wrote = file_len- prev_len; 
    syslog(LOG_INFO, "File info %d %d", file_len, prev_len);

    if((data_wrote) <=0) {
        
        syslog(LOG_INFO, "Nothing to send %d %d", file_len, prev_len);
        goto return_send;
    }
    prev_len = file_len;
    if(!file_len) goto return_send;
    
    tx_buf = (char *) malloc(file_len);
    syslog(LOG_INFO, "Sneding %d to client", file_len);
    if(fseek(fp, 0, SEEK_SET)) {  
        syslog(LOG_ERR, "aesdsocket: FSEEK failed %s", strerror(errno));
            }
    while(fgets(tx_buf, file_len, fp) != NULL)  {
        if(send(soc_client, tx_buf, strlen(tx_buf), 0)== -1)  {
            syslog(LOG_ERR, "aesdsocket: SEND failed %s", strerror(errno)); 
        }
    }
    if(fseek(fp, 0, SEEK_END)) {
        syslog(LOG_ERR, "aesdsocket: fseek failed %s", strerror(errno));  
    }    
    free(tx_buf);
 return_send: 
    pthread_mutex_unlock(&file_mutex);
    return data_wrote;
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
    end  = memchr(buffer, '\n', rcv_data_len ); 
    syslog(LOG_INFO, "aesdsocket: process_and_save_data: RCV_len = %d, wr_pointer = %d", rcv_data_len, *wr_pointer);
    while(1) {
        if(end == NULL) {
            memcpy(&file_buffer[*wr_pointer], &buffer[buf_rd_ptr], len_to_copy);
            *wr_pointer += len_to_copy;
            syslog(LOG_INFO, "aesdsocket: process_and_save_data: saving %d in memory, wr= =%d", len_to_copy, *wr_pointer);
            buf_rd_ptr = 0;
            break;
        }
        else {
            pos = (unsigned int)(end - start+1);
            memcpy(&file_buffer[*wr_pointer], &buffer[buf_rd_ptr], pos); 
            *wr_pointer += pos;
            file_buffer[*wr_pointer] ='\n';            
            syslog(LOG_INFO, "aesdsocket: process_and_save_data: saving %d in file", *wr_pointer);
            pthread_mutex_lock(&file_mutex);            
            rc = fwrite (file_buffer , sizeof(char), *wr_pointer, fp);
            if (rc < 0) {
                syslog(LOG_ERR, "aesdsocket: fwrite failed %s", strerror(errno));                
                pthread_mutex_unlock(&file_mutex);
                return rc;
            } 
            fflush(fp);   
            pthread_mutex_unlock(&file_mutex);
            *wr_pointer = 0;
            len_to_copy = len_to_copy - pos;
            buf_rd_ptr += pos;
            syslog(LOG_INFO, "aesdsocket: process_and_save_data: len= %d", len_to_copy);

            if(len_to_copy == 0) break;
        }
        start = end+1;
        end = memchr(start, '\n', len_to_copy );
    }
    return 0;
}

/*
* Soceket thread
* 
* Parameters:
*   argument:       Thread argument. 
*                   Freed by the caller.
*   
* Returns: Sucess or error through thread data structure
*/
static void *soc_thread(void *argument) {
    char *buffer = NULL;
    char *file_buffer = NULL;
    char *tx_buf = NULL;
    int rcv_data_len = 0;
    int wr_pointer = 0;
    int buffer_size = FIXED_RD_BUF_SIZE;
    
    ASEDSocThread_t *th_data = (ASEDSocThread_t *)argument;

    struct sockaddr_in aesdsoc_addr = th_data->aesdsoc_addr;
    int soc_client = th_data->soc_client;
    th_data->thread = pthread_self();
    
    syslog(LOG_INFO, "aesdsocket: thread started");

    while( exit_aesd_soc == FALSE) {
        
        syslog(LOG_INFO, "aesdsocket: allocating buffer");
        
        int byte_allocated = buffer_size;
        file_buffer = (char*)malloc(sizeof(char)*(buffer_size+100)); 
        buffer = (char*)malloc(sizeof(char)* buffer_size);
        if(buffer == NULL) {    
            syslog(LOG_ERR, "aesdsocket: malloc failed %s", strerror(errno));
            goto exit_client;
        }
        while(exit_aesd_soc == FALSE) {  
            rcv_data_len = recv(soc_client, buffer, buffer_size, 0);
            syslog(LOG_INFO, "aesdsocket: receive %d %d %d", soc_client, buffer_size, rcv_data_len);
            if (rcv_data_len < 0) {  
                //handle error Todo
                continue;
            }
      
            else if (rcv_data_len == 0) {
                if(buffer) { free(buffer); buffer = NULL;}
                if(file_buffer) { free(file_buffer); file_buffer = NULL;}
                if(tx_buf) { free(tx_buf); tx_buf = NULL;} 
                close(soc_client);
                syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(aesdsoc_addr.sin_addr));
                goto exit_client;
            }
            else {
                if (process_and_save_data(buffer, file_buffer, 
                    rcv_data_len, &wr_pointer, fp) < 0) {
                    syslog(LOG_ERR, "aesdsocket: process_and_save_data return error");
                    goto goto_mem_cleanup;
                }
                
                int len = sendpacket(soc_client);
                 if (len > 0) {
                    if(buffer) { free(buffer); buffer = NULL;}
                    if(file_buffer) { free(file_buffer); file_buffer = NULL;}
                    if(tx_buf) { free(tx_buf); tx_buf = NULL;}
                    buffer_size =  FIXED_RD_BUF_SIZE;
                    wr_pointer = 0;
                    close(soc_client);
                    goto exit_client;
                    break;         
                 }
                buffer_size *= 2;
                byte_allocated += buffer_size;
                syslog(LOG_INFO, "aesdsocket: file_buffer %d", byte_allocated+1);
                file_buffer = (char*)realloc(file_buffer, byte_allocated+1);
                if(file_buffer == NULL) {
                    syslog(LOG_ERR, "aesdsocket: realloc failed %s", strerror(errno));
                    goto goto_mem_cleanup;
                }
                syslog(LOG_INFO, "aesdsocket: buffer %d", buffer_size);
                buffer = (char*)realloc(buffer, buffer_size);
                if (buffer == NULL) {                    
                    syslog(LOG_ERR, "aesdsocket: realloc failed %s", strerror(errno));
                    goto goto_mem_cleanup;
                }
            }
        }
    }
    exit_client:

    goto_mem_cleanup:
    if(buffer) { free(buffer); buffer = NULL;}
    if(file_buffer) { free(file_buffer); file_buffer = NULL;}
    if(tx_buf) { free(tx_buf); tx_buf = NULL;}
   
    close(soc_client);
    pthread_mutex_lock(&link_list_mutex);
    th_data->done = 1;
    pthread_mutex_unlock(&link_list_mutex);
    pthread_exit(NULL);

}

