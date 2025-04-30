FROM ubuntu:20.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY client.c .

# Compile the client
RUN gcc client.c -o client -lssl -lcrypto

CMD ["./client"]