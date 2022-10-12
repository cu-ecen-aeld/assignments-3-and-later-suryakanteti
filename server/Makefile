# Default compiler
CC ?= $(CROSS_COMPILE)gcc

# Flags
LDFLAGS ?= -pthread -lrt

all: aesdsocket

# AESD socket application
aesdsocket: clean aesdsocket.o
	$(CC) aesdsocket.o -o aesdsocket $(LDFLAGS)

# aesdsocket .o files	
aesdsocket.o: aesdsocket.c
	$(CC) -c aesdsocket.c $(LDFLAGS)
	
# Clean all binaries
clean:
	rm -f *.o aesdsocket
