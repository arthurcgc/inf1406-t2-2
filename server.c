// O serviço a ser desenvolvido deve receber requisições com um número de 0 a 9 e responder uma string (mantida em uma tabela)de 128 bytes.
// Esse servidor deve disparar um novo thread para cada requsição recebida. Cada thread disparado deve atender a uma única requisição, fechar essa conexão com o cliente atendido e terminar execução.

// O serviço a ser desenvolvido deve receber requisições com um número de 0 a 9 e responder uma string (mantida em uma tabela)de 128 bytes.
// Esse servidor deve disparar um novo processo para cada requsição recebida. Cada processo disparado deve atender a uma única requisição, fechar essa conexão com o cliente atendido e terminar execução.x`

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#define FD_SIZE 10


void tcperror(char *s){
    fprintf(stderr, "tcp error: %s\n", s);
}

void notReady(int *sock, char response[64]) {
    strcpy(response, "not ready\n");
    /*
    * Send the message back to the client.
    */
    if (send(*sock, response, sizeof(char) * 64, 0) < 0)
    {
        tcperror("Send()");
        exit(8);
    }
    close(*sock);
    return;
}

void doNextChunk(int *sock, int fn) {
    FILE *fp;
    char buffer[64];
    char filepath[100];
    char *requestBody = (char*)malloc(sizeof(char)*128);

    sprintf(filepath, "data/%d.txt", fn);
    fp = fopen(filepath, "r");
    if (fp == NULL) {
        close(*sock);
        return;
    }
    
    for(;;){
        bzero(buffer, sizeof(char)*64);
        if (fgets(buffer, 64, fp) == NULL) {
            close(*sock);
            return;
        }
        if (recv(*sock, requestBody, sizeof(char)*128, 0) == -1)
        {
            tcperror("Recv()");
            exit(7);
        }

        if (strcmp(requestBody, "nextchunk\n") == 0 || strcmp(requestBody, "nextchunk") == 0) {
            
            /*
            * Send the message back to the client.
            */
            if (send(*sock, buffer, sizeof(char) * 64, 0) < 0)
            {
                tcperror("Send()");
                exit(8);
            }
        }
        else {
            close(*sock);
            return;
        }
    }

    return;
}

void start(int *sock, char response[64], int fn) {
    strcpy(response, "ready\n");
    /*
    * Send the message back to the client.
    */
    if (send(*sock, response, sizeof(char) * 64, 0) < 0)
    {
        tcperror("Send()");
        exit(8);
    }

    doNextChunk(sock, fn);    
}

void respond(char *requestBody, char response[64], int *sock){
    if (strlen(requestBody) == 1 || strlen(requestBody) == 2){
        int n = atoi(requestBody);
        if (n >= 0 && n <= 9)
        {
            start(sock, response, n);
            return;
        }
    }

    notReady(sock, response);
    printf("DEBUG: client socket = %d;\trequest body = %s;\tresponse to be sent = \"%s\";\n", *sock, requestBody, response);
    return;
}

void handleConnection(int *clientServerSocket) {
    int responseSize = sizeof(char) * 64;
    int requestSize = sizeof(char) * 128;
    printf("DEBUG: handling connection\n");
    char *requestBody = (char*)malloc(requestSize);
    char response[64];
    bzero(response, responseSize);

    /*
    * Receive the message on the newly connected socket.
    */
    if (recv(*clientServerSocket, requestBody, sizeof(requestBody), 0) == -1)
    {
        tcperror("Recv()");
        exit(7);
    }

    respond(requestBody, response, clientServerSocket);
    
    // close(*clientServerSocket);
}

int initServer(int port) {
    struct sockaddr_in server; /* server address information          */
    int serverSocket;          /* socket for accepting connections; aka parent socket   */
    int optval;                /* flag value for setsockopt */

    /*
     * Get a socket for accepting connections.
     */
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        tcperror("Socket()");
        exit(2);
    }
    
    /* setsockopt: Handy debugging trick that lets 
    * us rerun the server immediately after we kill it; 
    * otherwise we have to wait about 20 secs. 
    * Eliminates "ERROR on binding: Address already in use" error. 
    */
    optval = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

    /*
     * Bind the socket to the server address.
     */
    server.sin_family = AF_INET;
    server.sin_port   = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        tcperror("Bind()");
        exit(3);
    }

    /*
     * Listen for connections. Specify the backlog as 1.
     */
    if (listen(serverSocket, 1) != 0)
    {
        tcperror("Listen()");
        exit(4);
    }

    return serverSocket;
}

/*
 * Server Main.
 */
int main(int argc, char** argv)
{
    unsigned short port;       /* port server binds to                */
    char request;             /* buffer for sending & receiving data */
    char response[128];       /* buffer for sending & receiving data */
    struct sockaddr_in client; /* client address information          */
    struct sockaddr_in server; /* server address information          */
    int serverSocket;          /* socket for accepting connections; aka parent socket   */
    int clientServerSocket;    /* socket connected to client; aka child socket  */
    int namelen;               /* length of client name               */
    int optval;                /* flag value for setsockopt */

    fd_set currentSockets, readySockets;

    


    int number;

    /*
     * Check arguments. Should be only one: the port number to bind to.
     */

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(1);
    }

    /*
     * First argument should be the port.
     */
    serverSocket = initServer((unsigned short) atoi(argv[1]));

    /* Initialize the set of active sockets. */
    FD_ZERO(&currentSockets);
    FD_SET(serverSocket, &currentSockets);
    // setting timeout to 5s
    struct timeval timeout;
    timeout.tv_sec = 5;
    // creating a hashtable for each socket connection time
    struct timeval connectionTime[10];


    namelen = sizeof(client);
    while (1)
    {
        /* Select is destructive so we have to keep a copy */
        readySockets = currentSockets;

        if (select (FD_SIZE+1, &readySockets, NULL, NULL, &timeout) < 0)
        {
          tcperror("Select()");
          exit(5);
        }

        for (int i = 0; i < FD_SIZE; i++)
        {
            if (FD_ISSET(i, &readySockets)) {
                if (i == serverSocket){
                    // new connection
                    if (( clientServerSocket = accept(serverSocket, (struct sockaddr *)&client, (unsigned int *) &namelen)) == -1)
                    {
                        tcperror("Accept()");
                        exit(6);
                    }
                    FD_SET(clientServerSocket, &currentSockets);
                    gettimeofday(&connectionTime[i], NULL);
                }
                else {
                    // timeout configuration
                    struct timeval shouldStop; gettimeofday(&shouldStop, NULL);
                    if (shouldStop.tv_sec - connectionTime[i].tv_sec > 5){
                        close(i);
                        continue;
                    }
                    // do wathever we do with connections
                    handleConnection(&i);
                    FD_CLR(i, &currentSockets);
                }
            }
        }
    }
    close(serverSocket);

    printf("Server ended successfully\n");
    return 0;
}