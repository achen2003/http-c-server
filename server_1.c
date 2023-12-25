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
#include <stdarg.h>
#include <time.h>

/*========== Global variables and data structures ==========*/

#define SOCKET_PATH "example.sock"
#define PORT_NUMBER 12345

static int fd;

static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

static FILE *log_file = NULL;

typedef struct {
    int client_fd;
} ThreadData;

typedef enum {
    LOG_INFO,
    LOG_DEBUG,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

typedef struct {
    LogLevel log_level;
    const char *log_file_path;
} ServerConfig;

/*========== Error checking functions ==========*/

void check(int ret, const char* message) {
    // Function for error checking
    if (ret != -1) {
        return;
    }
    perror(message);
    exit(errno);
}

/*========== Logging functions ==========*/

const char *log_level_str(LogLevel level) {
    switch (level) {
        case LOG_INFO: return "INFO";
        case LOG_DEBUG: return "DEBUG";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void log_message(LogLevel level, const char *message, ...) {
    if (log_file == NULL) {
        fprintf(stderr, "Error - log file not initialized\n");
        return;
    }

    va_list args;
    va_start(args, message);

    time_t rawtime;
    struct tm* timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    fprintf(log_file, "[%s] %04d-%02d-%02d %02d:%02d:%02d ",
            log_level_str(level), timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
            timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    vfprintf(log_file, message, args);
    fprintf(log_file, "\n");

    va_end(args);
}

void init_log_file(const char *file_path) {
    log_file = fopen(file_path, "a");
    if (log_file == NULL) {
        fprintf(stderr, "Error: Could not open log file '%s'\n", file_path);
        exit(EXIT_FAILURE);
    }
}

void close_log_file() {
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
}

/*========== Main server functions ==========*/

void close_socket() {
    check(close(fd), "Error - close()");
    // check(unlink(SOCKET_PATH), "Error - unlink()");
    exit(0);
}

void handle_signal(int signum) {
    assert(signum == SIGINT || signum == SIGTERM);
    close_log_file();
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

    log_message(LOG_INFO, "Client request processes successfully");

    pthread_mutex_unlock(&client_mutex);

    free(thread_data);

    return NULL;
}

int main(void) {
    register_signal(SIGINT);
    register_signal(SIGTERM);

    init_log_file("/home/alex/http-c-server/logs/server_logs.txt");

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
