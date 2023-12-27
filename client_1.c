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

/*========== Error checking functions ==========*/

void check(int ret, const char* message) {
    // Function for error checking
    if (ret != -1) {
        return;
    }
    perror(message);
    exit(errno);
}

/*========== Main server functions ==========*/

void send_request(int fd, const char *request_type, const char *data) {
    char message[4096];
    snprintf(message, sizeof(message), "%s:%s", request_type, data);
    ssize_t bytes_written = write(fd, message, strlen(message) + 1);
    check(bytes_written, "Error - bytes_written");
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

    // Send requests
    send_request(fd, "GET_HELLO", "");
    send_request(fd, "SEND_MESSAGE", "Hello, server!");

    char buffer[4096];
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        // Read data from the server
        printf("Received: \"%s\"\n", buffer);
    }

    check(bytes_read, "Error - bytes_read");

    close(fd);

    return 0;
}
