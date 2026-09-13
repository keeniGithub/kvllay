#ifndef KVLLAY_SERVER_HPP
#define KVLLAY_SERVER_HPP

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cstring>
#include <constants.hpp>
#include <resp.hpp>
#include <store.hpp>
#include <snapshot.hpp>
#include <aof.hpp>
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

struct ServerConfig {
    int port = constants::DEFAULT_PORT;
    std::string host = constants::DEFAULT_HOST;
    std::string password = "";

    bool snapshot_enabled = true;
    std::string snapshot_path = constants::DEFAULT_SNAPSHOT_FILE;
    uint64_t save_interval_secs = constants::DEFAULT_SAVE_INTERVAL_SECS;
    uint64_t save_changes = constants::DEFAULT_SAVE_CHANGES;

    bool aof_enabled = false;
    std::string aof_path = constants::DEFAULT_AOF_FILE;
    FsyncPolicy aof_fsync_policy = FsyncPolicy::EverySec;

    size_t maxmemory = constants::DEFAULT_MAXMEMORY;
    constants::MaxmemoryPolicy maxmemory_policy = constants::MaxmemoryPolicy::NoEviction;
};

class Server {
public:
    explicit Server(int port = constants::DEFAULT_PORT, std::string host = constants::DEFAULT_HOST, std::string password = "")
        : Server(ServerConfig{port, std::move(host), std::move(password)}) {}

    explicit Server(ServerConfig config)
        : config_(std::move(config)),
          running_(false),
          server_socket_(INVALID_SOCKET),
          store_(config_.maxmemory, config_.maxmemory_policy),
          snapshot_mgr_(config_.snapshot_path, config_.save_interval_secs, config_.save_changes),
          aof_mgr_(config_.aof_path, config_.aof_enabled, config_.aof_fsync_policy),
          command_handler_(store_, config_.snapshot_enabled ? &snapshot_mgr_ : nullptr, config_.aof_enabled ? &aof_mgr_ : nullptr) {
        init_network();
    }

    ~Server() {
        stop();
        cleanup_network();
    }

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    bool start() {
        if (config_.snapshot_enabled) {
            if (snapshot_mgr_.load(store_)) {
                std::cout << "[kvllay] Snapshot loaded from " << config_.snapshot_path << std::endl;
            }
        }

        if (config_.aof_enabled) {
            aof_mgr_.load(store_);
            if (!aof_mgr_.start()) {
                std::cerr << "[kvllay] Warning: Failed to start AOF background writer" << std::endl;
            } else {
                std::cout << "[kvllay] AOF persistence enabled (" << config_.aof_path << ", fsync: " 
                          << fsync_policy_to_string(config_.aof_fsync_policy) << ")" << std::endl;
            }
        }

        if (config_.maxmemory > 0) {
            std::cout << "[kvllay] Maxmemory limit: " << constants::format_memory_human(config_.maxmemory)
                      << " (policy: " << constants::maxmemory_policy_to_string(config_.maxmemory_policy) << ")" << std::endl;
        }

        if (config_.snapshot_enabled && config_.save_interval_secs > 0) {
            snapshot_mgr_.start_auto_save(store_);
            std::cout << "[kvllay] Auto-save enabled (every " << config_.save_interval_secs << "s if >= " 
                      << config_.save_changes << " changes)" << std::endl;
        }

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
        server_addr.sin_port = htons(static_cast<uint16_t>(config_.port));
        if (config_.host == "0.0.0.0" || config_.host.empty()) {
            server_addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            inet_pton(AF_INET, config_.host.c_str(), &server_addr.sin_addr);
        }

        if (bind(server_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "[kvllay] Failed to bind to " << config_.host << ":" << config_.port << std::endl;
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
        std::cout << "[kvllay] Server started on " << config_.host << ":" << config_.port;
        if (!config_.password.empty()) {
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
            int bufsize = 262144;
            setsockopt(client_socket, SOL_SOCKET, SO_RCVBUF, (const char*)&bufsize, sizeof(bufsize));
            setsockopt(client_socket, SOL_SOCKET, SO_SNDBUF, (const char*)&bufsize, sizeof(bufsize));
#else
            setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
            int bufsize = 262144;
            setsockopt(client_socket, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
            setsockopt(client_socket, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
#endif

            std::thread client_thread(&Server::handle_client, this, client_socket);
            client_thread.detach();
        }
    }

    void stop() {
        if (!running_.exchange(false)) {
            return;
        }

        if (IS_VALID_SOCKET(server_socket_)) {
            CLOSE_SOCKET(server_socket_);
            server_socket_ = INVALID_SOCKET;
        }

        if (config_.snapshot_enabled) {
            snapshot_mgr_.stop_auto_save();
            if (store_.dirty_count() > 0) {
                snapshot_mgr_.save_sync(store_);
            }
        }

        if (config_.aof_enabled) {
            aof_mgr_.stop();
        }
    }

    Store& store() {
        return store_;
    }

    SnapshotManager& snapshot_manager() {
        return snapshot_mgr_;
    }

    AofManager& aof_manager() {
        return aof_mgr_;
    }

    const ServerConfig& config() const {
        return config_;
    }

private:
    ServerConfig config_;
    std::atomic<bool> running_;
    socket_t server_socket_;
    Store store_;
    SnapshotManager snapshot_mgr_;
    AofManager aof_mgr_;
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
        constexpr size_t BUFFER_SIZE = constants::CLIENT_BUFFER_SIZE;
        std::vector<char> raw_buffer(BUFFER_SIZE);
        char* buffer = raw_buffer.data();
        std::string client_buffer;
        client_buffer.reserve(BUFFER_SIZE * 2);
        size_t read_offset = 0;
        bool authenticated = config_.password.empty();

        std::vector<std::string_view> args;
        args.reserve(16);
        std::string unescape_buf;
        std::string out_batch;
        out_batch.reserve(BUFFER_SIZE * 2);

        while (running_) {
            if (read_offset > 0) {
                if (read_offset >= client_buffer.size()) {
                    client_buffer.clear();
                    read_offset = 0;
                } else if (read_offset >= BUFFER_SIZE || client_buffer.size() > BUFFER_SIZE * 4) {
                    client_buffer.erase(0, read_offset);
                    read_offset = 0;
                }
            }

            int bytes_read = recv(client_socket, buffer, static_cast<int>(BUFFER_SIZE), 0);
            if (bytes_read <= 0) {
                break;
            }

            client_buffer.append(buffer, bytes_read);
            out_batch.clear();

            while (read_offset < client_buffer.size()) {
                size_t consumed = 0;
                std::string_view sv(client_buffer.data() + read_offset, client_buffer.size() - read_offset);
                ParseStatus status = Resp::parse_command(sv, args, consumed, unescape_buf);

                if (status == ParseStatus::Success) {
                    read_offset += consumed;
                    bool should_close = false;
                    command_handler_.dispatch(args, out_batch, authenticated, config_.password, should_close);

                    if (should_close) {
                        if (!out_batch.empty()) {
                            send_all(client_socket, out_batch);
                        }
                        CLOSE_SOCKET(client_socket);
                        return;
                    }
                } else if (status == ParseStatus::Incomplete) {
                    break;
                } else {
                    Resp::append_error(out_batch, "Protocol error");
                    send_all(client_socket, out_batch);
                    CLOSE_SOCKET(client_socket);
                    return;
                }
            }

            if (!out_batch.empty()) {
                if (!send_all(client_socket, out_batch)) {
                    break;
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

}

#endif // KVLLAY_SERVER_HPP
