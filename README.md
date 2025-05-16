# C++ Multithreaded Web Server (Plain Text)

This is a simple multithreaded HTTP server written in C++ using POSIX sockets. It supports basic `GET` requests and serves plain text files (e.g., `.txt`) from the current directory.

## Features

- Handles multiple clients using a thread pool
- Responds only to `GET` requests
- Returns plain text responses
- Defaults to serving `index.txt` on root (`/`)
- Logs incoming requests to the console

## How It Works

- Listens on port `8080`
- Uses `select()` to accept incoming connections
- Pushes sockets into a thread-safe queue
- Worker threads handle each request from the queue
- If the requested file exists, it is returned with a `200 OK`
- If the file is missing, returns a simple `"Testing"` response
- Only supports `text/plain` content type

### Compile:
```bash
g++ -std=c++17 -pthread -o server main.cpp