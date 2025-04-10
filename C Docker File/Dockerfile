FROM gcc:latest

WORKDIR /app

# Copy C source files
COPY server.c .
COPY client.c .

# Compile both programs
RUN gcc -o server server.c
RUN gcc -o client client.c

# Default command (can be overridden by docker-compose)
CMD ["./server"]
