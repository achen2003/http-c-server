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
#include <jansson.h>

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
    int port_num;
    // LogLevel log_level;
    const char *log_file_path;
} ServerConfig;

// static ServerConfig default_config = {
//     .port_num = 8080,
//     .log_file_path = "server_logs.txt"
// };

/*========== Error checking functions ==========*/

void check(int ret, const char* message) {
    // Function for error checking
    if (ret != -1) {
        return;
    }
    perror(message);
    exit(errno);
}

void print_character_codes(const char *str) {
    for (int i = 0; str[i] != '\0'; ++i) {
        printf("%d ", str[i]);
    }
    printf("\n");
}

/*========== Configuration functions ==========*/

void load_config_from_json(const char *file_path, ServerConfig *server_config) {
    json_t *root;
    json_error_t error;

    // Load configuration data from JSON file
    root = json_load_file(file_path, 0, &error);

    if (!root) {
        fprintf(stderr, "Error loading JSON file: %s\n", error.text);
        exit(EXIT_FAILURE);
    }

    json_t *port_value = json_object_get(root, "port");

    if (json_is_integer(port_value)) {
        server_config -> port_num = json_integer_value(port_value);
    } else {
        fprintf(stderr, "Invalid 'port' value in JSON file\n");
        exit(EXIT_FAILURE);
    }

    json_t *log_file_path_value = json_object_get(root, "log_file_path");

    if (json_is_string(log_file_path_value)) {
        const char *log_file_path_str = json_string_value(log_file_path_value);

        // Duplicate the log file path string
        server_config -> log_file_path = strdup(log_file_path_str);
        if (server_config -> log_file_path == NULL) {
            fprintf(stderr, "Error duplicating log file path\n");
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "Invalid 'log_file_path' value in JSON file\n");
        exit(EXIT_FAILURE);
    }

    json_decref(root);

    fprintf(stdout, "Port: %d\n", server_config->port_num);
    fprintf(stdout, "Log File Path: %s\n", server_config->log_file_path);
    fprintf(stdout, "Loaded configuration successfully\n");
}

void init_config(ServerConfig *server_config, const char *file_path) {
    // *server_config = default_config;

    if (file_path != NULL) {
        load_config_from_json(file_path, server_config);
    }
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

    fprintf(stdout, "Log file initialized successfully\n");
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

void handle_client_request(const char *request_type, const char *data, int client_fd) {
    if (strcmp(request_type, "GET_HELLO") == 0) {
        const char *message = "Hello client!";
        ssize_t msg_len = strlen(message) + 1;
        ssize_t bytes_written = write(client_fd, message, msg_len);

        check(bytes_written, "Error - bytes_written");

        if (bytes_written != msg_len) {
            dprintf(2, "Error - write(): Unexpected partial result");
            exit(1);
        }
    } else if (strcmp(request_type, "SEND_MESSAGE") == 0) {
        fprintf(stdout, "Received message from client: %s", data);
        log_message(LOG_INFO, "Received message from client: %s", data);
    } else {
        log_message(LOG_WARNING, "Unknown request type: %s", request_type);
    }
}

void* handle_client(void *arg) {
    ThreadData* thread_data = (ThreadData*) arg;
    int client_fd = thread_data -> client_fd;

    clock_t start_time = clock();

    // Lock client data
    pthread_mutex_lock(&client_mutex);

    // Read and process request from client
    char buffer[4096];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
    ssize_t total_bytes_read = 0;

    while (bytes_read > 0) {
        total_bytes_read += bytes_read;

        char *request_type = strtok(buffer, ":");
        char *data = strtok(NULL, "");

        handle_client_request(request_type, data, client_fd);

        // Check for request delimiter
        if (buffer[total_bytes_read - 1] == '\n') {
            buffer[total_bytes_read - 1] = '\0';
            total_bytes_read = 0;
        }
    }

    check(bytes_read, "Error - bytes_read");

    clock_t end_time = clock();
    double proc_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

    log_message(LOG_INFO, "Client request processes successfully in %f seconds", proc_time);

    pthread_mutex_unlock(&client_mutex);

    check(close(client_fd), "Error - close()");

    free(thread_data);

    return NULL;
}

int main(void) {
    register_signal(SIGINT);
    register_signal(SIGTERM);

    const char *server_config_file_path = "/home/alex/http-c-server/config.json";

    ServerConfig server_config;
    init_config(&server_config, server_config_file_path);

    // print_character_codes(server_config.log_file_path);

    init_log_file(server_config.log_file_path);

    // Set up socket
    fd = socket(AF_INET, SOCK_STREAM, 0);
    check(fd, "Error - fd");

    struct sockaddr_in sockaddr = {0};
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(server_config.port_num);
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

    // Should never reach here
    return 0;
}
