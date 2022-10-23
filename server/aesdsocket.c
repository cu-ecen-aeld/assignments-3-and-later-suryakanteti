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
#include <sys/time.h>

// Switch

#define USE_AESD_CHAR_DEVICE 1

#ifdef USE_AESD_CHAR_DEVICE

	#define AESD_SOCKET_DATA_FILE "/dev/aesdchar"

#else

	#define AESD_SOCKET_DATA_FILE "/var/tmp/aesdsocketdata"

#endif

// Macros
#define PORT_NUM_STR "9000"
#define MAX_PENDING_CONNECTIONS 3
#define PACKET_BUFFER_SIZE 512

// Global variables
bool terminate = false;
struct addrinfo* serverInfo = NULL;
int serverSockfd = -1;
int dataFd = -1;
bool hasTimeElapsed = false;
int totalConnections = 0;
pthread_mutex_t mutex;

// Thread structures and types
struct threadInfo_s {

	pthread_t threadId;

	bool connectionInProgress;
	int sockFd;
	struct sockaddr* clientInfo;

	SLIST_ENTRY(threadInfo_s) entries;

};

SLIST_HEAD(slisthead, threadInfo_s) head = SLIST_HEAD_INITIALIZER(head);

// Function declarations
void SignalHandler(int signo);
void SendFileDataToClient(int fileFd, int socketFd);
int SetTimer();
int ClearTimer();
void WriteTimestampToFile(int fd);
void CleanUpList();
void AlarmHandler(int signo);
void* ThreadSendAndReceive(void* threadParams);


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
	signal(SIGALRM, AlarmHandler);
	
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

	// Initialize the mutex
	rc = pthread_mutex_init(&mutex, NULL);
	if(rc != 0)
	{
		perror("pthread_mutex_init");
		close(serverSockfd);
		return -1;
	}

#ifndef USE_AESD_CHAR_DEVICE
	// Set the timer
	rc = SetTimer();
	if(rc == -1)
	{
		close(serverSockfd);
		return -1;
	}
