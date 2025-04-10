import socket

client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
host = 'server'  # Docker Compose service name for the server
port = 12345

client_socket.connect((host, port))
print(f"[CLIENT] Connected to server at {host}:{port}")

while True:
    message = input("[YOU] Enter message: ")
    client_socket.send(message.encode())
    if message.lower() == 'exit':
        break
    response = client_socket.recv(1024).decode()
    print(f"[SERVER] {response}")

client_socket.close()
