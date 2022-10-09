#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>

#include "queue.h"
#include <pthread.h>

// Macros
#define PORT_NUM_STR "9000"
#define MAX_PENDING_CONNECTIONS 3
#define AESD_SOCKET_DATA_FILE "/var/tmp/aesdsocketdata"
#define PACKET_BUFFER_SIZE 512

// Global variables
bool terminate = false;
struct addrinfo* serverInfo = NULL;
int serverSockfd = -1;

// Thread structures and types
struct threadInfo_s {

	pthread_t* threadId;

	bool connectionInProgress;
	int acceptedSockFd;
	struct sockaddr* clientInfo;

	SLIST_ENTRY(threadInfo_s) next;

};
typedef struct threadInfo_s threadInfo_t;
typedef SLIST_HEAD(head_s, worker_thread_s) head_t;

head_t head;
threadInfo_t* ptr = NULL;
threadInfo_t* nextPtr = NULL;


/*
 * Function for handling registered signals
 */
void SignalHandler(int signo)
{
	syslog(LOG_INFO, "Caught signal, exiting");
	terminate = true;

	SLIST_FOREACH_SAFE(ptr, &head, next, nextPtr)
	{
		if(ptr->connectionInProgress == false)
		{
			// This thread can be removed
			pthread_join(ptr->threadId, NULL);
			SLIST_REMOVE(&head, ptr, threadInfo_t, next);
		}
	}

	int rc = remove(AESD_SOCKET_DATA_FILE);
	if(rc == -1)
		perror("remove socket data file");
	if(serverInfo != NULL)
		freeaddrinfo(serverInfo);
	if(serverSockfd != -1)
		close(serverSockfd);

	exit(0);

}

/*
 * Function to send file content through socket
 */
void SendFileDataToClient(int fileFd, int socketFd)
{
	ssize_t ret;
	char buf[512];

	// Seek to start of file
	lseek(fileFd, 0, SEEK_SET);

	while ((ret = read (fileFd, buf, sizeof buf)) != 0)
	{
        if (ret == -1) 
		{
                if (errno == EINTR)
                    continue;
                perror ("read");
                break;
        }

		// Send whatever is read to client
		send(socketFd, buf, (ret/sizeof(char)), 0);
	}
}

