# Makefile for RDMA Server Program

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -g

# Include directories
INCLUDES = -I.

# Libraries
LIBS = -lrdmacm -libverbs -lpthread

# Source files
SRCS = rdma_server.c

# Header files
HDRS = rdma_common.h

# Object files
OBJS = $(SRCS:.c=.o)

# Executable file
TARGET = rdma_server

# Default target
all: $(TARGET)

# Link the object files to create the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

# Compile source files into object files
%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean
