#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>

// Macros
#define PORT_NUM_STR "9000"
#define MAX_PENDING_CONNECTIONS 3
#define AESD_SOCKET_DATA_FILE "/var/tmp/aesdsocketdata"
#define PACKET_BUFFER_SIZE 512

// Global variables
bool terminate = false;
bool connectionInProgress = false;
struct addrinfo* serverInfo = NULL;

/*
 * Function for handling registered signals
 */
void SignalHandler(int signo)
{
	syslog(LOG_INFO, "Caught signal, exiting");
	terminate = true;

	// No active connections, exit
	if(!connectionInProgress)
	{
		int rc = remove(AESD_SOCKET_DATA_FILE);
		if(rc == -1)
		{
			perror("remove socket data file");
			freeaddrinfo(serverInfo);
			exit(-1);
		}
	
		freeaddrinfo(serverInfo);
		exit(-1);
	}
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

/*
 * Main: Entry point for the program
 */
int main()
{	
	int rc, serverSockfd, acceptedSocketfd;
	struct addrinfo hints;
	struct sockaddr clientInfo;
	socklen_t clientInfoLen = sizeof clientInfo;
	
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
	
	// Get addrinfo for socket
	memset(&hints, 0, sizeof hints);
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET; // SURYA: Should this be just IPv4?
    	hints.ai_socktype = SOCK_STREAM;
	
	rc = getaddrinfo(NULL, PORT_NUM_STR, &hints, &serverInfo);
	if(rc != 0)
	{
		printf("getaddrinfo: %s", gai_strerror(rc));
		if(rc == EAI_SYSTEM)
		{
			perror("getaddrinfo");
		}
		return -1;
	}
	
	
	// Bind socket with the info
	rc = bind(serverSockfd, serverInfo->ai_addr, sizeof(struct sockaddr));
	if(rc == -1)
	{
		perror("bind");
		freeaddrinfo(serverInfo);
		return -1;
	}
		
	// Listen for a connection
	rc = listen(serverSockfd, MAX_PENDING_CONNECTIONS); // max number of pending connections: 3
	if(rc == -1)
	{
		perror("listen");
		freeaddrinfo(serverInfo);
		return -1;
	}
	
	
	while(!terminate)
	{
	
		// Accept the connection
		acceptedSocketfd = accept(serverSockfd, &clientInfo, &clientInfoLen);
		if(acceptedSocketfd == -1)
		{
			perror("accept");
			freeaddrinfo(serverInfo);
			return -1;
		}
		connectionInProgress = true;
		syslog(LOG_INFO, "Accept connection from %s", clientInfo.sa_data); // Log client IP to syslog
	
	
		// Create data file if it doesn't exist
		int fd;
		fd = open(AESD_SOCKET_DATA_FILE, O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if(fd == -1)
		{
			perror("socket data file open");
			close(acceptedSocketfd);
			freeaddrinfo(serverInfo);
			return -1;
		}
	
		// Receive data from client and append to data file
		char* pktBuf = (char*)(malloc(PACKET_BUFFER_SIZE * sizeof(char))); // Packet buffer of 512 chars
		int currentBufSize = PACKET_BUFFER_SIZE;
		ssize_t retVal;
		char* ptr;
		int remainingSize = 0;
	
		if(pktBuf == NULL)
		{
			printf("Error while creating packet buffer!\n");
			close(fd);
			close(acceptedSocketfd);
			freeaddrinfo(serverInfo);
			return -1;
		}
	
		memset(pktBuf, 0, currentBufSize); // Make all zeroes
	
		while((retVal = recv(acceptedSocketfd, pktBuf + currentBufSize - PACKET_BUFFER_SIZE, PACKET_BUFFER_SIZE, 0)) != 0) // Until nothing more to read
		{
			if(retVal == -1)
			{
				perror("recv");
				close(fd);
				close(acceptedSocketfd);
				freeaddrinfo(serverInfo);
				return -1;
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
					close(fd);
					close(acceptedSocketfd);
					freeaddrinfo(serverInfo);
					return -1;
				}
				currentBufSize += PACKET_BUFFER_SIZE;
			}
			else
			{
				// Buffer has newline character. Write till there into the file
				rc = write(fd, pktBuf, (ptr - pktBuf + 1) * sizeof(char));

				// Send to client
				SendFileDataToClient(fd, acceptedSocketfd);
			
				// Move rest of the content to the start of buffer
				remainingSize = (int)(currentBufSize + pktBuf - ptr - 1);
				memcpy(pktBuf, ptr + 1, remainingSize);
			
				// Clear out the remaining part
				memset(pktBuf + remainingSize, 0, currentBufSize - remainingSize);
			}
		}
	
		close(fd);
		close(acceptedSocketfd);
		connectionInProgress = false;
		syslog(LOG_INFO, "Closed connection from %s", clientInfo.sa_data); // Log that connection closed
	
	}
	
	
	
	// If it reached this point, file must be deleted
	rc = remove(AESD_SOCKET_DATA_FILE);
	if(rc == -1)
	{
		perror("remove socket data file");
		freeaddrinfo(serverInfo);
		return -1;
	}
	
	freeaddrinfo(serverInfo);
	return 0;
}
