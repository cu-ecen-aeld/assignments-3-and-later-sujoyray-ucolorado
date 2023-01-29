/*****************************************************************************
* Copyright (C) 2023 by Sujoy Ray
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
* @brief This program write a pattern to a file. The program assumes that the 
*        directory is created by the caller. 
*        
*
* @author Sujoy Ray
* @date January 25, 2023
* @version 1.0
* CREDIT: Header credit: University of Colorado coding standard
* 
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>

/*******************************************************************************
 * Definitions
*******************************************************************************/
#define MAX_ARG_CNT     (3)

/*******************************************************************************
 * Prototypes
*******************************************************************************/
int writer(char *filename, char *string);

/*******************************************************************************
 * Variables and Macros
*******************************************************************************/



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
int main (int argc, char *argv[]) { 

    int rc = 0;

    /* Open log and set log level */
    openlog ("writer", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
    setlogmask (LOG_UPTO (LOG_DEBUG));

    if (argc < MAX_ARG_CNT) {
        printf("Writer: Error: Missing arguments \n\r");
        printf("\tusage: writer <file path> <string> \n\r");
        syslog (LOG_ERR, "Not all required arguments are present");
        syslog (LOG_ERR, "usage: writer <file path> <string>");
        closelog ();
        exit (1);
    }

    /* Call writer function */
    rc = writer(argv[1], argv[2]);
    closelog();
    return rc;

}

/*
* @brief  Write data into the file.
*
* This function writes data into the file. If the file exists, this function 
* will overwrite the data.
*
* Parameters:
*   filename: Filename along with path
*   string    Pointer to string
*
* Returns: 0 if the function executed without any error. Otherwise, error code 
*          is logged in syslog and the program exists with code 1
*   
*/
int writer(char *filename, char *string)
{
    FILE  *fp;
    int rc = 0;

    syslog(LOG_DEBUG, "Writing %s to file %s ", string, filename);

    fp = fopen(filename, "w");    
    if( fp == NULL)
    {
        syslog(LOG_ERR, "File Open error, file name: %s: %s", filename, 
            strerror(errno));        
        closelog();
        exit(1);
    }

    rc = fprintf(fp, "%s", string);
    if ( rc < 0 ) {
        
        syslog(LOG_ERR, "File Writer Error, file name: %s: %s", filename, 
            strerror(errno)); 
        fclose(fp);
        closelog();
        exit(1);
    }

    rc = fflush(fp);
    if ( rc < 0 ) {   
        syslog(LOG_ERR, "File Writer Error to disk, file name: %s: %s", filename, 
            strerror(errno)); 
        fclose(fp);
        closelog();
        exit(1);
    }

    fclose(fp);    
    return rc;
}

