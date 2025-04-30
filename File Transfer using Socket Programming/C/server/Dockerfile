FROM ubuntu:20.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY server.c .

# Compile the server
RUN gcc server.c -o server -lssl -lcrypto

CMD ["./server"]