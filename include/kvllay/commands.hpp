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
#include <snapshot.hpp>
#include <aof.hpp>

namespace kvllay {

struct CommandResult {
    std::string response;
    bool should_close = false;
};

class CommandHandler {
public:
    explicit CommandHandler(Store& store, SnapshotManager* snapshot_mgr = nullptr, AofManager* aof_mgr = nullptr)
        : store_(store), snapshot_mgr_(snapshot_mgr), aof_mgr_(aof_mgr),
          start_time_(std::chrono::steady_clock::now()) {}

    void set_snapshot_manager(SnapshotManager* mgr) { snapshot_mgr_ = mgr; }
    void set_aof_manager(AofManager* mgr) { aof_mgr_ = mgr; }

    static inline bool iequals(std::string_view a, std::string_view b) noexcept {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::toupper(static_cast<unsigned char>(a[i])) != b[i]) {
                return false;
            }
        }
        return true;
    }

    CommandResult dispatch(const std::vector<std::string>& args, bool& authenticated, const std::string& server_password) {
        if (args.empty()) {
            return {Resp::error("empty command"), false};
        }

        std::string_view cmd = args[0];

        if (iequals(cmd, "QUIT")) {
            return {Resp::ok(), true};
        }

        if (iequals(cmd, "AUTH")) {
            return handle_auth(args, authenticated, server_password);
        }

        if (!server_password.empty() && !authenticated) {
            return {Resp::error("NOAUTH Authentication required."), false};
        }

        CommandResult result;
        bool is_mutating = false;

        bool is_allocating = iequals(cmd, "SET") || iequals(cmd, "SETEX") || iequals(cmd, "MSET") ||
                             iequals(cmd, "LPUSH") || iequals(cmd, "RPUSH") ||
                             iequals(cmd, "INCR") || iequals(cmd, "DECR") ||
                             iequals(cmd, "INCRBY") || iequals(cmd, "DECRBY");
        if (is_allocating) {
            if (!store_.check_memory_and_evict()) {
                return {Resp::error("OOM command not allowed when used memory > 'maxmemory'."), false};
            }
        }

        if (iequals(cmd, "GET")) {
            result = handle_get(args);
        } else if (iequals(cmd, "SET")) {
            is_mutating = true;
            result = handle_set(args);
        } else if (iequals(cmd, "INCR")) {
            is_mutating = true;
            result = handle_incr(args);
        } else if (iequals(cmd, "PING")) {
            result = handle_ping(args);
        } else if (iequals(cmd, "DEL")) {
            is_mutating = true;
            result = handle_del(args);
        } else if (iequals(cmd, "EXISTS")) {
            result = handle_exists(args);
        } else if (iequals(cmd, "MGET")) {
            result = handle_mget(args);
        } else if (iequals(cmd, "MSET")) {
            is_mutating = true;
            result = handle_mset(args);
        } else if (iequals(cmd, "DECR")) {
            is_mutating = true;
            result = handle_decr(args);
        } else if (iequals(cmd, "INCRBY")) {
            is_mutating = true;
            result = handle_incrby(args);
        } else if (iequals(cmd, "DECRBY")) {
            is_mutating = true;
            result = handle_decrby(args);
        } else if (iequals(cmd, "TTL")) {
            result = handle_ttl(args);
        } else if (iequals(cmd, "PTTL")) {
            result = handle_pttl(args);
        } else if (iequals(cmd, "EXPIRE")) {
            is_mutating = true;
            result = handle_expire(args);
        } else if (iequals(cmd, "PEXPIRE")) {
            is_mutating = true;
            result = handle_pexpire(args);
        } else if (iequals(cmd, "PERSIST")) {
            is_mutating = true;
            result = handle_persist(args);
        } else if (iequals(cmd, "SETEX")) {
            is_mutating = true;
            result = handle_setex(args);
        } else if (iequals(cmd, "KEYS")) {
            result = handle_keys(args);
        } else if (iequals(cmd, "DBSIZE")) {
            result = handle_dbsize(args);
        } else if (iequals(cmd, "COMMAND")) {
            result = handle_command(args);
        } else if (iequals(cmd, "CONFIG")) {
            result = handle_config(args);
        } else if (iequals(cmd, "INFO")) {
            result = handle_info(args);
        } else if (iequals(cmd, "FLUSHDB") || iequals(cmd, "FLUSHALL")) {
            is_mutating = true;
            result = handle_flushdb(args);
        } else if (iequals(cmd, "ECHO")) {
            result = handle_echo(args);
        } else if (iequals(cmd, "SAVE")) {
            result = handle_save(args);
        } else if (iequals(cmd, "BGSAVE")) {
            result = handle_bgsave(args);
        } else if (iequals(cmd, "LASTSAVE")) {
            result = handle_lastsave(args);
        } else if (iequals(cmd, "BGREWRITEAOF")) {
            result = handle_bgrewriteaof(args);
        } else if (iequals(cmd, "LPUSH")) {
            is_mutating = true;
            result = handle_lpush(args);
        } else if (iequals(cmd, "RPUSH")) {
            is_mutating = true;
            result = handle_rpush(args);
        } else if (iequals(cmd, "LPOP")) {
            is_mutating = true;
            result = handle_lpop(args);
        } else if (iequals(cmd, "RPOP")) {
            is_mutating = true;
            result = handle_rpop(args);
        } else if (iequals(cmd, "LLEN")) {
            result = handle_llen(args);
        } else if (iequals(cmd, "LRANGE")) {
            result = handle_lrange(args);
        } else if (iequals(cmd, "LINDEX")) {
            result = handle_lindex(args);
        } else if (iequals(cmd, "TYPE")) {
            result = handle_type(args);
        } else {
            return {Resp::error("unknown command '" + std::string(cmd) + "'"), false};
        }

        if (is_mutating && aof_mgr_ && aof_mgr_->is_enabled()) {
            if (result.response.rfind("-ERR", 0) != 0 && result.response.rfind("-WRONG", 0) != 0) {
                aof_mgr_->append(args);
            }
        }

        return result;
    }

