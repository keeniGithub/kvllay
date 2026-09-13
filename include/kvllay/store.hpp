#ifndef KVLLAY_STORE_HPP
#define KVLLAY_STORE_HPP

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <shared_mutex>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <charconv>
#include <limits>
#include <constants.hpp>

namespace kvllay {

class Store {
public:
    struct Entry {
        std::string value;
        uint64_t expire_at = 0;
    };

    struct DumpEntry {
        std::string key;
        std::string value;
        uint64_t expire_at_epoch_ms = 0; // 0 if persistent, otherwise wall-clock epoch ms
    };

    Store() {
        start_active_eviction();
    }

    ~Store() {
        stop_active_eviction();
    }

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    static uint64_t current_time_ms() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }

    static uint64_t wall_time_ms() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }

    enum class IncrStatus {
        Success,
        NotAnInteger,
        Overflow
    };

    static bool parse_int64(const std::string& str, int64_t& out) {
        if (str.empty()) return false;
        const char* start = str.data();
        size_t len = str.size();
        if (start[0] == '+') {
            start++;
            len--;
            if (len == 0 || start[0] == '-' || start[0] == '+') return false;
        }
        auto [ptr, ec] = std::from_chars(start, start + len, out);
        return ec == std::errc() && ptr == start + len;
    }

    static bool add_overflow(int64_t a, int64_t b, int64_t& result) {
        if ((b > 0 && a > std::numeric_limits<int64_t>::max() - b) ||
            (b < 0 && a < std::numeric_limits<int64_t>::min() - b)) {
            return true;
        }
        result = a + b;
        return false;
    }

    static bool sub_overflow(int64_t a, int64_t b, int64_t& result) {
        if ((b > 0 && a < std::numeric_limits<int64_t>::min() + b) ||
            (b < 0 && a > std::numeric_limits<int64_t>::max() + b)) {
            return true;
        }
        result = a - b;
        return false;
    }

    IncrStatus incrby(const std::string& key, int64_t delta, int64_t& result_val) {
        return modify_int(key, delta, result_val, false);
    }

    IncrStatus decrby(const std::string& key, int64_t delta, int64_t& result_val) {
        return modify_int(key, delta, result_val, true);
    }

    bool set(const std::string& key, const std::string& value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        data_[key] = Entry{value, 0};
        keys_with_ttl_.erase(key);
        dirty_++;
        return true;
    }

    bool mset(const std::vector<std::pair<std::string, std::string>>& kvs) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        for (const auto& [key, value] : kvs) {
            data_[key] = Entry{value, 0};
            keys_with_ttl_.erase(key);
        }
        dirty_ += kvs.size();
        return true;
    }

    bool setex(const std::string& key, uint64_t ttl_ms, const std::string& value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        uint64_t expire_at = current_time_ms() + ttl_ms;
        data_[key] = Entry{value, expire_at};
        keys_with_ttl_.insert(key);
        dirty_++;
        return true;
    }

    std::optional<std::string> get(const std::string& key) {
        uint64_t now = current_time_ms();
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = data_.find(key);
            if (it == data_.end()) {
                return std::nullopt;
            }
            if (it->second.expire_at == 0 || it->second.expire_at > now) {
                return it->second.value;
            }
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = data_.find(key);
        if (it != data_.end()) {
            if (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
                data_.erase(it);
                keys_with_ttl_.erase(key);
                return std::nullopt;
            }
            return it->second.value;
        }
        return std::nullopt;
    }

    std::vector<std::optional<std::string>> mget(const std::vector<std::string>& keys) {
        uint64_t now = current_time_ms();
        std::vector<std::optional<std::string>> result;
        result.reserve(keys.size());
        std::vector<std::string> expired_keys;

        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            for (const auto& key : keys) {
                auto it = data_.find(key);
                if (it == data_.end()) {
                    result.push_back(std::nullopt);
                } else if (it->second.expire_at != 0 && it->second.expire_at <= now) {
                    result.push_back(std::nullopt);
                    expired_keys.push_back(key);
                } else {
                    result.push_back(it->second.value);
                }
            }
        }

        if (!expired_keys.empty()) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            uint64_t cur_now = current_time_ms();
            for (const auto& key : expired_keys) {
                auto it = data_.find(key);
                if (it != data_.end() && it->second.expire_at != 0 && it->second.expire_at <= cur_now) {
                    data_.erase(it);
                    keys_with_ttl_.erase(key);
                }
            }
        }

        return result;
    }

    size_t del(const std::vector<std::string>& keys) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        size_t count = 0;
        uint64_t now = current_time_ms();
        for (const auto& key : keys) {
            auto it = data_.find(key);
            if (it != data_.end()) {
                bool is_active = (it->second.expire_at == 0 || it->second.expire_at > now);
                data_.erase(it);
                keys_with_ttl_.erase(key);
                if (is_active) {
                    count++;
                }
            }
        }
        if (count > 0) {
            dirty_ += count;
        }
        return count;
    }

    size_t exists(const std::vector<std::string>& keys) {
        uint64_t now = current_time_ms();
        std::vector<std::string> expired_keys;
        size_t count = 0;

        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            for (const auto& key : keys) {
                auto it = data_.find(key);
                if (it != data_.end()) {
                    if (it->second.expire_at != 0 && it->second.expire_at <= now) {
                        expired_keys.push_back(key);
                    } else {
                        count++;
                    }
                }
            }
        }

        if (!expired_keys.empty()) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            uint64_t cur_now = current_time_ms();
            for (const auto& key : expired_keys) {
                auto it = data_.find(key);
                if (it != data_.end() && it->second.expire_at != 0 && it->second.expire_at <= cur_now) {
                    data_.erase(it);
                    keys_with_ttl_.erase(key);
                }
            }
        }

        return count;
    }

    std::vector<std::string> keys(const std::string& pattern = "*") const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<std::string> result;
        uint64_t now = current_time_ms();

        if (pattern == "*") {
            result.reserve(data_.size());
            for (const auto& [k, v] : data_) {
                if (v.expire_at == 0 || v.expire_at > now) {
                    result.push_back(k);
                }
            }
            return result;
        }

        bool match_prefix = (!pattern.empty() && pattern.back() == '*');
        bool match_suffix = (!pattern.empty() && pattern.front() == '*');

        std::string core = pattern;
        if (match_prefix && match_suffix && pattern.size() > 2) {
            core = pattern.substr(1, pattern.size() - 2);
            for (const auto& [k, v] : data_) {
                if (v.expire_at != 0 && v.expire_at <= now) continue;
                if (k.find(core) != std::string::npos) result.push_back(k);
            }
        } else if (match_prefix) {
            core = pattern.substr(0, pattern.size() - 1);
            for (const auto& [k, v] : data_) {
                if (v.expire_at != 0 && v.expire_at <= now) continue;
                if (k.rfind(core, 0) == 0) result.push_back(k);
            }
        } else if (match_suffix) {
            core = pattern.substr(1);
            for (const auto& [k, v] : data_) {
                if (v.expire_at != 0 && v.expire_at <= now) continue;
                if (k.size() >= core.size() && k.compare(k.size() - core.size(), core.size(), core) == 0) {
                    result.push_back(k);
                }
            }
        } else {
            auto it = data_.find(pattern);
            if (it != data_.end() && (it->second.expire_at == 0 || it->second.expire_at > now)) {
                result.push_back(pattern);
            }
        }

        return result;
    }

    int expire(const std::string& key, uint64_t ttl_ms) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = data_.find(key);
        if (it == data_.end()) {
            return 0;
        }
        uint64_t now = current_time_ms();
        if (it->second.expire_at != 0 && it->second.expire_at <= now) {
            data_.erase(it);
            keys_with_ttl_.erase(key);
            return 0;
        }

        if (ttl_ms == 0) {
            data_.erase(it);
            keys_with_ttl_.erase(key);
            return 1;
        }

        it->second.expire_at = now + ttl_ms;
        keys_with_ttl_.insert(key);
        dirty_++;
        return 1;
    }

    long long ttl(const std::string& key, bool in_milliseconds) {
        uint64_t now = current_time_ms();
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = data_.find(key);
            if (it == data_.end()) {
                return -2;
            }
            if (it->second.expire_at == 0) {
                return -1;
            }
            if (it->second.expire_at > now) {
                uint64_t diff = it->second.expire_at - now;
                if (in_milliseconds) {
                    return static_cast<long long>(diff);
                }
                return static_cast<long long>((diff + 999) / 1000);
            }
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = data_.find(key);
        if (it != data_.end() && it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
            data_.erase(it);
            keys_with_ttl_.erase(key);
        }
        return -2;
    }

    int persist(const std::string& key) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = data_.find(key);
        if (it == data_.end()) {
            return 0;
        }
        uint64_t now = current_time_ms();
        if (it->second.expire_at != 0 && it->second.expire_at <= now) {
            data_.erase(it);
            keys_with_ttl_.erase(key);
            return 0;
        }
        if (it->second.expire_at == 0) {
            return 0;
        }
        it->second.expire_at = 0;
        keys_with_ttl_.erase(key);
        dirty_++;
        return 1;
    }

    void flushdb() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        data_.clear();
        keys_with_ttl_.clear();
        dirty_++;
    }

    size_t size() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        uint64_t now = current_time_ms();
        size_t count = 0;
        for (const auto& [k, v] : data_) {
            if (v.expire_at == 0 || v.expire_at > now) {
                count++;
            }
        }
        return count;
    }

    size_t expires_size() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        uint64_t now = current_time_ms();
        size_t count = 0;
        for (const auto& k : keys_with_ttl_) {
            auto it = data_.find(k);
            if (it != data_.end() && (it->second.expire_at == 0 || it->second.expire_at > now)) {
                count++;
            }
        }
        return count;
    }

    void evict_expired(size_t batch_limit = constants::DEFAULT_EVICTION_BATCH_LIMIT) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (keys_with_ttl_.empty()) {
            return;
        }
        uint64_t now = current_time_ms();
        std::vector<std::string> to_remove;
        size_t checked = 0;
        for (auto it = keys_with_ttl_.begin(); it != keys_with_ttl_.end() && checked < batch_limit; ++it, ++checked) {
            auto data_it = data_.find(*it);
            if (data_it == data_.end()) {
                to_remove.push_back(*it);
            } else if (data_it->second.expire_at != 0 && data_it->second.expire_at <= now) {
                to_remove.push_back(*it);
                data_.erase(data_it);
            }
        }
        for (const auto& k : to_remove) {
            keys_with_ttl_.erase(k);
        }
    }

    void start_active_eviction() {
        if (active_eviction_running_.exchange(true)) {
            return;
        }
        eviction_thread_ = std::thread([this]() {
            while (active_eviction_running_) {
                {
                    std::unique_lock<std::mutex> lk(eviction_cv_mutex_);
                    eviction_cv_.wait_for(lk, std::chrono::milliseconds(constants::DEFAULT_EVICTION_INTERVAL_MS), [this]() {
                        return !active_eviction_running_.load();
                    });
                }
                if (!active_eviction_running_) {
                    break;
                }
                evict_expired();
            }
        });
    }

    void stop_active_eviction() {
        if (!active_eviction_running_.exchange(false)) {
            return;
        }
        eviction_cv_.notify_all();
        if (eviction_thread_.joinable()) {
            eviction_thread_.join();
        }
    }

    uint64_t dirty_count() const {
        return dirty_.load();
    }

    void reset_dirty() {
        dirty_.store(0);
    }

    std::vector<DumpEntry> get_all_entries() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        uint64_t now_steady = current_time_ms();
        uint64_t now_wall = wall_time_ms();
        std::vector<DumpEntry> result;
        result.reserve(data_.size());

        for (const auto& [k, v] : data_) {
            if (v.expire_at != 0 && v.expire_at <= now_steady) {
                continue;
            }
            uint64_t expire_at_wall = 0;
            if (v.expire_at > now_steady) {
                uint64_t remaining_ms = v.expire_at - now_steady;
                expire_at_wall = now_wall + remaining_ms;
            }
            result.push_back({k, v.value, expire_at_wall});
        }
        return result;
    }

    void restore_entry(const std::string& key, const std::string& value, uint64_t expire_at_epoch_ms) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (expire_at_epoch_ms == 0) {
            data_[key] = Entry{value, 0};
            keys_with_ttl_.erase(key);
        } else {
            uint64_t now_wall = wall_time_ms();
            if (expire_at_epoch_ms <= now_wall) {
                data_.erase(key);
                keys_with_ttl_.erase(key);
            } else {
                uint64_t remaining_ms = expire_at_epoch_ms - now_wall;
                data_[key] = Entry{value, current_time_ms() + remaining_ms};
                keys_with_ttl_.insert(key);
            }
        }
    }

