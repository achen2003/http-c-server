#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SOCKET_PATH "example.sock"
#define PORT_NUMBER 12345
#define SERVER_IP "127.0.0.1"

/*===================================================================*/

void check(int ret, const char* message) {
    // Function for error checking
    if (ret != -1) {
        return;
    }
    perror(message);
    exit(errno);
}

int main(void) {
    // Set up socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    check(fd, "Error - fd");

    struct sockaddr_in sockaddr = {0};
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(PORT_NUMBER);
    inet_pton(AF_INET, SERVER_IP, &sockaddr.sin_addr);
    // strncpy(sockaddr.sun_path, SOCKET_PATH, sizeof(sockaddr.sun_path) - 1);

    // Connect to the server
    check(connect(fd, (const struct sockaddr*) &sockaddr, sizeof(sockaddr)), "Error - connect()");

    char buffer[4096];
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        // Read data from the server
    }

    check(bytes_read, "Error - bytes_read");
    printf("Received: \"%s\"\n", buffer);

    close(fd);

    return 0;
}