void* ThreadSendAndReceive(void* threadParams)
{
	// Typecast the arguments
	ThreadNode* params = (ThreadNode*)(threadParams);

	// Log client IP to syslog
	char clientIpStr[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET, params->clientInfo, clientIpStr, sizeof(clientIpStr));
	syslog(LOG_INFO, "Accept connection from %s", clientIpStr);

	char* pktBuf = NULL;
	int fd;
	int currentBufSize = PACKET_BUFFER_SIZE;
	ssize_t retVal;
	char* ptr;
	int remainingSize = 0;
	
	// Create data file if it doesn't exist
	fd = open(AESD_SOCKET_DATA_FILE, O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if(fd == -1)
	{
		perror("socket data file open");
		close(params->acceptedSockFd);
		exit(-1);
	}
	
	// Receive data from client and append to data file
	pktBuf = (char*)(malloc(PACKET_BUFFER_SIZE * sizeof(char))); // Packet buffer of 512 chars
	if(pktBuf == NULL)
	{
		printf("Error while creating packet buffer!\n");
		close(fd);
		close(params->acceptedSockFd);
		exit(-1);
	}
	
	memset(pktBuf, 0, currentBufSize); // Make all zeroes
	
	while((retVal = recv(params->acceptedSockFd, pktBuf + currentBufSize - PACKET_BUFFER_SIZE, PACKET_BUFFER_SIZE, 0)) != 0) // Until nothing more to read
	{
		if(retVal == -1)
		{
			perror("recv");
			free(pktBuf);
			close(fd);
			close(params->acceptedSockFd);
			exit(-1);
		}
		
		// Check if the buffer has newline
		ptr = strchr(pktBuf, '\n');
		if(ptr == NULL)
		{
			// No newline character, extend the buffer and continue reading
			pktBuf = (char*)(realloc(pktBuf, currentBufSize + PACKET_BUFFER_SIZE));
			if(pktBuf == NULL)
			{
				printf("Error while reallocating buffer!\n");
				free(pktBuf);
				close(fd);
				close(params->acceptedSockFd);
				exit(-1);
			}
			currentBufSize += PACKET_BUFFER_SIZE;
		}
		else
		{
			// Buffer has newline character. Write till there into the file
			rc = write(fd, pktBuf, (ptr - pktBuf + 1) * sizeof(char));

			// Send to client
			SendFileDataToClient(fd, params->acceptedSockFd);
			
			// Move rest of the content to the start of buffer
			remainingSize = (int)(currentBufSize + pktBuf - ptr - 1);
			memcpy(pktBuf, ptr + 1, remainingSize);
			
			// Clear out the remaining part
			memset(pktBuf + remainingSize, 0, currentBufSize - remainingSize);
		}
	}
	
	free(pktBuf);
	close(fd);
	close(params->acceptedSockFd);
	connectionInProgress = false;
	syslog(LOG_INFO, "Closed connection from %s", clientIpStr); // Log that connection closed
}

/*
 * Main: Entry point for the program
 */
int main(int argc, char* argv[])
{	
	bool daemonMode = false;
	int rc;
	struct addrinfo hints;
	struct sockaddr clientInfo;
	socklen_t clientInfoLen = sizeof clientInfo;

	// Check arguments
	while((rc = getopt(argc, argv,"d")) != -1)
    {
        switch(rc)
        {
            case 'd':
                daemonMode = true;
                break;
        }

    }
	
	//Register for signals
	signal(SIGINT, SignalHandler);
	signal(SIGTERM, SignalHandler);
	
	// Set up syslog
	openlog("aesdsocket", LOG_NDELAY, LOG_USER);
	
	// Create socket
	serverSockfd = socket(PF_INET, SOCK_STREAM, 0); // IPv4, TCP socket
	if(serverSockfd == -1)
	{
		perror("socket");
		return -1;
	}

	// Set up socket to reuse the address
	int a = 1;
	rc = setsockopt(serverSockfd, SOL_SOCKET, SO_REUSEADDR, &a, sizeof(int));
	if(rc == -1)
	{
		perror("setsockopt");
		return -1;
	}
	
	// Get addrinfo for socket
	memset(&hints, 0, sizeof hints);
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;
    	hints.ai_socktype = SOCK_STREAM;
	
	rc = getaddrinfo(NULL, PORT_NUM_STR, &hints, &serverInfo);
	if(rc != 0)
	{
		printf("getaddrinfo: %s", gai_strerror(rc));
		if(rc == EAI_SYSTEM)
		{
			perror("getaddrinfo");
		}
		close(serverSockfd);
		return -1;
	}
	
	
	// Bind socket with the info
	rc = bind(serverSockfd, serverInfo->ai_addr, sizeof(struct sockaddr));
	if(rc == -1)
	{
		perror("bind");
		freeaddrinfo(serverInfo);
		close(serverSockfd);
		return -1;
	}

	freeaddrinfo(serverInfo);

	// Run in daemon mode based on flag
	if(daemonMode)
	{
		rc = daemon(-1, -1); // Non-zero values to perform daemon operations
		if(rc == -1)
		{
			perror("daemon");
			close(serverSockfd);
			return -1;
		}
	}

	// Initialize the thread linked list at this point
	SLIST_INIT(&head);
		
	// Listen for a connection
	rc = listen(serverSockfd, MAX_PENDING_CONNECTIONS); // max number of pending connections: 3
	if(rc == -1)
	{
		perror("listen");
		close(serverSockfd);
		return -1;
	}
	
	while(!terminate)
	{
		// Accept the connection
		int acceptedSockFd = accept(serverSockfd, &clientInfo, &clientInfoLen);
		if(acceptedSockFd == -1)
		{
			perror("accept");
			close(serverSockfd);
			return -1;
		}

		// Clean up the list
		SLIST_FOREACH_SAFE(ptr, &head, next, nextPtr)
		{
			if(ptr->connectionInProgress == false)
			{
				// This thread can be removed
				pthread_join(ptr->threadId, NULL);
				SLIST_REMOVE(&head, ptr, threadInfo_t, nextPtr);
			}
		}
		
		// Create a node for this and add to list
		ptr = malloc(sizeof(ThreadNode));
		ptr->connectionInProgress = true;
		ptr->acceptedSockFd = acceptedSockFd;
		ptr->clientInfo = &clientInfo;

		// Create a thread for the connection
		rc = pthread_create(ptr->thread, NULL, ThreadSendAndReceive, &ptr);
		if(rc != 0)
    	{
			syslog(LOG_ERR, "Error creating the thread. rc = %d", rc);
			close(acceptedSocketfd);
			close(serverSockfd);
    		return -1;
    	}

		// Add to the list
		SLIST_INSERT_HEAD(&head, ptr, next);
	}
	
	// If it reached this point, file must be deleted
	rc = remove(AESD_SOCKET_DATA_FILE);
	if(rc == -1)
	{
		perror("remove socket data file");
		return -1;
	}
	
	close(serverSockfd);
	return 0;
}
