#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	printf("Number of arguments are %d", argc);
	
	if(argc != 3) // Including the application itself
	{
		// Error message using SYSLOG
		return -1;
	}
	
	const char* writeFile = argv[1];
	
	printf("%s\n", writeFile);
	
	const char* writeStr = argv[2];
	
	printf("%s\n", writeStr);
	
	int fd;
	
	fd = open(writeFile, O_WRONLY | O_TRUNC | O_CREAT, 0644);
	
	if (fd == -1)
	{
		// Error has occurred. Use syslog statement here
		return -1;
	}
	
	ssize_t writeResult = write(fd, writeStr, strlen(writeStr));
	if(writeResult == -1)
	{
		// Error message using SYSLOG. Write error. Check errno
		return -1;
	}
	else if(writeResult != strlen(writeStr))
	{
		// Error. String not correctly written
		return -1;
	}
	
	fd = close(fd);
	if(fd == -1)
	{
		// Error while closing the file
		return -1;
	}
	return 0;
}
