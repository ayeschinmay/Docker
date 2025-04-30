#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define FILE_DIR "/app/server_files"
#define KEY_SIZE 32 // AES-256
#define IV_SIZE 16  // AES block size
#define MAX_USERNAME 32
#define MAX_PASSWORD 32

// Simple user database (username: SHA-256 hashed password)
typedef struct {
    char username[MAX_USERNAME];
    unsigned char password_hash[SHA256_DIGEST_LENGTH];
} User;

User users[] = {
    {"user1", {0}}, // Password: "password1"
    {"user2", {0}}  // Password: "password2"
};
int num_users = 2;

// Global encryption key and IV
unsigned char key[KEY_SIZE];
unsigned char iv[IV_SIZE];

void init_users() {
    // Initialize hashed passwords (SHA-256 of "password1" and "password2")
    SHA256((unsigned char*)"password1", strlen("password1"), users[0].password_hash);
    SHA256((unsigned char*)"password2", strlen("password2"), users[1].password_hash);
}

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

int authenticate_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int valread;

    // Receive username and password
    valread = read(client_socket, buffer, BUFFER_SIZE);
    if (valread <= 0) return 0;

    // Decrypt authentication data
    unsigned char plaintext[BUFFER_SIZE];
    int plaintext_len;
    decrypt_data((unsigned char*)buffer, valread, plaintext, &plaintext_len);
    plaintext[plaintext_len] = '\0';

    // Parse username:password
    char *username = strtok((char*)plaintext, ":");
    char *password = strtok(NULL, ":");
    if (!username || !password) return 0;

    // Compute SHA-256 hash of provided password
    unsigned char password_hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)password, strlen(password), password_hash);

    // Verify credentials
    for (int i = 0; i < num_users; i++) {
        if (strcmp(username, users[i].username) == 0 &&
            memcmp(password_hash, users[i].password_hash, SHA256_DIGEST_LENGTH) == 0) {
            char *auth_success = "AUTH_SUCCESS";
            unsigned char ciphertext[BUFFER_SIZE];
            int ciphertext_len;
            encrypt_data((unsigned char*)auth_success, strlen(auth_success) + 1, ciphertext, &ciphertext_len);
            write(client_socket, (char*)ciphertext, ciphertext_len);
            return 1;
        }
    }

    char *auth_failed = "AUTH_FAILED";
    unsigned char ciphertext[BUFFER_SIZE];
    int ciphertext_len;
    encrypt_data((unsigned char*)auth_failed, strlen(auth_failed) + 1, ciphertext, &ciphertext_len);
    write(client_socket, (char*)ciphertext, ciphertext_len);
    return 0;
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    unsigned char plaintext[BUFFER_SIZE];
    unsigned char ciphertext[BUFFER_SIZE];
    int valread, plaintext_len, ciphertext_len;

    // Send encryption key and IV
    write(client_socket, (char*)key, KEY_SIZE);
    write(client_socket, (char*)iv, IV_SIZE);

    // Authenticate client
    if (!authenticate_client(client_socket)) {
        close(client_socket);
        return;
    }

    while (1) {
        valread = read(client_socket, buffer, BUFFER_SIZE);
        if (valread <= 0) break;

        // Decrypt command
        decrypt_data((unsigned char*)buffer, valread, plaintext, &plaintext_len);
        plaintext[plaintext_len] = '\0';

        // Parse command (e.g., "DOWNLOAD:filename" or "UPLOAD:filename")
        char *action = strtok((char*)plaintext, ":");
        char *filename = strtok(NULL, ":");

        if (!action || !filename) {
            char *invalid = "INVALID_COMMAND";
            encrypt_data((unsigned char*)invalid, strlen(invalid) + 1, ciphertext, &ciphertext_len);
            write(client_socket, (char*)ciphertext, ciphertext_len);
            continue;
        }

        if (strcmp(action, "DOWNLOAD") == 0) {
            char file_path[256];
            snprintf(file_path, sizeof(file_path), "%s/%s", FILE_DIR, filename);
            int file_fd = open(file_path, O_RDONLY);
            if (file_fd >= 0) {
                char *file_exists = "FILE_EXISTS";
                encrypt_data((unsigned char*)file_exists, strlen(file_exists) + 1, ciphertext, &ciphertext_len);
                write(client_socket, (char*)ciphertext, ciphertext_len);

                while (1) {
                    ssize_t bytes_read = read(file_fd, plaintext, BUFFER_SIZE);
                    if (bytes_read <= 0) break;
                    encrypt_data(plaintext, bytes_read, ciphertext, &ciphertext_len);
                    write(client_socket, (char*)ciphertext, ciphertext_len);
                }
                close(file_fd);
                char *eof = "END_OF_FILE";
                encrypt_data((unsigned char*)eof, strlen(eof) + 1, ciphertext, &ciphertext_len);
                write(client_socket, (char*)ciphertext, ciphertext_len);
            } else {
                char *file_not_found = "FILE_NOT_FOUND";
                encrypt_data((unsigned char*)file_not_found, strlen(file_not_found) + 1, ciphertext, &ciphertext_len);
                write(client_socket, (char*)ciphertext, ciphertext_len);
            }
        } else if (strcmp(action, "UPLOAD") == 0) {
            char file_path[256];
            snprintf(file_path, sizeof(file_path), "%s/%s", FILE_DIR, filename);
            int file_fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (file_fd < 0) {
                char *error = "FILE_ERROR";
                encrypt_data((unsigned char*)error, strlen(error) + 1, ciphertext, &ciphertext_len);
                write(client_socket, (char*)ciphertext, ciphertext_len);
                continue;
            }

            char *upload_ready = "UPLOAD_READY";
            encrypt_data((unsigned char*)upload_ready, strlen(upload_ready) + 1, ciphertext, &ciphertext_len);
            write(client_socket, (char*)ciphertext, ciphertext_len);

            while (1) {
                valread = read(client_socket, buffer, BUFFER_SIZE);
                if (valread <= 0) break;
                decrypt_data((unsigned char*)buffer, valread, plaintext, &plaintext_len);
                if (strcmp((char*)plaintext, "END_OF_FILE") == 0) break;
                write(file_fd, plaintext, plaintext_len);
            }
            close(file_fd);
        } else {
            char *invalid = "INVALID_COMMAND";
            encrypt_data((unsigned char*)invalid, strlen(invalid) + 1, ciphertext, &ciphertext_len);
            write(client_socket, (char*)ciphertext, ciphertext_len);
        }
    }
    close(client_socket);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int addrlen = sizeof(client_addr);

    // Initialize user database
    init_users();

    // Generate encryption key and IV
    RAND_bytes(key, KEY_SIZE);
    RAND_bytes(iv, IV_SIZE);

    // Create file directory if it doesn't exist
    mkdir(FILE_DIR, 0755);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }
        handle_client(client_socket);
    }

    close(server_fd);
    return 0;
}