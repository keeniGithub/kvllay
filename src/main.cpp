#include <kvllay/kvllay.hpp>
#include <iostream>
#include <string>
#include <vector>

void print_help(const char* prog) {
    std::cout << "kvllay - In-memory key-value store (Redis RESP compatible)\n\n"
              << "Usage: " << prog << " [options] [port] [host]\n\n"
              << "Options:\n"
              << "  -p, --port <port>          Port to listen on (default: 6379)\n"
              << "  -h, --bind, --host <host>  Host address to bind (default: 0.0.0.0)\n"
              << "  -a, --requirepass <pass>   Require password authentication\n"
              << "  --help                     Display this help message\n\n"
              << "Examples:\n"
              << "  " << prog << " -p 6379\n"
              << "  " << prog << " -p 6379 -h 127.0.0.1\n"
              << "  " << prog << " -p 6379 -a \"mysecretpassword\"\n";
}

int main(int argc, char* argv[]) {
    int port = 6379;
    std::string host = "0.0.0.0";
    std::string password = "";

    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if ((arg == "-h" || arg == "--bind" || arg == "--host") && i + 1 < argc) {
            host = argv[++i];
        } else if ((arg == "-a" || arg == "--requirepass" || arg == "--password") && i + 1 < argc) {
            password = argv[++i];
        } else if (arg.rfind("-", 0) != 0) {
            positional.push_back(arg);
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_help(argv[0]);
            return 1;
        }
    }

    if (positional.size() >= 1) {
        try {
            port = std::stoi(positional[0]);
        } catch (...) {
            std::cerr << "Invalid port: " << positional[0] << std::endl;
            return 1;
        }
    }
    if (positional.size() >= 2) {
        host = positional[1];
    }
    if (positional.size() >= 3) {
        password = positional[2];
    }

    kvllay::Server server(port, host, password);
    server.run();

    return 0;
}
