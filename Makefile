.PHONY: client
client: 
	gcc -g -o client client.c
	./client 0.0.0.0 9999

.PHONY: server
server:
	gcc -g -o server server.c
	./server 9999
