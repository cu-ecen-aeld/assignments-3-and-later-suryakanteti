# Default compiler
CC = gcc

all: aesdsocket

# AESD socket application
aesdsocket: clean aesdsocket.o
	$(CROSS_COMPILE)$(CC) aesdsocket.o -o aesdsocket

# aesdsocket .o files	
aesdsocket.o: aesdsocket.c
	$(CROSS_COMPILE)$(CC) -c aesdsocket.c
	
# Clean all binaries
clean:
	rm -f *.o aesdsocket
