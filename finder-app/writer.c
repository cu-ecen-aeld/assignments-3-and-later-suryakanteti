#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>

#define VALID_NUM_OF_ARGUMENTS 3

int main(int argc, char* argv[])
{	
	if(argc != VALID_NUM_OF_ARGUMENTS)
	{
		// Error message using SYSLOG
		return 1;
	}
	
	// Set up logging
	openlog("AesdWriterApp", LOG_NDELAY, LOG_USER);
	
	// Parse the arguments
	const char* writeFile = argv[1];
	char* writeStr = argv[2];
	
	// Create the file if not present, clear existing contents if file present.
	int fd;
	syslog(LOG_INFO, "Performing file open operation on %s", writeFile);
	fd = open(writeFile, O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (fd == -1)
	{
		syslog(LOG_ERR, "File open failed with error: %d", errno);
		closelog();
		return 1;
	}
	
	// Write the string to the file
	size_t len = strlen(writeStr); // Length of the string to be written
	ssize_t writeResult;
	
	syslog(LOG_INFO, "Writing string %s into file %s", writeStr, writeFile);
	while (len != 0 && (writeResult = write(fd, writeStr, strlen(writeStr)) != 0))
	{
		if(writeResult == -1)
		{
			if(errno == EINTR)
				continue;
				
			syslog(LOG_ERR, "File write failed with error: %d", errno);
			break;
		}
		
		len -= writeResult;
		writeStr += writeResult;
	}
	
	// Close the file
	syslog(LOG_INFO, "Performing file close operation");
	fd = close(fd);
	if(fd == -1)
	{
		syslog(LOG_ERR, "File close failed with error: %d", errno);
		closelog();
		return 1;
	}
	
	// Clean up logging
	closelog();
	return 0;
}
