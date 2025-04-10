import socket

# Create a socket object
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Define the host and port to connect to
host = 'localhost'
port = 12345

# Connect to the server
client_socket.connect((host, port))
print(f"Connected to server at {host}:{port}")

# Message loop
while True:
    message = input("Enter message to server: ")
    client_socket.send(message.encode())
    
    if message.lower() == 'exit':
        print("Disconnected from server.")
        break

    response = client_socket.recv(1024).decode()
    print("Server says:", response)

# Close the connection
client_socket.close()
