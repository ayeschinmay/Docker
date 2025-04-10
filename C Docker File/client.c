#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT 8080

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[1024] = {0};
    char message[1024];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Socket creation error\n");
        return -1;
    }

    server = gethostbyname("server");
    if (server == NULL) {
        printf("No such host\n");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed\n");
        return -1;
    }

    printf("Connected to server!\n");
    fflush(stdout);

    while (1) {
        printf("Client: ");
        fflush(stdout);
        fgets(message, sizeof(message), stdin);
        send(sock, message, strlen(message), 0);

        if (strncmp(message, "exit", 4) == 0) break;

        memset(buffer, 0, sizeof(buffer));
        read(sock, buffer, 1024);
        printf("Server: %s", buffer);
        fflush(stdout);

        if (strncmp(buffer, "exit", 4) == 0) break;
    }

    close(sock);
    return 0;
}
