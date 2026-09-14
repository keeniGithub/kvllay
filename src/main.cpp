#include <kvllay/kvllay.hpp>
#include <iostream>
#include <string>
#include <vector>

void print_version() {
    std::cout << kvllay::constants::SERVER_NAME << " version " << kvllay::constants::VERSION << "\n";
}

void print_help(const char* prog) {
    std::cout << kvllay::constants::SERVER_NAME << " v" << kvllay::constants::VERSION
              << " - In-memory key-value store (Redis RESP compatible)\n\n"
              << "Usage: " << prog << " [options] [port] [host]\n\n"
              << "Options:\n"
              << "  -p, --port <port>              Port to listen on (default: " << kvllay::constants::DEFAULT_PORT << ")\n"
              << "  -h, --bind, --host <host>      Host address to bind (default: " << kvllay::constants::DEFAULT_HOST << ")\n"
              << "  -a, --requirepass <pass>       Require password authentication\n"
              << "  --save <secs> [changes]        Auto-save snapshot every <secs> if [changes] occur\n"
              << "  --snapshot, --save-file <file> Snapshot file path (default: " << kvllay::constants::DEFAULT_SNAPSHOT_FILE << ")\n"
              << "  --no-snapshot                  Disable snapshot saving\n"
              << "  --aof [file]                   Enable Append-Only Log persistence (default: " << kvllay::constants::DEFAULT_AOF_FILE << ")\n"
              << "  --appendfsync <policy>         AOF fsync policy: always, everysec, no (default: everysec)\n"
              << "  --maxmemory <bytes|mb>         Max memory limit (e.g. 512mb, 1gb, 0=unlimited)\n"
              << "  --maxmemory-policy <policy>    Eviction policy: noeviction, allkeys-lru, volatile-lru, allkeys-random, volatile-ttl\n"
              << "  --threads, --io-threads <n>    Number of worker event loop threads (default: auto)\n"
              << "  -v, --version                  Display version information\n"
              << "  --help                         Display this help message\n\n"
              << "Examples:\n"
              << "  " << prog << " -p 6379\n"
              << "  " << prog << " -p 6379 --save 60\n"
              << "  " << prog << " -p 6379 --aof kvllay.aof --appendfsync everysec\n"
              << "  " << prog << " -p 6379 --maxmemory 100mb --maxmemory-policy allkeys-lru\n"
              << "  " << prog << " -p 6379 --snapshot dump.kvl --aof\n";
}

int main(int argc, char* argv[]) {
    kvllay::ServerConfig config;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            print_version();
            return 0;
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
        } else if ((arg == "-h" || arg == "--bind" || arg == "--host") && i + 1 < argc) {
            config.host = argv[++i];
        } else if ((arg == "-a" || arg == "--requirepass" || arg == "--password") && i + 1 < argc) {
            config.password = argv[++i];
        } else if (arg == "--save" && i + 1 < argc) {
            config.save_interval_secs = std::stoull(argv[++i]);
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                try {
                    config.save_changes = std::stoull(argv[i + 1]);
                    i++;
                } catch (...) {}
            }
        } else if ((arg == "--snapshot" || arg == "--save-file") && i + 1 < argc) {
            config.snapshot_path = argv[++i];
            config.snapshot_enabled = true;
        } else if (arg == "--no-snapshot" || arg == "--no-save") {
            config.snapshot_enabled = false;
        } else if (arg == "--aof") {
            config.aof_enabled = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                config.aof_path = argv[++i];
            }
        } else if (arg == "--no-aof") {
            config.aof_enabled = false;
        } else if (arg == "--appendfsync" && i + 1 < argc) {
            config.aof_fsync_policy = kvllay::parse_fsync_policy(argv[++i]);
        } else if (arg == "--maxmemory" && i + 1 < argc) {
            if (!kvllay::constants::parse_memory_string(argv[++i], config.maxmemory)) {
                std::cerr << "Invalid maxmemory value: " << argv[i] << "\n";
                return 1;
            }
        } else if (arg == "--maxmemory-policy" && i + 1 < argc) {
            if (!kvllay::constants::parse_maxmemory_policy(argv[++i], config.maxmemory_policy)) {
                std::cerr << "Invalid maxmemory policy: " << argv[i] << "\n";
                return 1;
            }
        } else if ((arg == "--threads" || arg == "--io-threads") && i + 1 < argc) {
            try {
                config.io_threads = std::stoul(argv[++i]);
            } catch (...) {
                std::cerr << "Invalid threads value: " << argv[i] << "\n";
                return 1;
            }
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
            config.port = std::stoi(positional[0]);
        } catch (...) {
            std::cerr << "Invalid port: " << positional[0] << std::endl;
            return 1;
        }
    }
    if (positional.size() >= 2) {
        config.host = positional[1];
    }
    if (positional.size() >= 3) {
        config.password = positional[2];
    }

    kvllay::Server server(config);
    server.run();

    return 0;
}