#endif
	
	while(!terminate)
	{
		// Clean up the list if needed
		CleanUpList();

		// Accept the connection
		int acceptedSockFd = accept(serverSockfd, &clientInfo, &clientInfoLen);
		if(acceptedSockFd == -1)
		{
			perror("accept");
			close(serverSockfd);
			return -1;
		}
		
		// Create a node for this and add to list
		struct threadInfo_s* infoPtr = malloc(sizeof(struct threadInfo_s));
		infoPtr->connectionInProgress = true;
		infoPtr->sockFd = acceptedSockFd;
		infoPtr->clientInfo = &clientInfo;

		// Create a thread for the connection
		rc = pthread_create(&(infoPtr->threadId), NULL, ThreadSendAndReceive, infoPtr);
		if(rc != 0)
    	{
			syslog(LOG_ERR, "Error creating the thread. rc = %d", rc);
			close(infoPtr->sockFd);
			close(serverSockfd);
    		return -1;
    	}
		totalConnections++;

		// Add to the list
		SLIST_INSERT_HEAD(&head, infoPtr, entries);
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

/*
 * Function for handling registered signals
 */
void SignalHandler(int signo)
{
	syslog(LOG_INFO, "Caught signal, exiting");
	terminate = true;

	// Close all threads
	struct threadInfo_s* ptr = NULL;
	while(!SLIST_EMPTY(&head))
	{
		ptr = SLIST_FIRST(&head);
		if(ptr->connectionInProgress == false)
		{
			pthread_join(ptr->threadId, NULL);
			SLIST_REMOVE_HEAD(&head, entries);
			free(ptr);
		}
	}

#ifndef USE_AESD_CHAR_DEVICE
	int rc = remove(AESD_SOCKET_DATA_FILE);
	if(rc == -1)
		perror("remove socket data file");
#endif

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
#ifndef USE_AESD_CHAR_DEVICE	
	lseek(fileFd, 0, SEEK_SET);
#endif

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

int SetTimer()
{
	struct itimerval delay;

	delay.it_value.tv_sec = 10;
    delay.it_value.tv_usec = 0;
    delay.it_interval.tv_sec = 10;
    delay.it_interval.tv_usec = 0;


	int rc = setitimer(ITIMER_REAL, &delay, NULL);
	if(rc)
	{
		perror("setitimer");
		return -1;
	}
	return 0;
}

int ClearTimer()
{
	struct itimerval delay;

	delay.it_value.tv_sec = 0;
    delay.it_value.tv_usec = 0;
    delay.it_interval.tv_sec = 0;
    delay.it_interval.tv_usec = 0;


	int rc = setitimer(ITIMER_REAL, &delay, NULL);
	if(rc)
	{
		perror("cleartimer");
		return -1;
	}
	return 0;
}

void WriteTimestampToFile(int fd)
{
	char buf[100];
	memset(buf, 0, 100);

	time_t t = time(NULL);
  	struct tm currentTime = *localtime(&t);

	snprintf(buf, 100, "timestamp: %d-%02d-%02d %02d:%02d:%02d\n", currentTime.tm_year + 1900, currentTime.tm_mon + 1, currentTime.tm_mday, currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec);
	write(fd, buf, strlen(buf) * sizeof(char));
}

void CleanUpList()
{
	// Clean up the list
	struct threadInfo_s* ptr = NULL;
	struct threadInfo_s* nextPtr = NULL;
	SLIST_FOREACH_SAFE(ptr, &head, entries, nextPtr)
	{
		if(ptr->connectionInProgress == false)
		{
			// This thread can be removed
			pthread_join(ptr->threadId, NULL);
			totalConnections--;
			SLIST_REMOVE(&head, ptr, threadInfo_s, entries);
			free(ptr);
		}
	}
}

/*
 * Function for handling alarm signal
 */
void AlarmHandler(int signo)
{
	hasTimeElapsed = true;
	CleanUpList();
	if(totalConnections == 0)
	{
		int fd = open(AESD_SOCKET_DATA_FILE, O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if(fd == -1)
		{
			perror("socket data file open");
			exit(-1);
		}

#ifndef USE_AESD_CHAR_DEVICE
		WriteTimestampToFile(fd);
#endif

	}
}

void* ThreadSendAndReceive(void* threadParams)
{
	// Typecast the arguments
	struct threadInfo_s* params = (struct threadInfo_s*)(threadParams);

	// Log client IP to syslog
	char clientIpStr[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET, params->clientInfo, clientIpStr, sizeof(clientIpStr));
	syslog(LOG_INFO, "Accept connection from %s", clientIpStr);

	char* pktBuf = NULL;
	int fd, rc;
	int currentBufSize = PACKET_BUFFER_SIZE;
	ssize_t retVal;
	char* ptr;
	int remainingSize = 0;
	
	// Receive data from client and append to data file
	pktBuf = (char*)(malloc(PACKET_BUFFER_SIZE * sizeof(char))); // Packet buffer of 512 chars
	if(pktBuf == NULL)
	{
		printf("Error while creating packet buffer!\n");
		close(fd);
		close(params->sockFd);
		exit(-1);
	}
	
	memset(pktBuf, 0, currentBufSize); // Make all zeroes
	
	while((retVal = recv(params->sockFd, pktBuf + currentBufSize - PACKET_BUFFER_SIZE, PACKET_BUFFER_SIZE, 0)) != 0) // Until nothing more to read
	{
		if(retVal == -1)
		{
			perror("recv");
			free(pktBuf);
			close(fd);
			close(params->sockFd);
			exit(-1);
		}

		// Lock mutex
		rc = pthread_mutex_lock(&mutex);
		if(rc != 0)
		{
			printf("Error while obtaining mutex: %d", rc);
			exit(-1);
		}

		// Open the file
		fd = open(AESD_SOCKET_DATA_FILE, O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if(fd == -1)
		{
			perror("socket data file open");
			close(params->sockFd);
			exit(-1);
		}

#ifndef USE_AESD_CHAR_DEVICE
		// Check if time has elapsed
		if(hasTimeElapsed)
		{
			WriteTimestampToFile(fd);
			hasTimeElapsed = false;
		}
#endif
		
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
				close(params->sockFd);
				exit(-1);
			}
			currentBufSize += PACKET_BUFFER_SIZE;
		}
		else
		{
			// Buffer has newline character. Write till there into the file
			write(fd, pktBuf, (ptr - pktBuf + 1) * sizeof(char));

			// Send to client
			SendFileDataToClient(fd, params->sockFd);

			// Close the file
			close(fd);

			// Release the mutex
			rc = pthread_mutex_unlock(&mutex);
			if(rc != 0)
			{
				printf("Error while releasing mutex: %d", rc);
				close(params->sockFd);
				exit(-1);
			}
			
			// Move rest of the content to the start of buffer
			remainingSize = (int)(currentBufSize + pktBuf - ptr - 1);
			memcpy(pktBuf, ptr + 1, remainingSize);
			
			// Clear out the remaining part
			memset(pktBuf + remainingSize, 0, currentBufSize - remainingSize);
		}
	}

	free(pktBuf);
	close(params->sockFd);
	params->connectionInProgress = false;
	syslog(LOG_INFO, "Closed connection from %s", clientIpStr); // Log that connection closed
}
