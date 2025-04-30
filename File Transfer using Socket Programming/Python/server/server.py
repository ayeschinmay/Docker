import socket
import os
import json
from cryptography.fernet import Fernet
import threading
import hashlib

# Server configuration
HOST = '0.0.0.0'  # Listen on all interfaces
PORT = 8080
BUFFER_SIZE = 4096
FILE_DIR = '/app/server_files'
KEY = Fernet.generate_key()  # Generate encryption key
CIPHER = Fernet(KEY)

# Simple user database (in production, use a secure database)
USERS = {
    'user1': hashlib.sha256('password1'.encode()).hexdigest(),
    'user2': hashlib.sha256('password2'.encode()).hexdigest()
}

# Ensure file directory exists
if not os.path.exists(FILE_DIR):
    os.makedirs(FILE_DIR)

def authenticate_client(conn):
    """Authenticate the client with username and password."""
    try:
        # Receive authentication data
        auth_data = conn.recv(BUFFER_SIZE)
        auth_data = CIPHER.decrypt(auth_data).decode()
        auth_dict = json.loads(auth_data)
        username = auth_dict.get('username')
        password = auth_dict.get('password')

        # Verify credentials
        if username in USERS and USERS[username] == hashlib.sha256(password.encode()).hexdigest():
            conn.send(CIPHER.encrypt(b'AUTH_SUCCESS'))
            return True
        else:
            conn.send(CIPHER.encrypt(b'AUTH_FAILED'))
            return False
    except Exception as e:
        print(f"Authentication error: {e}")
        conn.send(CIPHER.encrypt(b'AUTH_FAILED'))
        return False

def handle_client(conn, addr):
    """Handle client requests (download/upload)."""
    print(f"New connection from {addr}")

    # Send encryption key to client
    conn.send(KEY)

    # Authenticate client
    if not authenticate_client(conn):
        print(f"Authentication failed for {addr}")
        conn.close()
        return

    while True:
        try:
            # Receive command
            data = conn.recv(BUFFER_SIZE)
            if not data:
                break
            command = CIPHER.decrypt(data).decode()
            command_dict = json.loads(command)

            action = command_dict.get('action')
            filename = command_dict.get('filename')

            if action == 'DOWNLOAD':
                file_path = os.path.join(FILE_DIR, filename)
                if os.path.exists(file_path):
                    conn.send(CIPHER.encrypt(b'FILE_EXISTS'))
                    with open(file_path, 'rb') as f:
                        while True:
                            bytes_read = f.read(BUFFER_SIZE)
                            if not bytes_read:
                                break
                            encrypted_data = CIPHER.encrypt(bytes_read)
                            conn.send(encrypted_data)
                    conn.send(CIPHER.encrypt(b'END_OF_FILE'))
                    print(f"Sent file {filename} to {addr}")
                else:
                    conn.send(CIPHER.encrypt(b'FILE_NOT_FOUND'))
                    print(f"File {filename} not found for {addr}")

            elif action == 'UPLOAD':
                file_path = os.path.join(FILE_DIR, filename)
                conn.send(CIPHER.encrypt(b'UPLOAD_READY'))
                with open(file_path, 'wb') as f:
                    while True:
                        data = conn.recv(BUFFER_SIZE)
                        if not data:
                            break
                        decrypted_data = CIPHER.decrypt(data)
                        if decrypted_data == b'END_OF_FILE':
                            break
                        f.write(decrypted_data)
                print(f"Received file {filename} from {addr}")

            else:
                conn.send(CIPHER.encrypt(b'INVALID_COMMAND'))
        except Exception as e:
            print(f"Error handling client {addr}: {e}")
            break

    conn.close()
    print(f"Connection closed with {addr}")

def main():
    # Create socket
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((HOST, PORT))
    server.listen(5)
    print(f"Server listening on {HOST}:{PORT}")

    while True:
        conn, addr = server.accept()
        # Handle each client in a separate thread
        client_thread = threading.Thread(target=handle_client, args=(conn, addr))
        client_thread.start()

if __name__ == '__main__':
    main()
