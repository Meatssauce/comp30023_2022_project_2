//
// Created by Mason Zhang on 2/6/2022.
//

// A simple client program for server.c
// To compile: gcc client.c -o client
// To run: start the server, then the client

#define _POSIX_C_SOURCE 200112L

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_REQUEST_CHAR 516

char *readfile(char *filename) {
    char *buffer = 0;
    long length;
    FILE *f = fopen(filename, "rb");

    if (f) {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = malloc(length);
        if (buffer) fread(buffer, 1, length, f);
        fclose(f);
    }

    if (buffer) return buffer;
    return NULL;
}

int main(int argc, char **argv) {
    int sockfd, n, s;
    struct addrinfo hints, *servinfo, *rp;
    char *buffer;

    if (argc < 3) {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Create address
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Get addrinfo of server. From man page:
    // The getaddrinfo() function combines the functionality provided by the
    // gethostbyname(3) and getservbyname(3) functions into a single interface
    s = getaddrinfo(argv[1], argv[2], &hints, &servinfo);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    // Connect to first valid result
    // Why are there multiple results? see man page (search 'several reasons')
    // How to search? enter /, then text to search for, press n/N to navigate
    for (rp = servinfo; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1)
            continue;

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break; // success

        close(sockfd);
    }
    if (rp == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(servinfo);

    // Read message from file
    buffer = readfile("request.txt");
    printf("Sending message: \n%s\n", buffer);

    // Send message to server
    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Read message from server
    n = read(sockfd, buffer, 255);
    if (n < 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }
    // Null-terminate string
    buffer[n] = '\0';
    printf("%s\n", buffer);

    close(sockfd);
    return 0;
}
