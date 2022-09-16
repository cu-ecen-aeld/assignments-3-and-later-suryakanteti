#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
//#define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    
    // Cast into struct
    struct thread_data* thread_func_args = (struct thread_data *) (thread_param);
    
    // Wait to obtain mutex
    int rc = usleep(1000 * (thread_func_args->wait_to_obtain_ms));
    if(rc == -1)
    {
    	ERROR_LOG("Error while waiting before obtaining mutex");
    	thread_func_args->thread_complete_success = false;
    }
    else
    {
    	// Wait successful, obtain mutex
    	rc = pthread_mutex_lock(thread_func_args->mutex);
    	if(rc != 0)
    	{
    		ERROR_LOG("Error obtaining mutex lock. rc = %d",rc);
    		thread_func_args->thread_complete_success = false;
    	}
    	else
    	{
    		// Mutex obtained, wait to release
    		rc = usleep(1000 * (thread_func_args->wait_to_release_ms));
    		if(rc == -1)
    		{
    			ERROR_LOG("Error while waiting after releasing mutex");
    			thread_func_args->thread_complete_success = false;
    		}
    		else
    		{
    			// Waited successfully, now release mutex
    			rc = pthread_mutex_unlock(thread_func_args->mutex);
    			if(rc != 0)
    			{
    				ERROR_LOG("Error unlocking mutex. rc = %d", rc);
    				thread_func_args->thread_complete_success = false;
    			}
    		}
    	}
    }
    
    //return thread_param;
    return thread_func_args;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
     
    // Allocate memory for thread data and initialize it
    struct thread_data* funcArgs = (struct thread_data*)(malloc(sizeof(struct thread_data)));
    
    funcArgs->wait_to_obtain_ms = wait_to_obtain_ms;
    funcArgs->wait_to_release_ms = wait_to_release_ms;
    funcArgs->mutex = mutex;
    funcArgs->thread_complete_success = true; // Default case is success
    
    // Setup mutex
    int rc = pthread_mutex_init(mutex, NULL);
    if(rc != 0)
    {
    	ERROR_LOG("Error setting up the mutex. rc = %d", rc);
    	free(funcArgs);
    	return false;
    }
    
    // Create the thread
    rc = pthread_create (thread, NULL, threadfunc, funcArgs);
    if(rc != 0)
    {
    	ERROR_LOG("Error creating the thread. rc = %d", rc);
    	free(funcArgs);
    	return false;
    }
    
    return true;
}

