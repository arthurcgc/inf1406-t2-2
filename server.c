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
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>

#define FD_SIZE 10
#define SOCKET_TIMEOUT 15

void notReady(int *sock, char response[64]) {
    strcpy(response, "not ready\n");
    /*
    * Send the message back to the client.
    */
    if (send(*sock, response, sizeof(char) * 64, 0) < 0)
    {
        perror("Send()");
        exit(8);
    }
    
    return;
}

void doNextChunk(int *sock, int fn, struct timeval socketTime) {
    FILE *fp;
    char buffer[64];
    char filepath[100];
    char *requestBody = (char*)malloc(sizeof(char)*128);

    sprintf(filepath, "data/%d.txt", fn);
    fp = fopen(filepath, "r");
    if (fp == NULL) {
        return;
    }
    
    for(;;){
        struct timeval checkTime;
        gettimeofday(&checkTime, NULL);
        if (checkTime.tv_sec - socketTime.tv_sec > SOCKET_TIMEOUT){
            printf("DEBUG: socket timeout, closing connection\n");
            return;
        }
        bzero(buffer, sizeof(char)*64);
        if (fgets(buffer, 64, fp) == NULL) {
            return;
        }
        if (recv(*sock, requestBody, sizeof(char)*128, 0) == -1)
        {
            perror("Recv()");
            exit(7);
        }

        if (strcmp(requestBody, "nextchunk\n") == 0 || strcmp(requestBody, "nextchunk") == 0) {
            
            /*
            * Send the message back to the client.
            */
            if (send(*sock, buffer, sizeof(char) * 64, 0) < 0)
            {
                perror("Send()");
                exit(8);
            }
        }
        else {
            return;
        }
    }

    return;
}

void start(int *sock, char response[64], int fn, struct timeval socketTime) {
    strcpy(response, "ready\n");
    /*
    * Send the message back to the client.
    */
    if (send(*sock, response, sizeof(char) * 64, 0) < 0)
    {
        perror("Send()");
        exit(8);
    }

    doNextChunk(sock, fn, socketTime);    
}

void respond(char *requestBody, char response[64], int *sock, fd_set *master, struct timeval socketTime){
    if (strlen(requestBody) == 1 || strlen(requestBody) == 2){
        int n = atoi(requestBody);
        if (n >= 0 && n <= 9)
        {
            start(sock, response, n, socketTime);
            return;
        }
    }

    notReady(sock, response);
    printf("DEBUG: client socket = %d;\trequest body = %s;\tresponse to be sent = \"%s\";\n", *sock, requestBody, response);
    return;
}

void handleConnection(int *clientServerSocket, fd_set *master, struct timeval socketTime) {
    int responseSize = sizeof(char) * 64;
    int requestSize = sizeof(char) * 128;
    printf("DEBUG: handling connection\n");
    char *requestBody = (char*)malloc(requestSize);
    char response[64];
    bzero(response, responseSize);
    int nbytes;

    /*
    * Receive the message on the newly connected socket.
    */
    nbytes = recv(*clientServerSocket, requestBody, sizeof(requestBody), 0);
    if (nbytes <= 0)
    {
        if(nbytes == 0)
            printf("server: Socket %d hung up\n", *clientServerSocket);
        else
            perror("Recv()");
        close(*clientServerSocket);
        FD_CLR(*clientServerSocket, master);
        return;
    }

    respond(requestBody, response, clientServerSocket, master, socketTime);
    close(*clientServerSocket);
    FD_CLR(*clientServerSocket, master);
}

/*
 * Server Main.
 */
int main(int argc, char** argv)
{
    unsigned short port;       /* port server binds to                */
    char request;             /* buffer for sending & receiving data */
    char response[128];       /* buffer for sending & receiving data */
    struct sockaddr_in client_addr; /* client address information          */
    struct sockaddr_in server_addr; /* server address information          */
    int serverSocket;          /* socket for accepting connections; aka parent socket   */
    int clientServerSocket;    /* socket connected to client; aka child socket  */
    int namelen;               /* length of client name               */
    int optval;                /* flag value for setsockopt */
    int fdmax;

    /* master file descriptor list */
    fd_set master;

    /* temp file descriptor list for select() */
    fd_set read_fds;

    /* clear the master and temp sets */
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    /*
     * Check arguments. Should be only one: the port number to bind to.
     */
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(1);
    }

    /*
     * Get a socket for accepting connections.
     */
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        perror("Socket()");
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
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port   = htons(atoi(argv[1]));
    memset(&(server_addr.sin_zero), '\0', 8);

    if (bind(serverSocket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind()");
        exit(3);
    }

    /*
     * Listen for connections. Specify the backlog as 1.
     */
    if (listen(serverSocket, FD_SIZE) != 0)
    {
        perror("Listen()");
        exit(4);
    }


    /* add the listener to the master set */
    FD_SET(serverSocket, &master);
    /* keep track of the biggest file descriptor */
    fdmax = serverSocket; /* so far, it's this one*/
    
    // setting timeout to 5min
    struct timeval timeout;
    timeout.tv_sec = 5*60;
    timeout.tv_usec = 0;
    // creating a hashtable for each socket connection time
    struct timeval connectionTime[10];


    while (1)
    {
        // copy
        read_fds = master;

        if (select (fdmax+1, &read_fds, NULL, NULL, &timeout) == -1)
        {
          perror("Select()");
          exit(5);
        }
        printf("Server-select() is OK...\n");

        for (int i = 0; i <=fdmax ; i++)
        {
            if (FD_ISSET(i, &read_fds)) {
                if (i == serverSocket){
                    int addrlen = sizeof(client_addr);
                    // we have reached the listening socket
                    // new connection
                    clientServerSocket = accept(serverSocket, (struct sockaddr *)&client_addr, (unsigned int *) &addrlen);
                    if ( clientServerSocket  == -1)
                    {
                        perror("Accept()");
                        exit(6);
                    }
                    FD_SET(clientServerSocket, &master);
                    if( clientServerSocket > fdmax) {
                        fdmax = clientServerSocket;
                    }
                    //  gettimeofday(&connectionTime[i], NULL);
                    printf("%s: New connection from %s on socket %d\n", argv[0], inet_ntoa(client_addr.sin_addr), clientServerSocket);
                }
                else {
                    // talk to client
                    gettimeofday(&connectionTime[i], NULL);
                    handleConnection(&i, &master, connectionTime[i]);
                }
            }
        }
    }
    close(serverSocket);

    printf("Server ended successfully\n");
    return 0;
}