private:
    Store& store_;
    SnapshotManager* snapshot_mgr_;
    AofManager* aof_mgr_;
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
        auto res = store_.get(args[1]);
        if (res.status == Store::GetStatus::WrongType) {
            return {Resp::error("WRONGTYPE Operation against a key holding the wrong kind of value"), false};
        }
        if (res.status == Store::GetStatus::NotFound) {
            return {Resp::null_bulk_string(), false};
        }
        return {Resp::bulk_string(res.value), false};
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

    CommandResult handle_config(const std::vector<std::string>& args) {
        if (args.size() < 2) {
            return {Resp::error("wrong number of arguments for 'config' command"), false};
        }
        if (iequals(args[1], "GET")) {
            if (args.size() != 3) {
                return {Resp::error("wrong number of arguments for 'config|get' command"), false};
            }
            std::string param = args[2];
            std::transform(param.begin(), param.end(), param.begin(), ::tolower);
            if (param == "maxmemory") {
                return {Resp::array(std::vector<std::string>{"maxmemory", std::to_string(store_.maxmemory())}), false};
            } else if (param == "maxmemory-policy") {
                return {Resp::array(std::vector<std::string>{"maxmemory-policy", constants::maxmemory_policy_to_string(store_.maxmemory_policy())}), false};
            } else if (param == "*") {
                return {Resp::array(std::vector<std::string>{
                    "maxmemory", std::to_string(store_.maxmemory()),
                    "maxmemory-policy", constants::maxmemory_policy_to_string(store_.maxmemory_policy())
                }), false};
            } else {
                return {Resp::empty_array(), false};
            }
        } else if (iequals(args[1], "SET")) {
            if (args.size() != 4) {
                return {Resp::error("wrong number of arguments for 'config|set' command"), false};
            }
            std::string param = args[2];
            std::transform(param.begin(), param.end(), param.begin(), ::tolower);
            if (param == "maxmemory") {
                size_t bytes = 0;
                if (!constants::parse_memory_string(args[3], bytes)) {
                    return {Resp::error("argument must be an integer or memory string (e.g. 100mb)"), false};
                }
                store_.set_maxmemory(bytes);
                return {Resp::ok(), false};
            } else if (param == "maxmemory-policy") {
                constants::MaxmemoryPolicy policy;
                if (!constants::parse_maxmemory_policy(args[3], policy)) {
                    return {Resp::error("invalid maxmemory policy"), false};
                }
                store_.set_maxmemory_policy(policy);
                return {Resp::ok(), false};
            } else {
                return {Resp::error("Unsupported CONFIG parameter: " + args[2]), false};
            }
        } else if (iequals(args[1], "RESETSTAT")) {
            return {Resp::ok(), false};
        }
        return {Resp::error("unknown subcommand '" + args[1] + "' for 'CONFIG'"), false};
    }

    CommandResult handle_info(const std::vector<std::string>& args) {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();

        bool all = (args.size() <= 1);
        std::string section = all ? "" : args[1];
        std::transform(section.begin(), section.end(), section.begin(), ::tolower);

        std::string info;
        if (all || section == "server" || section == "default") {
            info += "# Server\r\n";
            info += "redis_version:" + constants::REDIS_VERSION_STRING + "\r\n";
            info += "kvllay_version:" + std::string(constants::VERSION) + "\r\n";
            info += "uptime_in_seconds:" + std::to_string(uptime) + "\r\n";
        }
        if (all || section == "memory" || section == "default") {
            info += "# Memory\r\n";
            info += "used_memory:" + std::to_string(store_.used_memory()) + "\r\n";
            info += "used_memory_human:" + constants::format_memory_human(store_.used_memory()) + "\r\n";
            info += "maxmemory:" + std::to_string(store_.maxmemory()) + "\r\n";
            info += "maxmemory_human:" + constants::format_memory_human(store_.maxmemory()) + "\r\n";
            info += "maxmemory_policy:" + constants::maxmemory_policy_to_string(store_.maxmemory_policy()) + "\r\n";
            info += "evicted_keys:" + std::to_string(store_.evicted_keys_count()) + "\r\n";
        }
        if (all || section == "persistence" || section == "default") {
            info += "# Persistence\r\n";
            info += "loading:0\r\n";
            info += "rdb_changes_since_last_save:" + std::to_string(store_.dirty_count()) + "\r\n";
            info += "rdb_bgsave_in_progress:" + std::to_string(snapshot_mgr_ && snapshot_mgr_->is_saving() ? 1 : 0) + "\r\n";
            info += "rdb_last_save_time:" + std::to_string(snapshot_mgr_ ? snapshot_mgr_->last_save_time() : 0) + "\r\n";
            info += "rdb_last_bgsave_status:ok\r\n";
            info += "aof_enabled:" + std::to_string(aof_mgr_ && aof_mgr_->is_enabled() ? 1 : 0) + "\r\n";
            info += "aof_rewrite_in_progress:" + std::to_string(aof_mgr_ && aof_mgr_->is_rewriting() ? 1 : 0) + "\r\n";
        }
        if (all || section == "keyspace" || section == "default") {
            info += "# Keyspace\r\n";
            info += "db0:keys=" + std::to_string(store_.size()) + ",expires=" + std::to_string(store_.expires_size()) + "\r\n";
        }

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

    CommandResult format_incr_result(Store::IncrStatus status, int64_t result) {
        if (status == Store::IncrStatus::Success) {
            return {Resp::integer(result), false};
        } else if (status == Store::IncrStatus::WrongType) {
            return {Resp::error("WRONGTYPE Operation against a key holding the wrong kind of value"), false};
        } else if (status == Store::IncrStatus::NotAnInteger) {
            return {Resp::error("value is not an integer or out of range"), false};
        } else {
            return {Resp::error("increment or decrement would overflow"), false};
        }
    }

    CommandResult handle_incr(const std::vector<std::string>& args) {
        if (args.size() != 2) {
            return {Resp::error("wrong number of arguments for 'incr' command"), false};
        }
        int64_t result = 0;
        auto status = store_.incrby(args[1], 1, result);
        return format_incr_result(status, result);
    }

    CommandResult handle_decr(const std::vector<std::string>& args) {
        if (args.size() != 2) {
            return {Resp::error("wrong number of arguments for 'decr' command"), false};
        }
        int64_t result = 0;
        auto status = store_.decrby(args[1], 1, result);
        return format_incr_result(status, result);
    }

    CommandResult handle_incrby(const std::vector<std::string>& args) {
        if (args.size() != 3) {
            return {Resp::error("wrong number of arguments for 'incrby' command"), false};
        }
        int64_t delta = 0;
        if (!Store::parse_int64(args[2], delta)) {
            return {Resp::error("value is not an integer or out of range"), false};
        }
        int64_t result = 0;
        auto status = store_.incrby(args[1], delta, result);
        return format_incr_result(status, result);
    }

    CommandResult handle_decrby(const std::vector<std::string>& args) {
        if (args.size() != 3) {
            return {Resp::error("wrong number of arguments for 'decrby' command"), false};
        }
        int64_t delta = 0;
        if (!Store::parse_int64(args[2], delta)) {
            return {Resp::error("value is not an integer or out of range"), false};
        }
        int64_t result = 0;
        auto status = store_.decrby(args[1], delta, result);
        return format_incr_result(status, result);
    }

    CommandResult handle_mget(const std::vector<std::string>& args) {
        if (args.size() < 2) {
            return {Resp::error("wrong number of arguments for 'mget' command"), false};
        }
        std::vector<std::string> keys(args.begin() + 1, args.end());
        auto values = store_.mget(keys);
        return {Resp::array_of_bulk(values), false};
    }

    CommandResult handle_mset(const std::vector<std::string>& args) {
        if (args.size() < 3 || (args.size() - 1) % 2 != 0) {
            return {Resp::error("wrong number of arguments for 'mset' command"), false};
        }
        std::vector<std::pair<std::string, std::string>> kvs;
        kvs.reserve((args.size() - 1) / 2);
        for (size_t i = 1; i < args.size(); i += 2) {
            kvs.emplace_back(args[i], args[i + 1]);
        }
        store_.mset(kvs);
        return {Resp::simple_string("OK"), false};
    }

    CommandResult handle_save(const std::vector<std::string>& args) {
        if (args.size() != 1) {
            return {Resp::error("wrong number of arguments for 'save' command"), false};
        }
        if (!snapshot_mgr_) {
            return {Resp::error("ERR snapshot manager not configured"), false};
        }
        if (!snapshot_mgr_->save_sync(store_)) {
            return {Resp::error("ERR failed to save snapshot"), false};
        }
        return {Resp::simple_string("OK"), false};
    }

    CommandResult handle_bgsave(const std::vector<std::string>& args) {
        if (args.size() > 2) {
            return {Resp::error("wrong number of arguments for 'bgsave' command"), false};
        }
        if (!snapshot_mgr_) {
            return {Resp::error("ERR snapshot manager not configured"), false};
        }
        if (snapshot_mgr_->is_saving()) {
            return {Resp::error("Background save already in progress"), false};
        }
        if (!snapshot_mgr_->save_async(store_)) {
            return {Resp::error("ERR failed to start background save"), false};
        }
        return {Resp::simple_string("Background saving started"), false};
    }

    CommandResult handle_lastsave(const std::vector<std::string>& args) {
        if (args.size() != 1) {
            return {Resp::error("wrong number of arguments for 'lastsave' command"), false};
        }
        uint64_t t = snapshot_mgr_ ? snapshot_mgr_->last_save_time() : 0;
        return {Resp::integer(static_cast<long long>(t)), false};
    }

    CommandResult handle_bgrewriteaof(const std::vector<std::string>& args) {
        if (args.size() != 1) {
            return {Resp::error("wrong number of arguments for 'bgrewriteaof' command"), false};
        }
        if (!aof_mgr_ || !aof_mgr_->is_enabled()) {
            return {Resp::error("Background append only file rewriting not enabled"), false};
        }
        if (aof_mgr_->is_rewriting()) {
            return {Resp::error("Background append only file rewriting already in progress"), false};
        }
        if (!aof_mgr_->rewrite_async(store_)) {
            return {Resp::error("ERR failed to start background AOF rewrite"), false};
        }
        return {Resp::simple_string("Background append only file rewriting started"), false};
    }

    CommandResult handle_lpush(const std::vector<std::string>& args) {
        if (args.size() < 3) {
            return {Resp::error("wrong number of arguments for 'lpush' command"), false};
        }
        std::vector<std::string> values(args.begin() + 2, args.end());
        size_t new_len = 0;
        auto status = store_.lpush(args[1], values, new_len);
        if (status == Store::ListPushStatus::WrongType) {
            return {Resp::error("WRONGTYPE Operation against a key holding the wrong kind of value"), false};
        }
        return {Resp::integer(static_cast<long long>(new_len)), false};
    }

    CommandResult handle_rpush(const std::vector<std::string>& args) {
        if (args.size() < 3) {
            return {Resp::error("wrong number of arguments for 'rpush' command"), false};
        }
        std::vector<std::string> values(args.begin() + 2, args.end());
        size_t new_len = 0;
        auto status = store_.rpush(args[1], values, new_len);
        if (status == Store::ListPushStatus::WrongType) {
            return {Resp::error("WRONGTYPE Operation against a key holding the wrong kind of value"), false};
        }
        return {Resp::integer(static_cast<long long>(new_len)), false};
    }

    CommandResult handle_lpop(const std::vector<std::string>& args) {
        if (args.size() < 2 || args.size() > 3) {
            return {Resp::error("wrong number of arguments for 'lpop' command"), false};
        }
        if (args.size() == 2) {
            std::vector<std::string> popped;
            auto status = store_.lpop(args[1], 1, popped);
            if (status == Store::ListPopStatus::WrongType) {
                return {Resp::error("WRONGTYPE Operation against a key holding the wrong kind of value"), false};
            }
            if (status == Store::ListPopStatus::NotFound || popped.empty()) {
                return {Resp::null_bulk_string(), false};
            }
            return {Resp::bulk_string(popped[0]), false};
        }

        int64_t count = 0;
        if (!Store::parse_int64(args[2], count) || count < 0) {
            return {Resp::error("value is out of range, must be positive"), false};
        }
        if (count == 0) {
            size_t len = 0;
            auto lstatus = store_.llen(args[1], len);
            if (lstatus == Store::ListLenStatus::WrongType) {
                return {Resp::error("WRONGTYPE Operation against a key holding the wrong kind of value"), false};
            }
            if (len == 0 && store_.exists({args[1]}) == 0) {
                return {Resp::null_array(), false};
            }
            return {Resp::empty_array(), false};
        }

        std::vector<std::string> popped;
        auto status = store_.lpop(args[1], static_cast<size_t>(count), popped);
        if (status == Store::ListPopStatus::WrongType) {
            return {Resp::error("WRONGTYPE Operation against a key holding the wrong kind of value"), false};
        }
        if (status == Store::ListPopStatus::NotFound) {
            return {Resp::null_array(), false};
        }
        return {Resp::array(popped), false};
    }

    CommandResult handle_rpop(const std::vector<std::string>& args) {
        if (args.size() < 2 || args.size() > 3) {
            return {Resp::error("wrong number of arguments for 'rpop' command"), false};
        }
        if (args.size() == 2) {
            std::vector<std::string> popped;
            auto status = store_.rpop(args[1], 1, popped);
            if (status == Store::ListPopStatus::WrongType) {
                return {Resp::error("WRONGTYPE Operation against a key holding the wrong kind of value"), false};
            }
            if (status == Store::ListPopStatus::NotFound || popped.empty()) {
                return {Resp::null_bulk_string(), false};
            }
            return {Resp::bulk_string(popped[0]), false};
        }

        int64_t count = 0;
        if (!Store::parse_int64(args[2], count) || count < 0) {
            return {Resp::error("value is out of range, must be positive"), false};
        }
        if (count == 0) {
            size_t len = 0;
            auto lstatus = store_.llen(args[1], len);
            if (lstatus == Store::ListLenStatus::WrongType) {
                return {Resp::error("WRONGTYPE Operation against a key holding the wrong kind of value"), false};
            }
            if (len == 0 && store_.exists({args[1]}) == 0) {
                return {Resp::null_array(), false};
            }
            return {Resp::empty_array(), false};
        }

        std::vector<std::string> popped;
        auto status = store_.rpop(args[1], static_cast<size_t>(count), popped);
        if (status == Store::ListPopStatus::WrongType) {
            return {Resp::error("WRONGTYPE Operation against a key holding the wrong kind of value"), false};
        }
        if (status == Store::ListPopStatus::NotFound) {
            return {Resp::null_array(), false};
        }
        return {Resp::array(popped), false};
    }

    CommandResult handle_llen(const std::vector<std::string>& args) {
        if (args.size() != 2) {
            return {Resp::error("wrong number of arguments for 'llen' command"), false};
        }
        size_t len = 0;
        auto status = store_.llen(args[1], len);
        if (status == Store::ListLenStatus::WrongType) {
            return {Resp::error("WRONGTYPE Operation against a key holding the wrong kind of value"), false};
        }
        return {Resp::integer(static_cast<long long>(len)), false};
    }

    CommandResult handle_lrange(const std::vector<std::string>& args) {
        if (args.size() != 4) {
            return {Resp::error("wrong number of arguments for 'lrange' command"), false};
        }
        int64_t start = 0;
        int64_t stop = 0;
        if (!Store::parse_int64(args[2], start) || !Store::parse_int64(args[3], stop)) {
            return {Resp::error("value is not an integer or out of range"), false};
        }
        std::vector<std::string> elements;
        auto status = store_.lrange(args[1], start, stop, elements);
        if (status == Store::ListRangeStatus::WrongType) {
            return {Resp::error("WRONGTYPE Operation against a key holding the wrong kind of value"), false};
        }
        return {Resp::array(elements), false};
    }

    CommandResult handle_lindex(const std::vector<std::string>& args) {
        if (args.size() != 3) {
            return {Resp::error("wrong number of arguments for 'lindex' command"), false};
        }
        int64_t index = 0;
        if (!Store::parse_int64(args[2], index)) {
            return {Resp::error("value is not an integer or out of range"), false};
        }
        std::string elem;
        auto status = store_.lindex(args[1], index, elem);
        if (status == Store::ListRangeStatus::WrongType) {
            return {Resp::error("WRONGTYPE Operation against a key holding the wrong kind of value"), false};
        }
        if (status == Store::ListRangeStatus::NotFound) {
            return {Resp::null_bulk_string(), false};
        }
        return {Resp::bulk_string(elem), false};
    }

    CommandResult handle_type(const std::vector<std::string>& args) {
        if (args.size() != 2) {
            return {Resp::error("wrong number of arguments for 'type' command"), false};
        }
        auto t = store_.key_type(args[1]);
        switch (t) {
            case Store::KeyType::String: return {Resp::simple_string("string"), false};
            case Store::KeyType::List: return {Resp::simple_string("list"), false};
            case Store::KeyType::None:
            default: return {Resp::simple_string("none"), false};
        }
    }
};

}

#endif // KVLLAY_COMMANDS_HPP
