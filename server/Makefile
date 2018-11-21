TARGET = server

all: $(TARGET)
CC = gcc
LD = gcc

CFLAGS = -O2 -w --std=c99 -Iinclude
LDFLAGS = -pthread
HDRS = *.h 
OBJS = cmd.o server.o

%.o : %.c $(HDRS)
	$(CC) -c $(CFLAGS) $< -o $@

server: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@ && rm *.o

clean:
	rm -f *.o $(TARGET)
