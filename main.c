//#ifdef _WIN32
////For Windows
//int betriebssystem = 1;
//#include <winsock2.h>
//#include <ws2tcpip.h>
//#include <iphlpapi.h>
//#include <ws2def.h>
//#pragma comment(lib, "Ws2_32.lib")
//#include <windows.h>
//#include <io.h>
//#else
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <netdb.h>
//int betriebssystem = 2;
//#endif

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#ifndef NB_THREADS
#define NB_THREADS 5
#endif

#define EOL "\r\n"
#define EOL_SIZE 2

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
const char *webroot;

typedef struct {
    char *name;
    char *mimetype;
} fextn;

fextn file_extensions[] = {
        {".html", "text/html"},
        {".jpeg", "image/jpeg"},
        {".css",  "test/css"},
        {".js",   "text/js"},
        {NULL, NULL}
};

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void print_ips(struct addrinfo *lst) {
    // IPv4
    char ipv4[INET_ADDRSTRLEN];
    struct sockaddr_in *addr4;

    // IPv6
    char ipv6[INET6_ADDRSTRLEN];
    struct sockaddr_in6 *addr6;

    for (; lst != NULL; lst = lst->ai_next) {
        if (lst->ai_addr->sa_family == AF_INET) {
            addr4 = (struct sockaddr_in *) lst->ai_addr;
            inet_ntop(AF_INET, &addr4->sin_addr, ipv4, INET_ADDRSTRLEN);
            printf("Host IP: %s\n", ipv4);
        } else if (lst->ai_addr->sa_family == AF_INET6) {
            addr6 = (struct sockaddr_in6 *) lst->ai_addr;
            inet_ntop(AF_INET6, &addr6->sin6_addr, ipv6, INET6_ADDRSTRLEN);
            printf("Host IP: %s\n", ipv6);
        }
    }
}

void checkHostName(int hostname) {
    if (hostname == -1) {
        perror("gethostname");
        exit(1);
    }
}

void checkHostEntry(struct hostent *hostentry) {
    if (hostentry == NULL) {
        perror("gethostbyname");
        exit(1);
    }
}

void checkIPbuffer(char *IPbuffer) {
    if (NULL == IPbuffer) {
        perror("inet_ntoa");
        exit(1);
    }
}

/*
 This function recieves the buffer
 until an "End of line(EOL)" byte is recieved.
 Returns len of buffer, or 0 if reading failed.
 */
int recv_new(int fd, char *buffer) {
    char *p = buffer; // Use of a pointer to the buffer rather than dealing with the buffer directly
    int eol_matched = 0; // Use to check whether the recieved byte is matched with the buffer byte or not
    while (recv(fd, p, 1, 0) != 0) // Start receiving 1 byte at a time
    {
        if (*p == EOL[eol_matched]) // if the byte matches with the first eol byte that is '\r'
        {
            ++eol_matched;
            if (eol_matched == EOL_SIZE) // if both the bytes matches with the EOL
            {
                *(p + 1 - EOL_SIZE) = '\0'; // End the string
                return (int) strlen(buffer); // Return the bytes recieved
            }
        } else {
            eol_matched = 0;
        }
        p++; // Increment the pointer to receive next byte
    }
    return 0;
}

/*
 * Read any file. Return 0 if successful, 1 if file not found, 2 if failed to allocate memory for buffer.
 */
int readfile(char *filename, char *buffer) {
    long length;
    FILE *f = fopen(filename, "rb");

    if (!f) return 1;

    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    buffer = malloc(length);
    if (!buffer) return 2;
    fread(buffer, 1, length, f);
    fclose(f);

    return 0;
}

/*
 A helper function
 */
off_t get_file_size(int fd) {
    struct stat stat_struct;
    if (fstat(fd, &stat_struct) == -1)
        return 1;
    return (int) stat_struct.st_size;
}

void slice(const char *str, char *result, size_t start, size_t end) {
    strncpy(result, str + start, end - start);
}

int str_index(const char *str, char *substr) {
    return (int)(strstr(str, substr) - str);
}

