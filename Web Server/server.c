#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

// #define BUFFER_SIZE 1024
#define BUFFER_SIZE 1000000

typedef struct
{
    int client_fd; // File descriptor for the client connection
    char *root_directory; // Root directory for serving files
} ConnectionArgs;


void *handle_connection(void *arg)
{
    ConnectionArgs *args = (ConnectionArgs *)arg;
    int client_fd = args->client_fd;
    char *root_directory = args->root_directory;
    free(arg); // Freeing memory allocated for argument structure

    // Read request
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_read <= 0)
    {
        printf("Reading error from socket\n");
        close(client_fd);
        pthread_exit(NULL);
    }

    // Parse request
    buffer[bytes_read] = '\0';
    printf("Received request:\n%s\n", buffer);

    // Extract requested file path
    char method[10], path[100];
    sscanf(buffer, "%s %s", method, path);

    // Default to index.html if no page specified
    if (strcmp(path, "/") == 0)
        strcpy(path, "/index.html");

    // Construct full file path
    char full_path[150];

    // Copy root_directory to full_path
    strcpy(full_path, root_directory);

    // Concatenate path to the end of full_path
    strcat(full_path, path);

    // Check if it's a POST request
    if (strcmp(method, "POST") == 0)
    {
        char *body_start = strstr(buffer, "\r\n\r\n");
        if (body_start)
        {
            body_start += 4; // Move past the empty line
            char *text = strstr(body_start, "%**%");
            text += 4;
            int char_count = 0, word_count = 0, sentence_count = 0;
            // Counting characters, words, and sentences in the text
            word_count = 1; // Count the first word
            for (int i = 0; text[i] != '%'; i++)
            {
                if (text[i] == ' ')
                {
                    word_count++;
                }
                else if (text[i] == '.' || text[i] == '!' || text[i] == '?')
                {
                    sentence_count++;
                }
                else
                {
                    char_count++;
                }
            }

            // Prepare response
            strcpy(buffer, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
            strcat(buffer, "<h1>Text Analysis</h1>");
            char temp[100];
            sprintf(temp, "<p>Number of characters: %d</p>", char_count);
            strcat(buffer, temp);
            sprintf(temp, "<p>Number of words: %d</p>", word_count);
            strcat(buffer, temp);
            sprintf(temp, "<p>Number of sentences: %d</p>", sentence_count);
            strcat(buffer, temp);

            send(client_fd, buffer, strlen(buffer), 0);
        }
    }
    else
    { 
        // GET request
        // Open requested file
        FILE *file = fopen(full_path, "rb");
        if (!file)
        {
            // Serve 404.html if file not found
            sprintf(full_path, "%s/404.html", root_directory);
            file = fopen(full_path, "rb");
        }

        // Send response
        char response[BUFFER_SIZE];
        if (file)
        {
            // Send HTTP header
            strcpy(response, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
            send(client_fd, response, strlen(response), 0);

            // Send file content
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
            {
                send(client_fd, buffer, bytes_read, 0);
            }
            fclose(file);
        }
        else
        {
            // File not found, send error response
            strcpy(response, "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n");

            send(client_fd, response, strlen(response), 0);
            strcpy(response, "<h1>404 Not Found</h1>");
            send(client_fd, response, strlen(response), 0);
        }
    }

    // Close connection
    close(client_fd);
    pthread_exit(NULL);
}


int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Arguments are not correct\n");
        return 1;
    }

    int port = atoi(argv[1]);
    char *root_directory = argv[2];

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket creation failed");
        return 1;
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Bind failed");
        close(server_fd);
        return 1;
    }

    // Listen for connections
    if (listen(server_fd, 5) == -1)
    {
        printf("Listen failed\n");
        close(server_fd);
        return 1;
    }

    printf("SERVER ESTABLISHED SUCCESSFULLY on PORT %d...\n", port);

    while (1)
    {
        // Accept connection
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) == -1)
        {
            perror("Accept failed");
            continue;
        }

        // Handle connection in a new thread
        ConnectionArgs *args = (ConnectionArgs *)malloc(sizeof(ConnectionArgs));
        args->client_fd = client_fd;
        args->root_directory = root_directory;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_connection, args) != 0)
        {
            printf("Thread creation failed\n");
            close(client_fd);
            free(args);
            continue;
        }
    }

    close(server_fd);
    return 0;
}
