#ifndef KVLLAY_COMMANDS_HPP
#define KVLLAY_COMMANDS_HPP

#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <constants.hpp>
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

        if (cmd == "QUIT") {
            return {Resp::simple_string("OK"), true};
        }

        if (cmd == "AUTH") {
            return handle_auth(args, authenticated, server_password);
        }

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
        } else if (cmd == "EXPIRE") {
            return handle_expire(args);
        } else if (cmd == "PEXPIRE") {
            return handle_pexpire(args);
        } else if (cmd == "TTL") {
            return handle_ttl(args);
        } else if (cmd == "PTTL") {
            return handle_pttl(args);
        } else if (cmd == "PERSIST") {
            return handle_persist(args);
        } else if (cmd == "SETEX") {
            return handle_setex(args);
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
            authenticated = true;
            return {Resp::simple_string("OK"), false};
        }

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

    CommandResult handle_flushdb(const std::vector<std::string>&) {
        store_.flushdb();
        return {Resp::simple_string("OK"), false};
    }

    CommandResult handle_dbsize(const std::vector<std::string>&) {
        return {Resp::integer(store_.size()), false};
    }

    CommandResult handle_echo(const std::vector<std::string>& args) {
        if (args.size() != 2) {
            return {Resp::error("wrong number of arguments for 'echo' command"), false};
        }
        return {Resp::bulk_string(args[1]), false};
    }

    CommandResult handle_command(const std::vector<std::string>&) {
        return {Resp::empty_array(), false};
    }

    CommandResult handle_info(const std::vector<std::string>&) {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();

        std::string info;
        info += "# Server\r\n";
        info += "redis_version:" + constants::REDIS_VERSION_STRING + "\r\n";
        info += "kvllay_version:" + std::string(constants::VERSION) + "\r\n";
        info += "uptime_in_seconds:" + std::to_string(uptime) + "\r\n";
        info += "# Keyspace\r\n";
        info += "db0:keys=" + std::to_string(store_.size()) + ",expires=" + std::to_string(store_.expires_size()) + "\r\n";

        return {Resp::bulk_string(info), false};
    }

    CommandResult handle_expire(const std::vector<std::string>& args) {
        if (args.size() != 3) {
            return {Resp::error("wrong number of arguments for 'expire' command"), false};
        }
        long long seconds = 0;
        try {
            seconds = std::stoll(args[2]);
        } catch (...) {
            return {Resp::error("value is not an integer or out of range"), false};
        }
        if (seconds <= 0) {
            int res = store_.expire(args[1], 0);
            return {Resp::integer(res), false};
        }
        int res = store_.expire(args[1], static_cast<uint64_t>(seconds) * 1000);
        return {Resp::integer(res), false};
    }

    CommandResult handle_pexpire(const std::vector<std::string>& args) {
        if (args.size() != 3) {
            return {Resp::error("wrong number of arguments for 'pexpire' command"), false};
        }
        long long ms = 0;
        try {
            ms = std::stoll(args[2]);
        } catch (...) {
            return {Resp::error("value is not an integer or out of range"), false};
        }
        if (ms <= 0) {
            int res = store_.expire(args[1], 0);
            return {Resp::integer(res), false};
        }
        int res = store_.expire(args[1], static_cast<uint64_t>(ms));
        return {Resp::integer(res), false};
    }

    CommandResult handle_ttl(const std::vector<std::string>& args) {
        if (args.size() != 2) {
            return {Resp::error("wrong number of arguments for 'ttl' command"), false};
        }
        long long rem = store_.ttl(args[1], false);
        return {Resp::integer(rem), false};
    }

    CommandResult handle_pttl(const std::vector<std::string>& args) {
        if (args.size() != 2) {
            return {Resp::error("wrong number of arguments for 'pttl' command"), false};
        }
        long long rem = store_.ttl(args[1], true);
        return {Resp::integer(rem), false};
    }

    CommandResult handle_persist(const std::vector<std::string>& args) {
        if (args.size() != 2) {
            return {Resp::error("wrong number of arguments for 'persist' command"), false};
        }
        int res = store_.persist(args[1]);
        return {Resp::integer(res), false};
    }

    CommandResult handle_setex(const std::vector<std::string>& args) {
        if (args.size() != 4) {
            return {Resp::error("wrong number of arguments for 'setex' command"), false};
        }
        long long seconds = 0;
        try {
            seconds = std::stoll(args[2]);
        } catch (...) {
            return {Resp::error("value is not an integer or out of range"), false};
        }
        if (seconds <= 0) {
            return {Resp::error("invalid expire time in 'setex' command"), false};
        }
        store_.setex(args[1], static_cast<uint64_t>(seconds) * 1000, args[3]);
        return {Resp::simple_string("OK"), false};
    }
};

}

#endif // KVLLAY_COMMANDS_HPP
