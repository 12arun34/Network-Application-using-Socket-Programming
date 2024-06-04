#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

typedef struct {
    int client_socket;
    const char *root_dir;
} client_data;

void *handle_client(void *arg) {
    client_data *data = (client_data *)arg;
    int client_socket = data->client_socket;
    const char *root_dir = data->root_dir;
    free(data); // Free the memory allocated for client data

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Receive filename from client
    if ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0)) <= 0) {
        printf("Receiving error\n");
        close(client_socket);
        return NULL;
    }

    buffer[bytes_read] = '\0';
    printf("Requested file: %s\n", buffer);
    int song_num = atoi(buffer);

    // Construct full path to the requested file
    char file_path[256];
    printf("song num : %d line", song_num);

    strcpy(file_path,root_dir);
    strcat(file_path,"/song");
    char song_num_str[10];
    sprintf(song_num_str, "%d", song_num);
    strcat(file_path, song_num_str);
    strcat(file_path, ".mp3");

    // Open the requested file
    printf("Searching file: %s\n", file_path);
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        printf("File open error\n");
        close(client_socket);
        return NULL;
    }

    // Send file contents to client
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (send(client_socket, buffer, bytes_read, 0) != bytes_read) {
            printf("send function error\n");
            fclose(file);
            close(client_socket);
            return NULL;
        }
    }

    fclose(file);
    close(client_socket);
    return NULL;
}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Arguments count error\n");
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    const char *mp3_folder = argv[2];
    int max_streams = atoi(argv[3]);

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
 
    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Prepare server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, max_streams) == -1) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("SERVER ESTABILISHED SUCCESSFULLY on PORT %d...\n", port);

    // Accept incoming connections and handle them in separate threads
    while (1) {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len)) == -1) {
            printf("Accept function error\n");
            continue;
        }

        printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        pthread_t tid;
        client_data *data = (client_data *)malloc(sizeof(client_data));
        data->client_socket = client_socket;
        data->root_dir = mp3_folder;

        if (pthread_create(&tid, NULL, handle_client, (void *)data) != 0) {
            printf("Thread creation error\n");
            close(client_socket);
            free(data);
            continue;
        }
        // Detach thread to allow its resources to be released automatically
        pthread_detach(tid); 
    }

    close(server_socket);
    return 0;
}

