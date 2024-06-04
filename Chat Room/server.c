#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

// Structure to hold client information
typedef struct {
    int sockfd;
    char username[BUFFER_SIZE];
    struct sockaddr_in addr;
} ClientInfo;

// Global variables
ClientInfo clients[MAX_CLIENTS];
int num_clients = 0;
int server_sockfd;
int max_clients;

// Just a temporary flag
bool flag = false;

// Mutex for thread safety
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Function to broadcast a message to all clients except sender
void broadcast_message(const char *message, int sender_ind) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < num_clients; i++) {
        if (i != sender_ind) { // Don't send message to the sender
            send(clients[i].sockfd, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&mutex);
}

// Function to handle a client's connection
void *handle_client(void *arg) {

    int client_index = *((int *)arg);
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + 50]; // Increased buffer size for formatted messages

    while (1) {
        int bytes_received = recv(clients[client_index].sockfd, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            // Client disconnected
            // Printing in server console for logging purposes
            printf("Client %s is disconnected.\n", clients[client_index].username);
            close(clients[client_index].sockfd);

            // Broadcast leave message to all clients
            strcpy(message, clients[client_index].username);
            strcat(message, " left the chatroom\n");
            broadcast_message(message, client_index);

            // Remove client from the clients list
            pthread_mutex_lock(&mutex);
            for (int i = client_index; i < num_clients - 1; i++) {
                clients[i] = clients[i + 1];
            }
            num_clients--;
            pthread_mutex_unlock(&mutex);

            // Printing new list of users in server console for logging purposes
            printf("List of users: \n");
            for (int i = 0; i < num_clients; i++) {
                printf("%s\n", clients[i].username);
            }
            printf("\n");

            pthread_exit(NULL);
        }
        else
        {
            buffer[bytes_received] = '\0'; // Adding null at the end of the buffer
            if (strcmp(buffer, "\\list") == 0) {

                // Send list of users
                pthread_mutex_lock(&mutex);
                char user_list[BUFFER_SIZE] = "Users in chatroom:\n";
                for (int i = 0; i < num_clients; i++) {
                    strcat(user_list, clients[i].username);
                    strcat(user_list, "\n");
                }
                send(clients[client_index].sockfd, user_list, strlen(user_list), 0);
                pthread_mutex_unlock(&mutex);

            } else if (strcmp(buffer, "\\bye") == 0) {

                // Disconnect client
                close(clients[client_index].sockfd);

                // Broadcast leave message
                strcpy(message, clients[client_index].username);
                strcat(message, " left the chatroom\n");
                broadcast_message(message, client_index);

                // Printing in server console for logging purposes
                printf("Client %s is disconnected.\n", clients[client_index].username);

                // Remove client from the clients list
                pthread_mutex_lock(&mutex);
                for (int i = client_index; i < num_clients - 1; i++) {
                    clients[i] = clients[i + 1];
                }
                num_clients--;
                pthread_mutex_unlock(&mutex);

                // Printing new list of users in server console for logging purposes
                printf("List of users: \n");
                for (int i = 0; i < num_clients; i++) {
                    printf("%s\n", clients[i].username);
                }
                printf("\n");

                pthread_exit(NULL);
            } else {
                // Broadcast message to all clients
                strcpy(message, clients[client_index].username);
                strcat(message, ": ");
                strcat(message, buffer);
                broadcast_message(message, client_index);
            }
        }
    }
}

// Function to start the server
void start_server(int port_no, int max_clients, int timeout) {
    struct sockaddr_in server_addr;

    // Create socket
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_no);

    // Bind socket to address
    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_sockfd, max_clients) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server started. Waiting for connections...\n");

    // Accept connections and handle them in separate threads
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sockfd == -1) {
            perror("Accept failed");
            continue;
        }

        // Prompt client to enter username
        const char *prompt_message = "Please enter your username: ";
        send(client_sockfd, prompt_message, strlen(prompt_message), 0);

        // Receive username
        char username[BUFFER_SIZE];
        int bytes_received = recv(client_sockfd, username, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            close(client_sockfd);
            continue;
        }
        username[bytes_received] = '\0'; // Adding null at the end of the username

        // Check if username already exists
        int username_exists = 0;
        for (int i = 0; i < num_clients; i++) {
            if (strcmp(clients[i].username, username) == 0) {
                username_exists = 1;
                break;
            }
        }

        // Username already exists, keep prompting for a new username (that's howw we chose to implement it)
        while (username_exists) {

            // Prompting the user to enter a different username
            const char *username_exists_message = "Username already exists. Please enter a different username: ";
            send(client_sockfd, username_exists_message, strlen(username_exists_message), 0);

            // Receive new username
            bytes_received = recv(client_sockfd, username, BUFFER_SIZE, 0);
            if (bytes_received <= 0) {
                close(client_sockfd);
                continue;
            }
            username[bytes_received] = '\0';

            // Check if the new username already exists
            username_exists = 0;
            for (int i = 0; i < num_clients; i++) {
                if (strcmp(clients[i].username, username) == 0) {
                    username_exists = 1;
                    break;
                }
            }
        }

        // Printing in server console for logging purposes
        printf("%s joined the chat\n", username);

        // Printing list of users
        printf("List of users: \n");
        printf("%s\n", username);
        for (int i = 0; i < num_clients; i++) {
            printf("%s\n", clients[i].username);
        }
        printf("\n");

        // Add client to the clients list
        pthread_mutex_lock(&mutex);
        clients[num_clients].sockfd = client_sockfd;
        strcpy(clients[num_clients].username, username);
        clients[num_clients].addr = client_addr;
        num_clients++;
        pthread_mutex_unlock(&mutex);

        // Broadcast join message
        char message[BUFFER_SIZE + 50]; // Increased buffer size for formatted messages
        strcpy(message, username);
        strcat(message, " joined the chatroom\n");
        broadcast_message(message, num_clients - 1); //broadcasting to all clients othre than joined client

        char welcome_message[BUFFER_SIZE + 50]; // Increased buffer size for formatted messages
        strcpy(welcome_message, "Welcome ");
        strcat(welcome_message, username);
        strcat(welcome_message, "\n");
        send(client_sockfd, welcome_message, strlen(welcome_message), 0); // welcome message to current joined client
        

        // Create thread to handle client
        pthread_t tid;
        int *arg = (int *)malloc(sizeof(*arg));
        *arg = num_clients - 1;
        pthread_create(&tid, NULL, handle_client, arg);
    }
}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "arguments count is %d [NOT 4] \n", argc);
        exit(EXIT_FAILURE);
    }

    int port_no = atoi(argv[1]);
    max_clients = atoi(argv[2]);
    int timeout = atoi(argv[3]);

    // Initialize mutex
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("Mutex initialization failed");
        exit(EXIT_FAILURE);
    }

    // Start the server
    start_server(port_no, max_clients, timeout);

    // Destroy mutex
    pthread_mutex_destroy(&mutex);

    return 0;
}
