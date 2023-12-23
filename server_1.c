#include <assert.h>
#include <bits/pthreadtypes.h>
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

#define SOCKET_PATH "example.sock"
#define PORT_NUMBER 12345

static int fd;
static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int client_fd;
} ThreadData;

/*===================================================================*/

void check(int ret, const char* message) {
    // Function for error checking
    if (ret != -1) {
        return;
    }
    perror(message);
    exit(errno);
}

void close_socket() {
    check(close(fd), "Error - close()");
    // check(unlink(SOCKET_PATH), "Error - unlink()");
    exit(0);
}

void handle_signal(int signum) {
    assert(signum == SIGINT || signum == SIGTERM);
    close_socket();
}

void register_signal(int signum) {
    struct sigaction new_action = {0};
    sigemptyset(&new_action.sa_mask);
    new_action.sa_handler = handle_signal;
    new_action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    check(sigaction(signum, &new_action, NULL), "failed sigaction");
}

void* handle_client(void *arg) {
    ThreadData* thread_data = (ThreadData*) arg;
    int client_fd = thread_data -> client_fd;

    const char *message = "Hello World";
    ssize_t msg_len = strlen(message) + 1;
    ssize_t bytes_written = write(client_fd, message, msg_len);

    // Lock client data
    pthread_mutex_lock(&client_mutex);

    check(bytes_written, "Error - bytes_written");

    if (bytes_written != msg_len) {
        dprintf(2, "Error - write(): Unexpected partial result");
        exit(1);
    }

    check(close(client_fd), "Error - close()");

    pthread_mutex_unlock(&client_mutex);

    free(thread_data);

    return NULL;
}

int main(void) {
    register_signal(SIGINT);
    register_signal(SIGTERM);

    // Set up socket
    fd = socket(AF_INET, SOCK_STREAM, 0);
    check(fd, "Error - fd");

    struct sockaddr_in sockaddr = {0};
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(PORT_NUMBER);
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    // strncpy(sockaddr.sun_path, SOCKET_PATH, sizeof(sockaddr.sun_path) - 1);

    // Open the server to connections
    check(bind(fd, (const struct sockaddr*) &sockaddr, sizeof(sockaddr)), "Error - bind()");
    check(listen(fd, 0), "Error - listen()");

    // Handle connections
    while (true) {
        int client_fd = accept(fd, NULL, NULL);
        check(client_fd, "Error - client_fd");

        ThreadData* thread_data = (ThreadData*)malloc(sizeof(ThreadData));
        if (thread_data == NULL) {
            perror("Error - malloc()");
            exit(errno);
        }

        thread_data -> client_fd =  client_fd;

        pthread_t client_thread;
        check(pthread_create(&client_thread, NULL, &handle_client, (void *)thread_data), "Error - pthread_create()");
        check(pthread_detach(client_thread), "Error - pthread_detach()");
    }

    return 0;
}
