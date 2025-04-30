import socket
import os
import json
from cryptography.fernet import Fernet

# Client configuration
HOST = 'server'  # Docker service name
PORT = 8080
BUFFER_SIZE = 4096
FILE_DIR = '/app/client_files'

# Ensure file directory exists
if not os.path.exists(FILE_DIR):
    os.makedirs(FILE_DIR)

def main():
    # Create socket
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client.connect((HOST, PORT))

    # Receive encryption key
    key = client.recv(BUFFER_SIZE)
    cipher = Fernet(key)

    # Authenticate
    username = input("Enter username: ")
    password = input("Enter password: ")
    auth_data = json.dumps({'username': username, 'password': password}).encode()
    client.send(cipher.encrypt(auth_data))

    # Check authentication result
    auth_response = client.recv(BUFFER_SIZE)
    auth_response = cipher.decrypt(auth_response).decode()
    if auth_response != 'AUTH_SUCCESS':
        print("Authentication failed")
        client.close()
        return

    print("Authentication successful")

    while True:
        print("\nOptions: 1. Download file  2. Upload file  3. Exit")
        choice = input("Enter choice (1-3): ")

        if choice == '1':
            filename = input("Enter filename to download: ")
            command = json.dumps({'action': 'DOWNLOAD', 'filename': filename}).encode()
            client.send(cipher.encrypt(command))

            # Check if file exists
            response = client.recv(BUFFER_SIZE)
            response = cipher.decrypt(response).decode()
            if response == 'FILE_NOT_FOUND':
                print(f"File {filename} not found on server")
                continue

            # Receive file
            file_path = os.path.join(FILE_DIR, filename)
            with open(file_path, 'wb') as f:
                while True:
                    data = client.recv(BUFFER_SIZE)
                    if not data:
                        break
                    decrypted_data = cipher.decrypt(data)
                    if decrypted_data == b'END_OF_FILE':
                        break
                    f.write(decrypted_data)
            print(f"Downloaded file {filename}")

        elif choice == '2':
            filename = input("Enter filename to upload: ")
            file_path = os.path.join(FILE_DIR, filename)
            if not os.path.exists(file_path):
                print(f"File {filename} not found locally")
                continue

            command = json.dumps({'action': 'UPLOAD', 'filename': filename}).encode()
            client.send(cipher.encrypt(command))

            # Wait for server readiness
            response = client.recv(BUFFER_SIZE)
            response = cipher.decrypt(response).decode()
            if response != 'UPLOAD_READY':
                print("Server not ready for upload")
                continue

            # Send file
            with open(file_path, 'rb') as f:
                while True:
                    bytes_read = f.read(BUFFER_SIZE)
                    if not bytes_read:
                        break
                    encrypted_data = cipher.encrypt(bytes_read)
                    client.send(encrypted_data)
            client.send(cipher.encrypt(b'END_OF_FILE'))
            print(f"Uploaded file {filename}")

        elif choice == '3':
            break

        else:
            print("Invalid choice")

    client.close()
    print("Disconnected from server")

if __name__ == '__main__':
    main()
