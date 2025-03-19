#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include "main.h"

volatile bool shutdown_requested = false;

// Function to handle HTTP requests
void handle_http_request(int client_socket) {
    char buffer[1024];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        printf("Error receiving data.\n");
        return;
    }
    buffer[bytes_received] = '\0';

    // Check if the request contains the shutdown signal
    if (strstr(buffer, "SHUTDOWN") != NULL) {
        shutdown_requested = true; // Set the shutdown flag
        const char *response = "HTTP/1.1 200 OK\r\n\r\nServer is shutting down.";
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        return;
    }

    // Handle file download request
    FILE *file = fopen("/temp.zip", "rb");
    if (!file) {
        const char *not_found_response = "HTTP/1.1 404 Not Found\r\n\r\nFile not found.";
        send(client_socket, not_found_response, strlen(not_found_response), 0);
        close(client_socket);
        return;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Extract the file name from the file path
    const char *file_name = strrchr("/temp.zip", '/');
    if (file_name == NULL) {
        file_name = "/temp.zip"; // If no '/' is found, use the full path
    } else {
        file_name++; // Move past the '/'
    }

    // Send HTTP headers
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/zip\r\n"
             "Content-Disposition: attachment; filename=\"%s\"\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n\r\n", file_name, file_size);
    send(client_socket, header, strlen(header), 0);

    // Send file content
    char file_buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
        send(client_socket, file_buffer, bytes_read, 0);
    }

    fclose(file);
    close(client_socket);
}

int startSend() {
    socketInitializeDefault();
    u32 local_ip = gethostid();
    u32 correct_ip = __builtin_bswap32(local_ip);
    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d",
             (correct_ip >> 24) & 0xFF,
             (correct_ip >> 16) & 0xFF,
             (correct_ip >> 8) & 0xFF,
             correct_ip & 0xFF);

    printf(CONSOLE_ESC(1C)"Switch IP: " CONSOLE_ESC(38;5;226m));
    printf("%s:8080\n", ip_str);
    printf(CONSOLE_ESC(38;5;255m));

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        printf("Failed to create socket.\n");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(8080);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Failed to bind socket.\n");
        close(server_socket);
        return 1;
    }
    if (listen(server_socket, 5) < 0) {
        printf("Failed to listen on socket.\n");
        close(server_socket);
        return 1;
    }

    printf(CONSOLE_ESC(1C) "Server is now running\n");
    consoleUpdate(NULL);

    // Accept incoming connections and handle them
    while (!shutdown_requested) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            printf("Failed to accept connection.\n");
            continue;
        }

        handle_http_request(client_socket);
    }

    // Cleanup
    printf(CONSOLE_ESC(1C) "Shutting down server\n");
    consoleUpdate(NULL);
    close(server_socket);
    socketExit();
    return 0;
}