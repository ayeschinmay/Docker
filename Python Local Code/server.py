import socket

# Create a socket object
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Define the host and port
host = 'localhost'
port = 12345

# Bind the socket to the host and port
server_socket.bind((host, port))

# Start listening for connections (up to 5 clients)
server_socket.listen(5)
print(f"Server listening on {host}:{port}")

# Accept a connection
client_socket, addr = server_socket.accept()
print(f"Connected to client at {addr}")

# Message loop
while True:
    message = client_socket.recv(1024).decode()
    if message.lower() == 'exit':
        print("Client disconnected.")
        break
    print("Client says:", message)

    response = input("Enter reply to client: ")
    client_socket.send(response.encode())

# Close the connection
client_socket.close()
server_socket.close()
