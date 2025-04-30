#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define FILE_DIR "/app/client_files"
#define KEY_SIZE 32
#define IV_SIZE 16
#define MAX_USERNAME 32
#define MAX_PASSWORD 32

unsigned char key[KEY_SIZE];
unsigned char iv[IV_SIZE];

void encrypt_data(unsigned char *plaintext, int plaintext_len, unsigned char *ciphertext, int *ciphertext_len) {
    AES_KEY aes_key;
    AES_set_encrypt_key(key, 256, &aes_key);
    unsigned char local_iv[IV_SIZE];
    memcpy(local_iv, iv, IV_SIZE);
    AES_cbc_encrypt(plaintext, ciphertext, plaintext_len, &aes_key, local_iv, AES_ENCRYPT);
    *ciphertext_len = plaintext_len;
}

void decrypt_data(unsigned char *ciphertext, int ciphertext_len, unsigned char *plaintext, int *plaintext_len) {
    AES_KEY aes_key;
    AES_set_decrypt_key(key, 256, &aes_key);
    unsigned char local_iv[IV_SIZE];
    memcpy(local_iv, iv, IV_SIZE);
    AES_cbc_encrypt(ciphertext, plaintext, ciphertext_len, &aes_key, local_iv, AES_DECRYPT);
    *plaintext_len = ciphertext_len;
}

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    unsigned char plaintext[BUFFER_SIZE];
    unsigned char ciphertext[BUFFER_SIZE];
    int valread, plaintext_len, ciphertext_len;

    // Create file directory
    mkdir(FILE_DIR, 0755);

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "server", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connect
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Receive encryption key and IV
    valread = read(sock, (char*)key, KEY_SIZE);
    if (valread != KEY_SIZE) {
        printf("Failed to receive key\n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    valread = read(sock, (char*)iv, IV_SIZE);
    if (valread != IV_SIZE) {
        printf("Failed to receive IV\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Authenticate
    char username[MAX_USERNAME], password[MAX_PASSWORD];
    printf("Enter username: ");
    scanf("%31s", username);
    printf("Enter password: ");
    scanf("%31s", password);

    snprintf((char*)plaintext, BUFFER_SIZE, "%s:%s", username, password);
    encrypt_data(plaintext, strlen((char*)plaintext) + 1, ciphertext, &ciphertext_len);
    write(sock, (char*)ciphertext, ciphertext_len);

    valread = read(sock, buffer, BUFFER_SIZE);
    if (valread <= 0) {
        printf("Authentication response failed\n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    decrypt_data((unsigned char*)buffer, valread, plaintext, &plaintext_len);
    plaintext[plaintext_len] = '\0';
    if (strcmp((char*)plaintext, "AUTH_SUCCESS") != 0) {
        printf("Authentication failed\n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Authentication successful\n");

    while (1) {
        printf("\n1. Download file\n2. Upload file\n3. Exit\nEnter choice (1-3): ");
        int choice;
        scanf("%d", &choice);
        getchar(); // Clear newline

        if (choice == 3) break;

        char filename[256];
        printf("Enter filename: ");
        scanf("%255s", filename);

        if (choice == 1) {
            snprintf((char*)plaintext, BUFFER_SIZE, "DOWNLOAD:%s", filename);
            encrypt_data(plaintext, strlen((char*)plaintext) + 1, ciphertext, &ciphertext_len);
            write(sock, (char*)ciphertext, ciphertext_len);

            valread = read(sock, buffer, BUFFER_SIZE);
            if (valread <= 0) break;
            decrypt_data((unsigned char*)buffer, valread, plaintext, &plaintext_len);
            plaintext[plaintext_len] = '\0';

            if (strcmp((char*)plaintext, "FILE_NOT_FOUND") == 0) {
                printf("File %s not found on server\n", filename);
                continue;
            }

            char file_path[512];
            snprintf(file_path, sizeof(file_path), "%s/%s", FILE_DIR, filename);
            int file_fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (file_fd < 0) {
                printf("Failed to create file %s\n", filename);
                continue;
            }

            while (1) {
                valread = read(sock, buffer, BUFFER_SIZE);
                if (valread <= 0) break;
                decrypt_data((unsigned char*)buffer, valread, plaintext, &plaintext_len);
                if (strcmp((char*)plaintext, "END_OF_FILE") == 0) break;
                write(file_fd, plaintext, plaintext_len);
            }
            close(file_fd);
            printf("Downloaded file %s\n", filename);
        } else if (choice == 2) {
            char file_path[512];
            snprintf(file_path, sizeof(file_path), "%s/%s", FILE_DIR, filename);
            int file_fd = open(file_path, O_RDONLY);
            if (file_fd < 0) {
                printf("File %s not found locally\n", filename);
                continue;
            }

            snprintf((char*)plaintext, BUFFER_SIZE, "UPLOAD:%s", filename);
            encrypt_data(plaintext, strlen((char*)plaintext) + 1, ciphertext, &ciphertext_len);
            write(sock, (char*)ciphertext, ciphertext_len);

            valread = read(sock, buffer, BUFFER_SIZE);
            if (valread <= 0) {
                close(file_fd);
                break;
            }
            decrypt_data((unsigned char*)buffer, valread, plaintext, &plaintext_len);
            plaintext[plaintext_len] = '\0';
            if (strcmp((char*)plaintext, "UPLOAD_READY") != 0) {
                printf("Server not ready for upload\n");
                close(file_fd);
                continue;
            }

            while (1) {
                ssize_t bytes_read = read(file_fd, plaintext, BUFFER_SIZE);
                if (bytes_read <= 0) break;
                encrypt_data(plaintext, bytes_read, ciphertext, &ciphertext_len);
                write(sock, (char*)ciphertext, ciphertext_len);
            }
            close(file_fd);
            char *eof = "END_OF_FILE";
            encrypt_data((unsigned char*)eof, strlen(eof) + 1, ciphertext, &ciphertext_len);
            write(sock, (char*)ciphertext, ciphertext_len);
            printf("Uploaded file %s\n", filename);
        } else {
            printf("Invalid choice\n");
        }
    }

    close(sock);
    printf("Disconnected from server\n");
    return 0;
}