#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <vector>
#include <condition_variable>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>

constexpr int PORT = 8080;
constexpr int MAX_CLIENTS = 10;
constexpr int THREAD_POOL_SIZE = 4;

std::mutex cout_mutex;
std::mutex queue_mutex;
std::condition_variable cv;
std::queue<int> client_queue;

std::string get_mime_type(const std::string &path)
{
    return "text/plain";
}

void handle_client(int client_socket)
{
    char buffer[2048] = {0};
    read(client_socket, buffer, sizeof(buffer));

    std::istringstream request_stream(buffer);
    std::string method, path;
    request_stream >> method >> path;

    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "[REQUEST] " << method << " " << path << std::endl;
    }

    std::string response;
    if (method != "GET")
    {
        response =
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "Only GET method supported.";
    }
    else
    {
        if (path == "/")
            path = "/index.txt";
        std::string file_path = "." + path;
        std::ifstream file(file_path);

        if (!file.is_open())
        {
            response =
                "HTTP/1.1 Testing\r\n"
                "Content-Type: text/plain\r\n\r\n"
                "Testing";
        }
        else
        {
            std::ostringstream ss;
            ss << file.rdbuf();
            std::string body = ss.str();
            std::string content_type = get_mime_type(file_path);
            std::ostringstream full_response;
            full_response << "HTTP/1.1 200 OK\r\n"
                          << "Content-Type: " << content_type << "\r\n"
                          << "Content-Length: " << body.size() << "\r\n\r\n"
                          << body;
            response = full_response.str();
        }
    }

    send(client_socket, response.c_str(), response.length(), 0);
    close(client_socket);
}

void worker()
{
    while (true)
    {
        int client_socket;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait(lock, []
                    { return !client_queue.empty(); });
            client_socket = client_queue.front();
            client_queue.pop();
        }
        handle_client(client_socket);
    }
}

int main()
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("Listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server listening on port: " << PORT << "..." << std::endl;

    std::vector<std::thread> workers;
    for (int i = 0; i < THREAD_POOL_SIZE; ++i)
    {
        workers.emplace_back(worker);
    }

    while (true)
    {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0)
        {
            perror("Accept");
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            client_queue.push(new_socket);
        }
        cv.notify_one();
    }

    for (auto &th : workers)
        th.join();
    return 0;
}
