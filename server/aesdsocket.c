
/* Inspired by
https://www.geeksforgeeks.org/socket-programming-cc/
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





#define SOCKET_PORT                 (9000)
#define MAX_SERVER_CONNECTION       (1)
#define TRUE                        (1)
#define FALSE                       (0)
#define PACKET_TIMEOUT_END          (10)
#define MSEC_2_USEC(x)              ((x) * 1000)
#define FIXED_RD_BUF_SIZE           (1024)
volatile sig_atomic_t exit_aesd_soc = FALSE;
volatile sig_atomic_t file_close = TRUE;

void process_and_save_data(char *buffer, char *file_buffer, int value_read, int *wr_pointer, FILE *fp) {
    int buf_rd_ptr = 0;    
    int pos = 0;    
    char *start = &buffer[buf_rd_ptr];
    char *end = &buffer[buf_rd_ptr];
    int len_to_copy = value_read;  
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
            fwrite (file_buffer , sizeof(char), *wr_pointer, fp);
            fflush(fp);
            *wr_pointer = 0;
            len_to_copy = len_to_copy - pos;
            buf_rd_ptr += pos;
            if(len_to_copy == 0)
            break;
        }
        start = end+1;
        end  = strstr(start, "\n");
    }

    printf("value_read = %d and len_to_copy =%d \n", value_read, len_to_copy);
}


void aesdsoc_sighandler(int signal_no) {
    if (signal_no == SIGINT || signal_no == SIGTERM) {
       exit_aesd_soc = TRUE;
       syslog(LOG_INFO, "Caught signal, exiting"); 
    }
    if (signal_no == SIGINT )
       file_close = FALSE;     

}
int main(int argc, char **argv)
{
    int soc_server = -1;
    int soc_client = -1;
    int rc = -1;
    int cmd_option = 1;
    struct sockaddr_in aesdsoc_addr;
    int aesdsoc_addr_len = sizeof(struct sockaddr_in);
    int value_read = 0;
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

    /* Open log and set log level */
    openlog ("aesdsocket", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
    setlogmask (LOG_UPTO (LOG_DEBUG));

    if(argc > 1)
    {
        if (strcmp(argv[1],"-d") !=0)
        {
            printf("%d %s \n",argc, argv[1]);
            printf("-d is NOT detected \n");
        }
        else
        {        
            printf("-d is detected \n");
            daemon(0,0);
        }
   }

   
   fp =  fopen("/var/tmp/aesdsocketdata", "a+");
   if (fp == NULL) {
           printf("Can't open file");
           exit (1);
   }

   file_close = TRUE;
   
   syslog(LOG_INFO, "Sujoy: Starting APP"); 

   memset(&signal_action, 0, sizeof(signal_action));
   signal_action.sa_handler = aesdsoc_sighandler;

   sigaction(SIGINT,  &signal_action, 0);
   sigaction(SIGTERM, &signal_action, 0);



    soc_server = socket(AF_INET, SOCK_STREAM, 0);
    if (soc_server < 0) {
        perror("aesdsocket: Socket creation failed");
        goto return_func;
    }

    rc = setsockopt(soc_server, SOL_SOCKET, SO_REUSEPORT, &cmd_option, sizeof(cmd_option));
    if (rc < 0) {
        perror("aesdsocket: API setsockopt failed");
        goto exit_shutdown;
    }

    aesdsoc_addr.sin_family = AF_INET;
    aesdsoc_addr.sin_addr.s_addr = INADDR_ANY;
    aesdsoc_addr.sin_port = htons(SOCKET_PORT);

    rc = bind(soc_server, (struct sockaddr*)&aesdsoc_addr, aesdsoc_addr_len);
    if (rc < 0) {
        perror("aesdsocket: API bind failure");
        goto exit_shutdown;
    }

    while( exit_aesd_soc == FALSE) {
        rc = listen(soc_server, MAX_SERVER_CONNECTION);
        if (rc < 0) {
            perror("aesdsocket: API listen failed");
            goto exit_shutdown;
        }

        soc_client = accept(soc_server, (struct sockaddr*)&aesdsoc_addr, 
            (socklen_t*)&aesdsoc_addr_len);
        if (soc_client < 0 ) {
            rc = -1;
            goto exit_shutdown;
        }

        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(aesdsoc_addr.sin_addr)); 

                
        fcntl(soc_client, F_SETFL, O_NONBLOCK);
        clock_gettime(CLOCK_MONOTONIC, &start);

        int total_bytes_received = 0;
        buffer = (char*)malloc(buffer_size);
        while(1) {
            value_read = recv(soc_client, buffer + total_bytes_received, buffer_size - total_bytes_received, 0);
            if (value_read < 0) {                
                clock_gettime(CLOCK_MONOTONIC, &now);
                diff_time = ((now.tv_sec - start.tv_sec) * 1000000) + ((now.tv_nsec - start.tv_nsec) / 1000);            
                if(diff_time > MSEC_2_USEC(PACKET_TIMEOUT_END)) {   
                    file_buffer = (char*)malloc(total_bytes_received+1);
                    process_and_save_data(buffer, file_buffer, total_bytes_received, &wr_pointer, fp);
                    int file_len = ftell(fp);
                    tx_buf = (char *)malloc(file_len);
                    if(fseek(fp, 0, SEEK_SET)) {
                        perror("FSEEK");
                    }
                    while(fgets(tx_buf, file_len, fp) != NULL)  {
                        if(send(soc_client, tx_buf, strlen(tx_buf), 0)== -1)  {
                            perror("Send");
                            exit(1);
                        }                        
                    }
                    free(tx_buf);
                    if(fseek(fp, 0, SEEK_END)) {
                        perror("FSEEK");
                    }
                    close(soc_client);                     
                    if(buffer)
                    {
                        free(buffer);
                        total_bytes_received = 0;
                        buffer_size =  FIXED_RD_BUF_SIZE;
                        wr_pointer = 0;                                            
                    }    
                    
                    if(buffer)
                        free(file_buffer);
                    break;
                }
            }
            else if (value_read == 0) {
                close(soc_client);
                if(buffer)
                {
                    
                    free(buffer);                    
                    total_bytes_received = 0;
                }
                if(file_buffer)
                {
                    free(file_buffer);
                }
                fclose(fp);
                file_close = FALSE;
                syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(aesdsoc_addr.sin_addr)); 
                break;
            }
            else {
                clock_gettime(CLOCK_MONOTONIC, &start);
                total_bytes_received += value_read;
                buffer_size *= 2;
                buffer = (char*)realloc(buffer, buffer_size);
            }
        }
    
   }

exit_shutdown:
    shutdown(soc_server, SHUT_RDWR);
return_func:
    if(file_close)
    {
        fclose(fp);
    }
    if(remove("/var/tmp/aesdsocketdata")) {
        printf("Error file removal\n");
    }
    closelog();
    return rc;

}

