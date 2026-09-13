#ifndef KVLLAY_STORE_HPP
#define KVLLAY_STORE_HPP

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <array>
#include <algorithm>
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
    static constexpr size_t NUM_SHARDS = 32;

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
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            shard.data[key] = Entry{value, 0};
            shard.keys_with_ttl.erase(key);
        }
        dirty_++;
        return true;
    }

    bool mset(const std::vector<std::pair<std::string, std::string>>& kvs) {
        if (kvs.empty()) return true;

        std::vector<size_t> involved_shards;
        involved_shards.reserve(kvs.size());
        for (const auto& [k, v] : kvs) {
            involved_shards.push_back(shard_index(k));
        }
        std::sort(involved_shards.begin(), involved_shards.end());
        involved_shards.erase(std::unique(involved_shards.begin(), involved_shards.end()), involved_shards.end());

        std::vector<std::unique_lock<std::shared_mutex>> locks;
        locks.reserve(involved_shards.size());
        for (size_t s_idx : involved_shards) {
            locks.emplace_back(shards_[s_idx].mutex);
        }

        for (const auto& [key, value] : kvs) {
            size_t idx = shard_index(key);
            auto& shard = shards_[idx];
            shard.data[key] = Entry{value, 0};
            shard.keys_with_ttl.erase(key);
        }
        dirty_ += kvs.size();
        return true;
    }

    bool setex(const std::string& key, uint64_t ttl_ms, const std::string& value) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        uint64_t expire_at = current_time_ms() + ttl_ms;
        {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            shard.data[key] = Entry{value, expire_at};
            shard.keys_with_ttl.insert(key);
        }
        dirty_++;
        return true;
    }

    std::optional<std::string> get(const std::string& key) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        uint64_t now = current_time_ms();
        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.data.find(key);
            if (it == shard.data.end()) {
                return std::nullopt;
            }
            if (it->second.expire_at == 0 || it->second.expire_at > now) {
                return it->second.value;
            }
        }

        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end()) {
            if (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
                shard.data.erase(it);
                shard.keys_with_ttl.erase(key);
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

        std::vector<size_t> involved_shards;
        involved_shards.reserve(keys.size());
        for (const auto& k : keys) {
            involved_shards.push_back(shard_index(k));
        }
        std::sort(involved_shards.begin(), involved_shards.end());
        involved_shards.erase(std::unique(involved_shards.begin(), involved_shards.end()), involved_shards.end());

        std::vector<std::shared_lock<std::shared_mutex>> locks;
        locks.reserve(involved_shards.size());
        for (size_t s_idx : involved_shards) {
            locks.emplace_back(shards_[s_idx].mutex);
        }

        for (const auto& key : keys) {
            size_t idx = shard_index(key);
            auto& shard = shards_[idx];
            auto it = shard.data.find(key);
            if (it == shard.data.end()) {
                result.push_back(std::nullopt);
            } else if (it->second.expire_at != 0 && it->second.expire_at <= now) {
                result.push_back(std::nullopt);
                expired_keys.push_back(key);
            } else {
                result.push_back(it->second.value);
            }
        }
        locks.clear();

        if (!expired_keys.empty()) {
            uint64_t cur_now = current_time_ms();
            for (const auto& key : expired_keys) {
                size_t idx = shard_index(key);
                auto& shard = shards_[idx];
                std::unique_lock<std::shared_mutex> lock(shard.mutex);
                auto it = shard.data.find(key);
                if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= cur_now) {
                    shard.data.erase(it);
                    shard.keys_with_ttl.erase(key);
                }
            }
        }

        return result;
    }

    size_t del(const std::vector<std::string>& keys) {
        if (keys.empty()) return 0;
        if (keys.size() == 1) {
            const auto& key = keys[0];
            size_t idx = shard_index(key);
            auto& shard = shards_[idx];
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            uint64_t now = current_time_ms();
            auto it = shard.data.find(key);
            if (it != shard.data.end()) {
                bool is_active = (it->second.expire_at == 0 || it->second.expire_at > now);
                shard.data.erase(it);
                shard.keys_with_ttl.erase(key);
                if (is_active) {
                    dirty_++;
                    return 1;
                }
            }
            return 0;
        }

        std::vector<size_t> involved_shards;
        involved_shards.reserve(keys.size());
        for (const auto& k : keys) {
            involved_shards.push_back(shard_index(k));
        }
        std::sort(involved_shards.begin(), involved_shards.end());
        involved_shards.erase(std::unique(involved_shards.begin(), involved_shards.end()), involved_shards.end());

        std::vector<std::unique_lock<std::shared_mutex>> locks;
        locks.reserve(involved_shards.size());
        for (size_t s_idx : involved_shards) {
            locks.emplace_back(shards_[s_idx].mutex);
        }

        size_t count = 0;
        uint64_t now = current_time_ms();
        for (const auto& key : keys) {
            size_t idx = shard_index(key);
            auto& shard = shards_[idx];
            auto it = shard.data.find(key);
            if (it != shard.data.end()) {
                bool is_active = (it->second.expire_at == 0 || it->second.expire_at > now);
                shard.data.erase(it);
                shard.keys_with_ttl.erase(key);
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
        if (keys.empty()) return 0;
        uint64_t now = current_time_ms();
        std::vector<std::string> expired_keys;
        size_t count = 0;

        std::vector<size_t> involved_shards;
        involved_shards.reserve(keys.size());
        for (const auto& k : keys) {
            involved_shards.push_back(shard_index(k));
        }
        std::sort(involved_shards.begin(), involved_shards.end());
        involved_shards.erase(std::unique(involved_shards.begin(), involved_shards.end()), involved_shards.end());

        std::vector<std::shared_lock<std::shared_mutex>> locks;
        locks.reserve(involved_shards.size());
        for (size_t s_idx : involved_shards) {
            locks.emplace_back(shards_[s_idx].mutex);
        }

        for (const auto& key : keys) {
            size_t idx = shard_index(key);
            auto& shard = shards_[idx];
            auto it = shard.data.find(key);
            if (it != shard.data.end()) {
                if (it->second.expire_at != 0 && it->second.expire_at <= now) {
                    expired_keys.push_back(key);
                } else {
                    count++;
                }
            }
        }
        locks.clear();

        if (!expired_keys.empty()) {
            uint64_t cur_now = current_time_ms();
            for (const auto& key : expired_keys) {
                size_t idx = shard_index(key);
                auto& shard = shards_[idx];
                std::unique_lock<std::shared_mutex> lock(shard.mutex);
                auto it = shard.data.find(key);
                if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= cur_now) {
                    shard.data.erase(it);
                    shard.keys_with_ttl.erase(key);
                }
            }
        }

        return count;
    }

    std::vector<std::string> keys(const std::string& pattern = "*") const {
        std::vector<std::string> result;
        uint64_t now = current_time_ms();

        if (pattern == "*") {
            for (const auto& shard : shards_) {
                std::shared_lock<std::shared_mutex> lock(shard.mutex);
                for (const auto& [k, v] : shard.data) {
                    if (v.expire_at == 0 || v.expire_at > now) {
                        result.push_back(k);
                    }
                }
            }
            return result;
        }

        bool match_prefix = (!pattern.empty() && pattern.back() == '*');
        bool match_suffix = (!pattern.empty() && pattern.front() == '*');

        if (!match_prefix && !match_suffix) {
            size_t idx = shard_index(pattern);
            auto& shard = shards_[idx];
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.data.find(pattern);
            if (it != shard.data.end() && (it->second.expire_at == 0 || it->second.expire_at > now)) {
                result.push_back(pattern);
            }
            return result;
        }

        std::string core = pattern;
        if (match_prefix && match_suffix && pattern.size() > 2) {
            core = pattern.substr(1, pattern.size() - 2);
            for (const auto& shard : shards_) {
                std::shared_lock<std::shared_mutex> lock(shard.mutex);
                for (const auto& [k, v] : shard.data) {
                    if (v.expire_at != 0 && v.expire_at <= now) continue;
                    if (k.find(core) != std::string::npos) result.push_back(k);
                }
            }
        } else if (match_prefix) {
            core = pattern.substr(0, pattern.size() - 1);
            for (const auto& shard : shards_) {
                std::shared_lock<std::shared_mutex> lock(shard.mutex);
                for (const auto& [k, v] : shard.data) {
                    if (v.expire_at != 0 && v.expire_at <= now) continue;
                    if (k.rfind(core, 0) == 0) result.push_back(k);
                }
            }
        } else if (match_suffix) {
            core = pattern.substr(1);
            for (const auto& shard : shards_) {
                std::shared_lock<std::shared_mutex> lock(shard.mutex);
                for (const auto& [k, v] : shard.data) {
                    if (v.expire_at != 0 && v.expire_at <= now) continue;
                    if (k.size() >= core.size() && k.compare(k.size() - core.size(), core.size(), core) == 0) {
                        result.push_back(k);
                    }
                }
            }
        }

        return result;
    }

    int expire(const std::string& key, uint64_t ttl_ms) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) {
            return 0;
        }
        uint64_t now = current_time_ms();
        if (it->second.expire_at != 0 && it->second.expire_at <= now) {
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            return 0;
        }

        if (ttl_ms == 0) {
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            return 1;
        }

        it->second.expire_at = now + ttl_ms;
        shard.keys_with_ttl.insert(key);
        dirty_++;
        return 1;
    }

    long long ttl(const std::string& key, bool in_milliseconds) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        uint64_t now = current_time_ms();
        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.data.find(key);
            if (it == shard.data.end()) {
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

        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
        }
        return -2;
    }

    int persist(const std::string& key) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) {
            return 0;
        }
        uint64_t now = current_time_ms();
        if (it->second.expire_at != 0 && it->second.expire_at <= now) {
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            return 0;
        }
        if (it->second.expire_at == 0) {
            return 0;
        }
        it->second.expire_at = 0;
        shard.keys_with_ttl.erase(key);
        dirty_++;
        return 1;
    }

    void flushdb() {
        for (auto& shard : shards_) {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            shard.data.clear();
            shard.keys_with_ttl.clear();
        }
        dirty_++;
    }

    size_t size() const {
        uint64_t now = current_time_ms();
        size_t count = 0;
        for (const auto& shard : shards_) {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            for (const auto& [k, v] : shard.data) {
                if (v.expire_at == 0 || v.expire_at > now) {
                    count++;
                }
            }
        }
        return count;
    }

    size_t expires_size() const {
        uint64_t now = current_time_ms();
        size_t count = 0;
        for (const auto& shard : shards_) {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            for (const auto& k : shard.keys_with_ttl) {
                auto it = shard.data.find(k);
                if (it != shard.data.end() && (it->second.expire_at == 0 || it->second.expire_at > now)) {
                    count++;
                }
            }
        }
        return count;
    }

    void evict_expired(size_t batch_limit = constants::DEFAULT_EVICTION_BATCH_LIMIT) {
        uint64_t now = current_time_ms();
        size_t limit_per_shard = (batch_limit + NUM_SHARDS - 1) / NUM_SHARDS;
        for (auto& shard : shards_) {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            if (shard.keys_with_ttl.empty()) continue;
            std::vector<std::string> to_remove;
            size_t checked = 0;
            for (auto it = shard.keys_with_ttl.begin(); it != shard.keys_with_ttl.end() && checked < limit_per_shard; ++it, ++checked) {
                auto data_it = shard.data.find(*it);
                if (data_it == shard.data.end()) {
                    to_remove.push_back(*it);
                } else if (data_it->second.expire_at != 0 && data_it->second.expire_at <= now) {
                    to_remove.push_back(*it);
                    shard.data.erase(data_it);
                }
            }
            for (const auto& k : to_remove) {
                shard.keys_with_ttl.erase(k);
            }
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
        uint64_t now_steady = current_time_ms();
        uint64_t now_wall = wall_time_ms();
        std::vector<DumpEntry> result;

        // Acquire shared lock on all shards in ascending order
        std::vector<std::shared_lock<std::shared_mutex>> locks;
        locks.reserve(NUM_SHARDS);
        for (const auto& shard : shards_) {
            locks.emplace_back(shard.mutex);
        }

        for (const auto& shard : shards_) {
            for (const auto& [k, v] : shard.data) {
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
        }
        return result;
    }

    void restore_entry(const std::string& key, const std::string& value, uint64_t expire_at_epoch_ms) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        if (expire_at_epoch_ms == 0) {
            shard.data[key] = Entry{value, 0};
            shard.keys_with_ttl.erase(key);
        } else {
            uint64_t now_wall = wall_time_ms();
            if (expire_at_epoch_ms <= now_wall) {
                shard.data.erase(key);
                shard.keys_with_ttl.erase(key);
            } else {
                uint64_t remaining_ms = expire_at_epoch_ms - now_wall;
                shard.data[key] = Entry{value, current_time_ms() + remaining_ms};
                shard.keys_with_ttl.insert(key);
            }
        }
    }

private:
    IncrStatus modify_int(const std::string& key, int64_t delta, int64_t& result_val, bool is_decrement) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        uint64_t now = current_time_ms();
        auto it = shard.data.find(key);
        if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= now) {
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            it = shard.data.end();
        }

        int64_t current_val = 0;
        if (it != shard.data.end()) {
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

        if (it != shard.data.end()) {
            it->second.value = std::to_string(new_val);
        } else {
            shard.data[key] = Entry{std::to_string(new_val), 0};
        }

        dirty_++;
        result_val = new_val;
        return IncrStatus::Success;
    }

    struct alignas(64) Shard {
        mutable std::shared_mutex mutex;
        std::unordered_map<std::string, Entry> data;
        std::unordered_set<std::string> keys_with_ttl;
    };

    std::array<Shard, NUM_SHARDS> shards_;

    inline size_t shard_index(const std::string& key) const noexcept {
        return std::hash<std::string>{}(key) & (NUM_SHARDS - 1);
    }

    std::atomic<uint64_t> dirty_{0};
    std::atomic<bool> active_eviction_running_{false};
    std::thread eviction_thread_;
    std::mutex eviction_cv_mutex_;
    std::condition_variable eviction_cv_;
};

}

#endif // KVLLAY_STORE_HPP
