#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <syslog.h>

int main()
{	
	int rc, serverSockfd, acceptedSocketfd;
	struct addrinfo hints, *serverInfo, *clientInfo;
	socklen_t* clientInfoLen;
	
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
	hints.ai_family = AF_UNSPEC; // SURYA: Should this be just IPv4?
    	hints.ai_socktype = SOCK_STREAM;
	
	rc = getaddrinfo(NULL, "9000", &hints, &serverInfo);
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
	rc = listen(serverSockfd, 3); // max number of pending connections: 3
	if(rc == -1)
	{
		perror("listen");
		freeaddrinfo(serverInfo);
		return -1;
	}
	
	
	// Accept the connection
	acceptedSocketfd = accept(serverSockfd, clientInfo, clientInfoLen);
	if(acceptedSocketfd == -1)
	{
		perror("accept");
		freeaddrinfo(serverInfo);
		return -1;
	}
	
	syslog(LOG_INFO, "Accept connection from %s", clientInfo->ai_addr->sa_data); // Log client IP to syslog
	
	
	
	/*Append data received to /var/tmp/aesdsocketdata. Create file if it doesn't exist*/
	
	
	
	// freeaddrinfo(res)
	return 0;
}
