#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <process.h> // For _beginthreadex

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define BUFFER_SIZE 1024

// Function to read the contents of a file
char* read_file(const char* filename, long* file_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* content = (char*)malloc(*file_size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }

    fread(content, 1, *file_size, file);
    content[*file_size] = '\0';

    fclose(file);
    return content;
}

// Function to determine the Content-Type based on the file extension
const char* get_content_type(const char* path) {
    const char* ext = strrchr(path, '.');
    if (ext) {
        if (strcmp(ext, ".html") == 0) return "text/html";
        if (strcmp(ext, ".css") == 0) return "text/css";
        if (strcmp(ext, ".js") == 0) return "application/javascript";
        if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
        if (strcmp(ext, ".png") == 0) return "image/png";
        if (strcmp(ext, ".gif") == 0) return "image/gif";
        if (strcmp(ext, ".pdf") == 0) return "application/pdf";
        if (strcmp(ext, ".json") == 0) return "application/json";
        if (strcmp(ext, ".txt") == 0) return "text/plain";
        if (strcmp(ext, ".xml") == 0) return "application/xml";
    }
    return "application/octet-stream"; // Default MIME type for unknown file types
}

// Thread function to handle client requests
unsigned __stdcall handle_client(void* socket) {
    SOCKET new_socket = (SOCKET)socket;
    char buffer[BUFFER_SIZE] = {0};

    // Read client request
    recv(new_socket, buffer, BUFFER_SIZE, 0);
    printf("Received request:\n%s\n", buffer);

    // Parse the request to get the file path
    char* path = strtok(buffer, " ");
    path = strtok(NULL, " ");

    if (!path) {
        const char* bad_request_response = "HTTP/1.1 400 Bad Request\nContent-Type: text/plain\nContent-Length: 15\n\n400 Bad Request";
        send(new_socket, bad_request_response, strlen(bad_request_response), 0);
        closesocket(new_socket);
        return 0;
    }

    // Construct the file path
    char file_path[256];
    if (strcmp(path, "/") == 0) {
        strcpy(file_path, "index.html");
    } else {
        snprintf(file_path, sizeof(file_path), ".%s", path);
    }

    // Read the requested file
    long file_size;
    char* file_content = read_file(file_path, &file_size);
    if (!file_content) {
        const char* not_found_response = "HTTP/1.1 404 Not Found\nContent-Type: text/plain\nContent-Length: 13\n\n404 Not Found";
        send(new_socket, not_found_response, strlen(not_found_response), 0);
    } else {
        const char* content_type = get_content_type(file_path);
        char header[256];
        snprintf(header, sizeof(header), "HTTP/1.1 200 OK\nContent-Type: %s\nContent-Length: %ld\n\n", content_type, file_size);

        // Send the HTTP header and file content
        send(new_socket, header, strlen(header), 0);
        send(new_socket, file_content, file_size, 0);

        free(file_content);
    }

    // Close the socket for this client
    closesocket(new_socket);
    return 0;
}

int main() {
    WSADATA wsaData;
    SOCKET server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        return 1;
    }

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed.\n");
        WSACleanup();
        return 1;
    }

    // Set SO_REUSEADDR option
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        printf("setsockopt failed.\n");
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    // Set up address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        printf("Bind failed.\n");
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    // Listen for connections
    if (listen(server_fd, 3) == SOCKET_ERROR) {
        printf("Listen failed.\n");
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    printf("Server is listening on port %d\n", PORT);

    while (1) {
        // Accept incoming connection
        SOCKET new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket == INVALID_SOCKET) {
            printf("Accept failed.\n");
            continue;
        }

        // Create a new thread to handle the client
        _beginthreadex(NULL, 0, handle_client, (void*)new_socket, 0, NULL);
    }

    // Close the server socket
    closesocket(server_fd);
    WSACleanup();
    return 0;
}