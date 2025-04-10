import socket

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
host = '0.0.0.0'  # Accept connections from any IP
port = 12345

server_socket.bind((host, port))
server_socket.listen(1)
print(f"[SERVER] Listening on {host}:{port}...")

client_socket, addr = server_socket.accept()
print(f"[SERVER] Connected to client at {addr}")

while True:
    data = client_socket.recv(1024).decode()
    if not data or data.lower() == 'exit':
        print("[SERVER] Connection closed.")
        break
    print(f"[CLIENT] {data}")
    reply = input("[YOU] Enter response: ")
    client_socket.send(reply.encode())

client_socket.close()
server_socket.close()
