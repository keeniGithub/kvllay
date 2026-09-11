#ifndef KVLLAY_COMMANDS_HPP
#define KVLLAY_COMMANDS_HPP

#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <resp.hpp>
#include <store.hpp>

namespace kvllay {

struct CommandResult {
    std::string response;
    bool should_close = false;
};

class CommandHandler {
public:
    explicit CommandHandler(Store& store)
        : store_(store), start_time_(std::chrono::steady_clock::now()) {}

    CommandResult dispatch(const std::vector<std::string>& args, bool& authenticated, const std::string& server_password) {
        if (args.empty()) {
            return {Resp::error("empty command"), false};
        }

        std::string cmd = args[0];
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

        // QUIT is always allowed
        if (cmd == "QUIT") {
            return {Resp::simple_string("OK"), true};
        }

        // AUTH command handling
        if (cmd == "AUTH") {
            return handle_auth(args, authenticated, server_password);
        }

        // Check authentication requirement
        if (!server_password.empty() && !authenticated) {
            return {Resp::error("NOAUTH Authentication required."), false};
        }

        if (cmd == "PING") {
            return handle_ping(args);
        } else if (cmd == "SET") {
            return handle_set(args);
        } else if (cmd == "GET") {
            return handle_get(args);
        } else if (cmd == "DEL") {
            return handle_del(args);
        } else if (cmd == "EXISTS") {
            return handle_exists(args);
        } else if (cmd == "KEYS") {
            return handle_keys(args);
        } else if (cmd == "FLUSHDB" || cmd == "FLUSHALL") {
            return handle_flushdb(args);
        } else if (cmd == "DBSIZE") {
            return handle_dbsize(args);
        } else if (cmd == "ECHO") {
            return handle_echo(args);
        } else if (cmd == "COMMAND") {
            return handle_command(args);
        } else if (cmd == "INFO") {
            return handle_info(args);
        }

        return {Resp::error("unknown command '" + args[0] + "'"), false};
    }

private:
    Store& store_;
    std::chrono::steady_clock::time_point start_time_;

    CommandResult handle_auth(const std::vector<std::string>& args, bool& authenticated, const std::string& server_password) {
        if (args.size() < 2 || args.size() > 3) {
            return {Resp::error("wrong number of arguments for 'auth' command"), false};
        }

        if (server_password.empty()) {
            // No password configured on server
            authenticated = true;
            return {Resp::simple_string("OK"), false};
        }

        // AUTH [username] password
        std::string provided_password = (args.size() == 2) ? args[1] : args[2];
        if (provided_password == server_password) {
            authenticated = true;
            return {Resp::simple_string("OK"), false};
        }

        return {Resp::error("WRONGPASS invalid username-password pair or user is disabled."), false};
    }

    CommandResult handle_ping(const std::vector<std::string>& args) {
        if (args.size() == 1) {
            return {Resp::simple_string("PONG"), false};
        } else if (args.size() == 2) {
            return {Resp::bulk_string(args[1]), false};
        }
        return {Resp::error("wrong number of arguments for 'ping' command"), false};
    }

    CommandResult handle_set(const std::vector<std::string>& args) {
        if (args.size() < 3) {
            return {Resp::error("wrong number of arguments for 'set' command"), false};
        }
        store_.set(args[1], args[2]);
        return {Resp::simple_string("OK"), false};
    }

    CommandResult handle_get(const std::vector<std::string>& args) {
        if (args.size() != 2) {
            return {Resp::error("wrong number of arguments for 'get' command"), false};
        }
        auto val = store_.get(args[1]);
        if (val.has_value()) {
            return {Resp::bulk_string(*val), false};
        }
        return {Resp::null_bulk_string(), false};
    }

    CommandResult handle_del(const std::vector<std::string>& args) {
        if (args.size() < 2) {
            return {Resp::error("wrong number of arguments for 'del' command"), false};
        }
        std::vector<std::string> keys_to_del(args.begin() + 1, args.end());
        size_t count = store_.del(keys_to_del);
        return {Resp::integer(count), false};
    }

    CommandResult handle_exists(const std::vector<std::string>& args) {
        if (args.size() < 2) {
            return {Resp::error("wrong number of arguments for 'exists' command"), false};
        }
        std::vector<std::string> keys_to_check(args.begin() + 1, args.end());
        size_t count = store_.exists(keys_to_check);
        return {Resp::integer(count), false};
    }

    CommandResult handle_keys(const std::vector<std::string>& args) {
        std::string pattern = "*";
        if (args.size() >= 2) {
            pattern = args[1];
        }
        auto matched = store_.keys(pattern);
        return {Resp::array(matched), false};
    }

    CommandResult handle_flushdb(const std::vector<std::string>& /*args*/) {
        store_.flushdb();
        return {Resp::simple_string("OK"), false};
    }

    CommandResult handle_dbsize(const std::vector<std::string>& /*args*/) {
        return {Resp::integer(store_.size()), false};
    }

    CommandResult handle_echo(const std::vector<std::string>& args) {
        if (args.size() != 2) {
            return {Resp::error("wrong number of arguments for 'echo' command"), false};
        }
        return {Resp::bulk_string(args[1]), false};
    }

    CommandResult handle_command(const std::vector<std::string>& /*args*/) {
        // Return empty array to satisfy redis-cli command discovery
        return {Resp::empty_array(), false};
    }

    CommandResult handle_info(const std::vector<std::string>& /*args*/) {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();

        std::string info;
        info += "# Server\r\n";
        info += "redis_version:kvllay-1.0.0\r\n";
        info += "kvllay_version:1.0.0\r\n";
        info += "uptime_in_seconds:" + std::to_string(uptime) + "\r\n";
        info += "# Keyspace\r\n";
        info += "db0:keys=" + std::to_string(store_.size()) + ",expires=0\r\n";

        return {Resp::bulk_string(info), false};
    }
};

} // namespace kvllay

#endif // KVLLAY_COMMANDS_HPP
