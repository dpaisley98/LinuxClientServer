CC = gcc
CFLAGS = -Wall -Wextra -g

all: client server

client: client.o
	$(CC) $(CFLAGS) -o $@ $^ -g

server: server.o
	$(CC) $(CFLAGS) -o $@ $^ -pthread -g

client.o: client.c
	$(CC) $(CFLAGS) -c $< -g

server.o: server.c
	$(CC) $(CFLAGS) -c $< -pthread -g

clean:
	rm -f client server *.o
