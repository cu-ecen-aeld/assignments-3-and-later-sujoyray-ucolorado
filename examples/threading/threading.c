#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "threading.h"

#define MS_VAL_TO_US(x)     ((x) * 1000)
#define DEBUG_LOG_ENABLE  0

// Optional: use these functions to add debug or error prints to your application
#if (DEBUG_LOG_ENABLE == 1)
#define DEBUG_LOG(msg,...) printf("threading Info: " msg "\n" , ##__VA_ARGS__)
#else
#define DEBUG_LOG(msg,...)
#endif

#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    int ret_code = -1;

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    if (thread_param == NULL) {
        ERROR_LOG("thread_param struct is NULL");
        goto exit;
    }

    struct thread_data *th_param =  (struct thread_data *)thread_param;
    th_param->thread = pthread_self();

    ret_code = usleep(MS_VAL_TO_US(th_param->wait_to_obtain_ms));
    if (ret_code != 0) {
        th_param->extended_thread_err = errno;
        ERROR_LOG("usleep error: %s", strerror(errno));
        th_param->thread_complete_success = false;
        goto exit;
    }

    ret_code = pthread_mutex_lock(th_param->mutex);
    if (ret_code != 0) {
        th_param->extended_thread_err = ret_code;
        ERROR_LOG("mutex_lock error: %s",strerror(ret_code));        
        th_param->thread_complete_success = false;        
        goto exit;
    }

    ret_code = usleep(MS_VAL_TO_US(th_param->wait_to_release_ms));
    if (ret_code != 0) {
        th_param->extended_thread_err = errno;
        ERROR_LOG("usleep error: %s", strerror(errno));
        th_param->thread_complete_success = false;
        goto exit;
    }

    ret_code = pthread_mutex_unlock(th_param->mutex);
    
    if (ret_code != 0) {
        th_param->extended_thread_err = ret_code;
        ERROR_LOG("mutex_unlock error: %s",strerror(ret_code));        
        th_param->thread_complete_success = false;        
        goto exit;
    }
    
    th_param->thread_complete_success = true;
    
exit:
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    int ret_code = -1;
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    DEBUG_LOG("Starting start_thread_obtaining_mutex function...");
    struct thread_data *th_data = (struct thread_data *)malloc(sizeof(struct thread_data)); 
    if (th_data == NULL) {
        ERROR_LOG("Memory allocation error");
        return false;
    }

    th_data->wait_to_obtain_ms  = wait_to_obtain_ms; 
    th_data->wait_to_release_ms = wait_to_release_ms;
    th_data->mutex = mutex;
    th_data->thread = 0;
    th_data->thread_complete_success = 0;

    DEBUG_LOG("Creating threadfunc ...");
    ret_code = pthread_create(thread, NULL, threadfunc, th_data);
    if( ret_code != 0) {
        ERROR_LOG("Unable to create error, error_code %d", ret_code);
        return false;
    }
    DEBUG_LOG("Created threadfunc successfully ...");
    return true;
}