private:
    IncrStatus modify_int(const std::string& key, int64_t delta, int64_t& result_val, bool is_decrement) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        uint64_t now = current_time_ms();
        auto it = data_.find(key);
        if (it != data_.end() && it->second.expire_at != 0 && it->second.expire_at <= now) {
            data_.erase(it);
            keys_with_ttl_.erase(key);
            it = data_.end();
        }

        int64_t current_val = 0;
        if (it != data_.end()) {
            if (!parse_int64(it->second.value, current_val)) {
                return IncrStatus::NotAnInteger;
            }
        }

        int64_t new_val = 0;
        bool overflow = is_decrement ? sub_overflow(current_val, delta, new_val)
                                     : add_overflow(current_val, delta, new_val);
        if (overflow) {
            return IncrStatus::Overflow;
        }

        if (it != data_.end()) {
            it->second.value = std::to_string(new_val);
        } else {
            data_[key] = Entry{std::to_string(new_val), 0};
        }

        dirty_++;
        result_val = new_val;
        return IncrStatus::Success;
    }

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Entry> data_;
    std::unordered_set<std::string> keys_with_ttl_;

    std::atomic<uint64_t> dirty_{0};
    std::atomic<bool> active_eviction_running_{false};
    std::thread eviction_thread_;
    std::mutex eviction_cv_mutex_;
    std::condition_variable eviction_cv_;
};

}

#endif // KVLLAY_STORE_HPP
