# Default compiler
CC ?= $(CROSS_COMPILE)gcc

all: aesdsocket

# AESD socket application
aesdsocket: clean aesdsocket.o
	$(CC) aesdsocket.o -o aesdsocket

# aesdsocket .o files	
aesdsocket.o: aesdsocket.c
	$(CC) -c aesdsocket.c
	
# Clean all binaries
clean:
	rm -f *.o aesdsocket
