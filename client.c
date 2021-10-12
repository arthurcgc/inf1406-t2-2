#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#define N_REQUESTS 1

void tcperror(char *s){
    fprintf(stderr, "tcp error: %s\n", s);
}

int create_tcp_connection(const char *hostname, unsigned short port)
{
    // create socket
    int sock;
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Error : Could not create socket\n");
        return -1;
    }
 
    // find server
    struct hostent *server;
    server = gethostbyname(hostname);
    if(server == NULL)
    {
        perror("Could not find server\n");
        return -1;
    }
 
    // create address
    struct sockaddr_in serveraddr;
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    
    // connect to server
    if(connect(sock, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
    {
        perror("Could not connect to server\n");
        return -1;
    }
 
    return sock;
}

void make_first_request(int sock, char *buf, int size)
{
    // 5. prepare and send request
    bzero(buf,size);

    int cNum = rand() % 10;
    printf("[DEBUG]client: sending number %d\n", cNum);
    sprintf(buf, "%d", cNum);
    if(send(sock, buf, strlen(buf), 0) < 0)
    {
        perror("Error while sending request");
        return;
    }

    /*
    * Server Response
    */
    if (recv(sock, buf, sizeof(char)*64, 0) < 0)
    {
        tcperror("Recv()");
        exit(6);
    }
    // close(sock);
}

int nextChunkRequest(int sock)
{
    char requestBody[128];
    // 5. prepare and send request
    bzero(requestBody,sizeof(char)*128);
    sprintf(requestBody, "nextchunk\n");
    // printf("DEBUG: sending->%s\n", requestBody);
    if(send(sock, requestBody, sizeof(char)*128, 0) < 0)
    {
        perror("Error while sending request");
        return 0;
    }

    char responseBody[64];
    bzero(responseBody, sizeof(char) * 64);
    /*
    * Server Response
    */
    // printf("Getting response from server...\n");
    if (recv(sock, responseBody, sizeof(char) * 64, 0) <= 0)
    {
        // printf("server done\n");
        return 0;
    }
    fprintf(stdout, "%s\n", responseBody);
    return 1;
}

int main(int argc,char ** argv)
{
    unsigned short port;       /* port client will connect to         */
    char buf[64];             /* data buffer for sending & receiving */
    struct hostent *hostnm;    /* server host name information        */
    struct sockaddr_in server; /* server address                      */
    int s;                     /* client socket                       */
    time_t t;

    /* Intializes random number generator */
    srand((unsigned) time(&t));

    /*
     * Check Arguments Passed. Should be hostname and port.
     */
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
        exit(1);
    }
    
    struct timeval start,stop;
    gettimeofday(&start, NULL);
    const char *hostname = argv[1];
    port = (unsigned short) atoi(argv[2]);
    for (size_t i = 0; i < N_REQUESTS; i++)
    {
        s = create_tcp_connection(hostname, port);
        make_first_request(s, buf, 64);
        fprintf(stdout, "%s\n", buf);

        if (strcmp(buf, "ready\n") == 0 || strcmp(buf, "ready") == 0){
            for (int i = 0;; i++)
            {
                if (nextChunkRequest(s) == 0) {
                    break;
                }
            }
        }

        close(s);
    }
    gettimeofday(&stop, NULL);
    

    printf("Client Ended Successfully\nElapsed time (ms): %f\n", (stop.tv_usec - start.tv_usec) / 1000.0);

    return 0;

}