void serverop(int newsockfd) {
    int n;
    char request[512];

//    // Convert request into string
//    if (recv_new(newsockfd, request) == 0) {
//        fprintf(stderr, "Recieve Failed\n");
//        exit(EXIT_FAILURE);
//    }

//    printf("Received request:\n%s\n", request);

    // todo: investigate recv

    // Read characters from the connection, then process
    n = (int) read(newsockfd, request, 255); // n is number of characters read
    if (n < 0) error("read");
    // Null-terminate string
    request[n] = '\0';

    printf("Received message:\n%s\n", request);

    char pathname[512];

    // Validate request format
    if (strstr(request, "HTTP") == NULL) {
        fprintf(stderr, "NOT HTTP!\n");
        close(newsockfd);
        return;
    }
    if (strncmp(request, "GET ", 4) != 0) {
        fprintf(stderr, "Unknown Request!\n");
        close(newsockfd);
        return;
    }

    // Parse path name from request
    int path_end = str_index(request, " HTTP");
    slice(request, pathname, 4, path_end);
    printf("%s\n", pathname);

    int resourcefd = -1;
    char *content_type;
    char resource_pathname[512];

    // Open the file it's under web root
    if (strncmp(pathname, "..", 2) != 0) {
        strcpy(resource_pathname, webroot);
        strcat(resource_pathname, pathname);
        for (int i = 0; file_extensions[i].name != NULL; i++) {
            if ((strstr(pathname, file_extensions[i].name)) == NULL)
                continue;

            content_type = file_extensions[i].mimetype;
            resourcefd = open(resource_pathname, O_RDONLY, 0);
            printf("Opening \"%s\"\n", resource_pathname);

            break;
        }
    }

    char *response = malloc(sizeof response);

    // Compile and send headers
    if (resourcefd < 0) {
        fprintf(stderr, "404 File Not Found Error\n");
        strcat(response, "HTTP/1 404 Not Found\r\n");
    } else {
        strcat(response, "HTTP/1 200 OK\r\n");
    }
    strcat(response, content_type);
    if (send(newsockfd, response, strlen(response), 0) == -1) {
        error("send");
        close(newsockfd);
        return;
    }
    free(response);

    // Send the file
    if (resourcefd >= 0) {
        off_t length;
        if ((length = get_file_size(resourcefd)) == -1)
            fprintf(stderr, "Error in getting file size!\n");

        size_t total_bytes_sent = 0;
        ssize_t bytes_sent;
        while (total_bytes_sent < length) {
            off_t offset = length - (off_t) total_bytes_sent;
            if ((bytes_sent = sendfile(newsockfd, resourcefd, 0,
                                       &offset, NULL, 0)) <= 0) {
                if (errno == EINTR || errno == EAGAIN) {
                    continue;
                }
                fprintf(stderr, "Error sending file %d\n", errno);
                close(newsockfd);
                return;
            }
            total_bytes_sent += bytes_sent;
        }

        close(resourcefd);
    }

    close(newsockfd);
}

_Noreturn void *serveroploop(void *sockfd) {
    int newsockfd;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_size = sizeof client_addr;

    // Accept a connection - blocks until a connection is ready to be accepted
    // Get back a new file descriptor to communicate on
    while (1) {
        pthread_mutex_lock(&lock);
        newsockfd = accept(*(int *) sockfd, (struct sockaddr *) &client_addr, &client_addr_size);
        pthread_mutex_unlock(&lock);
        if (newsockfd < 0) error("accept");

        serverop(newsockfd);
        close(newsockfd);
    }
}

int main(int argc, char *argv[]) {
    int sockfd, re, s;
    struct addrinfo hints, *res;
    pthread_t pids[NB_THREADS];

    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(EXIT_FAILURE);
    }

    int protocol = (int) *argv[1] - '0';
    char *port = argv[2];
    webroot = argv[3];

    printf("Protocol: IPv%d\n", protocol);
    printf("Port: %s\n", port);
    printf("Web root: %s\n", webroot);

    // Create address we're going to listen on (with given port number)
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept
    // node (NULL means any interface), service (port), hints, res
    s = getaddrinfo(NULL, port, &hints, &res);
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

    print_ips(res);
//    // Print host ip - does not work! blocks for some reason
//    struct hostent *host_entry;
//    char hostbuffer[256];
//    char *IPbuffer;
//    // Returns hostname for the local computer
//    gethostname(hostbuffer, sizeof hostbuffer);
//    checkHostName(hostbuffer);
//    // Returns host information corresponding to host name
//    host_entry = gethostbyname(hostbuffer);
//    checkHostEntry(host_entry);
//    IPbuffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
//    // Converts space-delimited IPv4 addresses to dotted-decimal format
//    checkIPbuffer(IPbuffer);
//    printf("Hostname: %s\n", hostbuffer);
//    printf("Host IP: %s", IPbuffer);
//    printf("This line is never reached");

    freeaddrinfo(res);

    // Listen on socket - means we're ready to accept connections,
    // incoming connection requests will be queued, man 3 listen
    if (listen(sockfd, 5) < 0) error("listen");
//
//    if (pthread_mutex_init(&lock, NULL)) {
//        fprintf(stderr, "mutex init failed\n");
//        exit(EXIT_FAILURE);
//    }

    for (int i = 0; i < NB_THREADS; i++) {
        if (pthread_create(&pids[i], NULL, serveroploop, (void *) &sockfd)) {
            fprintf(stderr, "Error creating thread %d\n", i);
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < NB_THREADS; i++) {
        if (pthread_join(pids[i], NULL)) {
            fprintf(stderr, "Error joining thread %d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    pthread_mutex_destroy(&lock);
    close(sockfd);
    return 0;
}
