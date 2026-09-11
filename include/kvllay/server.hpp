#ifndef KVLLAY_SERVER_HPP
#define KVLLAY_SERVER_HPP

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cstring>
#include <resp.hpp>
#include <store.hpp>
#include <commands.hpp>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    #define IS_VALID_SOCKET(s) ((s) != INVALID_SOCKET)
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    using socket_t = int;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR   (-1)
    #define IS_VALID_SOCKET(s) ((s) >= 0)
    #define CLOSE_SOCKET(s) ::close(s)
#endif

namespace kvllay {

class Server {
public:
    explicit Server(int port = 6379, std::string host = "0.0.0.0", std::string password = "")
        : port_(port), host_(std::move(host)), password_(std::move(password)),
          running_(false), server_socket_(INVALID_SOCKET), command_handler_(store_) {
        init_network();
    }

    ~Server() {
        stop();
        cleanup_network();
    }

    // Non-copyable
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    bool start() {
        server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (!IS_VALID_SOCKET(server_socket_)) {
            std::cerr << "[kvllay] Failed to create socket" << std::endl;
            return false;
        }

        int opt = 1;
#ifdef _WIN32
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(static_cast<uint16_t>(port_));
        if (host_ == "0.0.0.0" || host_.empty()) {
            server_addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr);
        }

        if (bind(server_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "[kvllay] Failed to bind to " << host_ << ":" << port_ << std::endl;
            CLOSE_SOCKET(server_socket_);
            server_socket_ = INVALID_SOCKET;
            return false;
        }

        if (listen(server_socket_, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "[kvllay] Failed to listen on socket" << std::endl;
            CLOSE_SOCKET(server_socket_);
            server_socket_ = INVALID_SOCKET;
            return false;
        }

        running_ = true;
        std::cout << "[kvllay] Server started on " << host_ << ":" << port_;
        if (!password_.empty()) {
            std::cout << " (password protected)";
        }
        std::cout << std::endl;
        return true;
    }

    void run() {
        if (!running_ && !start()) {
            return;
        }

        while (running_) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            socket_t client_socket = accept(server_socket_, (sockaddr*)&client_addr, &client_len);

            if (!IS_VALID_SOCKET(client_socket)) {
                if (running_) {
                    std::cerr << "[kvllay] Failed to accept client connection" << std::endl;
                }
                continue;
            }

            int nodelay = 1;
#ifdef _WIN32
            setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
#else
            setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
#endif

            std::thread client_thread(&Server::handle_client, this, client_socket);
            client_thread.detach();
        }
    }

    void stop() {
        running_ = false;
        if (IS_VALID_SOCKET(server_socket_)) {
            CLOSE_SOCKET(server_socket_);
            server_socket_ = INVALID_SOCKET;
        }
    }

    Store& store() {
        return store_;
    }

private:
    int port_;
    std::string host_;
    std::string password_;
    std::atomic<bool> running_;
    socket_t server_socket_;
    Store store_;
    CommandHandler command_handler_;

    void init_network() {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }

    void cleanup_network() {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void handle_client(socket_t client_socket) {
        constexpr size_t BUFFER_SIZE = 4096;
        char buffer[BUFFER_SIZE];
        std::string client_buffer;
        bool authenticated = password_.empty();

        while (running_) {
            int bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_read <= 0) {
                break; // Connection closed or error
            }

            client_buffer.append(buffer, bytes_read);

            while (!client_buffer.empty()) {
                std::vector<std::string> args;
                size_t consumed = 0;
                ParseStatus status = Resp::parse_command(client_buffer, args, consumed);

                if (status == ParseStatus::Success) {
                    client_buffer.erase(0, consumed);
                    CommandResult result = command_handler_.dispatch(args, authenticated, password_);
                    
                    if (!result.response.empty()) {
                        send_all(client_socket, result.response);
                    }

                    if (result.should_close) {
                        CLOSE_SOCKET(client_socket);
                        return;
                    }
                } else if (status == ParseStatus::Incomplete) {
                    // Wait for more data from socket
                    break;
                } else { // ParseStatus::Error
                    std::string err = Resp::error("Protocol error");
                    send_all(client_socket, err);
                    CLOSE_SOCKET(client_socket);
                    return;
                }
            }
        }

        CLOSE_SOCKET(client_socket);
    }

    bool send_all(socket_t socket, const std::string& data) {
        size_t total_sent = 0;
        size_t to_send = data.size();

        while (total_sent < to_send) {
            int sent = send(socket, data.c_str() + total_sent, static_cast<int>(to_send - total_sent), 0);
            if (sent <= 0) {
                return false;
            }
            total_sent += sent;
        }
        return true;
    }
};

} // namespace kvllay

#endif // KVLLAY_SERVER_HPP
