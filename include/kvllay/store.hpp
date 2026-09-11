#ifndef KVLLAY_STORE_HPP
#define KVLLAY_STORE_HPP

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <shared_mutex>
#include <mutex>

namespace kvllay {

class Store {
public:
    Store() = default;
    ~Store() = default;

    // Non-copyable
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    bool set(const std::string& key, const std::string& value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        data_[key] = value;
        return true;
    }

    std::optional<std::string> get(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = data_.find(key);
        if (it != data_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    size_t del(const std::vector<std::string>& keys) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        size_t count = 0;
        for (const auto& key : keys) {
            if (data_.erase(key) > 0) {
                count++;
            }
        }
        return count;
    }

    size_t exists(const std::vector<std::string>& keys) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        size_t count = 0;
        for (const auto& key : keys) {
            if (data_.find(key) != data_.end()) {
                count++;
            }
        }
        return count;
    }

    std::vector<std::string> keys(const std::string& pattern = "*") const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<std::string> result;

        if (pattern == "*") {
            result.reserve(data_.size());
            for (const auto& [k, v] : data_) {
                result.push_back(k);
            }
            return result;
        }

        // Simple wildcard match (* at end, * at start, or exact)
        bool match_prefix = (!pattern.empty() && pattern.back() == '*');
        bool match_suffix = (!pattern.empty() && pattern.front() == '*');

        std::string core = pattern;
        if (match_prefix && match_suffix && pattern.size() > 2) {
            core = pattern.substr(1, pattern.size() - 2);
            for (const auto& [k, v] : data_) {
                if (k.find(core) != std::string::npos) result.push_back(k);
            }
        } else if (match_prefix) {
            core = pattern.substr(0, pattern.size() - 1);
            for (const auto& [k, v] : data_) {
                if (k.rfind(core, 0) == 0) result.push_back(k);
            }
        } else if (match_suffix) {
            core = pattern.substr(1);
            for (const auto& [k, v] : data_) {
                if (k.size() >= core.size() && k.compare(k.size() - core.size(), core.size(), core) == 0) {
                    result.push_back(k);
                }
            }
        } else {
            if (data_.find(pattern) != data_.end()) {
                result.push_back(pattern);
            }
        }

        return result;
    }

    void flushdb() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        data_.clear();
    }

    size_t size() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return data_.size();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> data_;
};

} // namespace kvllay

#endif // KVLLAY_STORE_HPP
