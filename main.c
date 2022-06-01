#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void *serverop(void *sockfd) {
    int newsockfd, n;
    char buffer[256];
    struct sockaddr_storage client_addr;
    socklen_t client_addr_size;

    // Accept a connection - blocks until a connection is ready to be accepted
    // Get back a new file descriptor to communicate on
    client_addr_size = sizeof client_addr;
    newsockfd = accept(*(int *)sockfd, (struct sockaddr*)&client_addr, &client_addr_size);
    if (newsockfd < 0) error("accept");

    // Read characters from the connection, then process
    n = (int)read(newsockfd, buffer, 255); // n is number of characters read
    if (n < 0) error("read");
    // Null-terminate string
    buffer[n] = '\0';

    // Write message back
    printf("Here is the message: %s\n", buffer);
    n = (int)write(newsockfd, "I got your message", 18);
    if (n < 0) error("write");

    close(newsockfd);
    return NULL;
}

pthread_mutex_t lock;

int main(int argc, char *argv[]) {
    int sockfd, newsockfd, n, re, s;
    char buffer[256];
    struct addrinfo hints, *res;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_size;

    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(EXIT_FAILURE);
    }

    // Create address we're going to listen on (with given port number)
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept
    // node (NULL means any interface), service (port), hints, res
    s = getaddrinfo(NULL, argv[1], &hints, &res);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    // Create socket
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) error("socket");

    // Reuse port if possible
    re = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) error("setsockopt");
    // Bind address to the socket
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) error("bind");
    freeaddrinfo(res);

    // Listen on socket - means we're ready to accept connections,
    // incoming connection requests will be queued, man 3 listen
    if (listen(sockfd, 5) < 0) error("listen");

    int nbthreads = 5;
    pthread_t pids[nbthreads];

    if (pthread_mutex_init(&lock, NULL)) {
        fprintf(stderr, "mutex init failed\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < nbthreads; i++) {
        if (pthread_create(&pids[i], NULL, serverop, (void *)&sockfd)) {
            fprintf(stderr, "Error creating thread %d\n", i);
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < nbthreads; i++) {
        if (pthread_join(pids[i], NULL)) {
            fprintf(stderr, "Error joining thread %d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    pthread_mutex_destroy(&lock);
    close(sockfd);
    return 0;
}
