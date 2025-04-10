#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    char message[1024];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);

    printf("Server waiting for connection...\n");
    fflush(stdout);

    new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    printf("Client connected!\n");
    fflush(stdout);

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        read(new_socket, buffer, 1024);
        printf("Client: %s", buffer);
        fflush(stdout);

        if (strncmp(buffer, "exit", 4) == 0) break;

        printf("Server: ");
        fflush(stdout);
        fgets(message, sizeof(message), stdin);
        send(new_socket, message, strlen(message), 0);

        if (strncmp(message, "exit", 4) == 0) break;
    }

    close(new_socket);
    close(server_fd);
    return 0;
}
