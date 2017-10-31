all: clean client server

client:
	gcc client.c -o client

server:
	gcc server.c -o server -lpthread

clean:
	rm -f server client