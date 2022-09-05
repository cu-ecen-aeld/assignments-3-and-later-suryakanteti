# Default compiler
CC = gcc

# Writer application
writer: clean writer.o
	$(CROSS_COMPILE)$(CC) writer.o -o writer

# Writer .o files	
writer.o: writer.c
	$(CROSS_COMPILE)$(CC) -c writer.c
	
# Clean all binaries
clean:
	rm -f *.o writer
