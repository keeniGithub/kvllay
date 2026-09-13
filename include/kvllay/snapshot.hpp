#ifndef KVLLAY_SNAPSHOT_HPP
#define KVLLAY_SNAPSHOT_HPP

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <constants.hpp>
#include <store.hpp>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <io.h>
    #include <process.h>
    #define GETPID() _getpid()
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/types.h>
    #define GETPID() getpid()
#endif

namespace kvllay {

struct Crc32Table {
    uint32_t data[256];
    constexpr Crc32Table() : data() {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            data[i] = c;
        }
    }
};

inline constexpr Crc32Table CRC32_TABLE{};

class Crc32 {
public:
    static uint32_t update(uint32_t crc, const void* data, size_t length) {
        uint32_t c = ~crc;
        const uint8_t* p = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < length; ++i) {
            c = CRC32_TABLE.data[(c ^ p[i]) & 0xFF] ^ (c >> 8);
        }
        return ~c;
    }
};

class SnapshotManager {
public:
    static constexpr char MAGIC[8] = {'K', 'V', 'L', 'L', 'A', 'Y', 'S', '1'};
    static constexpr char MAGIC2[8] = {'K', 'V', 'L', 'L', 'A', 'Y', 'S', '2'};

    SnapshotManager(std::string snapshot_path = constants::DEFAULT_SNAPSHOT_FILE,
                    uint64_t save_interval_secs = constants::DEFAULT_SAVE_INTERVAL_SECS,
                    uint64_t save_changes = constants::DEFAULT_SAVE_CHANGES)
        : snapshot_path_(std::move(snapshot_path)),
          save_interval_secs_(save_interval_secs),
          save_changes_(save_changes),
          last_save_time_(static_cast<uint64_t>(time(nullptr))),
          saving_in_progress_(false),
          auto_save_running_(false) {}

    ~SnapshotManager() {
        stop_auto_save();
    }

    SnapshotManager(const SnapshotManager&) = delete;
    SnapshotManager& operator=(const SnapshotManager&) = delete;

    const std::string& snapshot_path() const {
        return snapshot_path_;
    }

    void set_snapshot_path(const std::string& path) {
        snapshot_path_ = path;
    }

    uint64_t last_save_time() const {
        return last_save_time_.load();
    }

    bool is_saving() const {
        return saving_in_progress_.load();
    }

    uint64_t save_interval_secs() const {
        return save_interval_secs_;
    }

    void set_save_interval(uint64_t seconds, uint64_t changes = constants::DEFAULT_SAVE_CHANGES) {
        save_interval_secs_ = seconds;
        save_changes_ = changes;
    }

    bool save_sync(const Store& store, const std::string& filepath = "") {
        std::string target = filepath.empty() ? snapshot_path_ : filepath;
        bool expected = false;
        if (!saving_in_progress_.compare_exchange_strong(expected, true)) {
            return false;
        }

        bool ok = perform_save(store.get_all_entries(), target);
        saving_in_progress_.store(false);
        return ok;
    }

    bool save_async(const Store& store, const std::string& filepath = "") {
        std::string target = filepath.empty() ? snapshot_path_ : filepath;
        bool expected = false;
        if (!saving_in_progress_.compare_exchange_strong(expected, true)) {
            return false;
        }

        // Capture in-memory snapshot quickly under shared lock
        auto entries = store.get_all_entries();

        std::thread([this, entries = std::move(entries), target]() mutable {
            perform_save(entries, target);
            saving_in_progress_.store(false);
        }).detach();

        return true;
    }

    bool load(Store& store, const std::string& filepath = "") {
        std::string target = filepath.empty() ? snapshot_path_ : filepath;

        FILE* fp = fopen(target.c_str(), "rb");
        if (!fp) {
            return false;
        }

        // Get file size
        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        if (file_size < 28) { // 8 magic + 8 timestamp + 8 count + 4 crc
            fclose(fp);
            return false;
        }

        std::vector<uint8_t> buffer(static_cast<size_t>(file_size));
        size_t read_bytes = fread(buffer.data(), 1, buffer.size(), fp);
        fclose(fp);

        if (read_bytes != buffer.size()) {
            return false;
        }

        // Verify magic header
        bool is_v1 = (std::memcmp(buffer.data(), MAGIC, sizeof(MAGIC)) == 0);
        bool is_v2 = (std::memcmp(buffer.data(), MAGIC2, sizeof(MAGIC2)) == 0);
        if (!is_v1 && !is_v2) {
            return false;
        }

        // Verify CRC32
        size_t data_len = buffer.size() - sizeof(uint32_t);
        uint32_t computed_crc = Crc32::update(0, buffer.data(), data_len);

        uint32_t stored_crc = 0;
        std::memcpy(&stored_crc, buffer.data() + data_len, sizeof(uint32_t));

        if (computed_crc != stored_crc) {
            std::cerr << "[kvllay] Snapshot CRC32 checksum mismatch in " << target << std::endl;
            return false;
        }

        // Parse content
        size_t offset = sizeof(MAGIC);

        uint64_t timestamp = 0;
        std::memcpy(&timestamp, buffer.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);

        uint64_t count = 0;
        std::memcpy(&count, buffer.data() + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);

        if (is_v1) {
            for (uint64_t i = 0; i < count; ++i) {
                if (offset + sizeof(uint32_t) > data_len) return false;
                uint32_t key_len = 0;
                std::memcpy(&key_len, buffer.data() + offset, sizeof(uint32_t));
                offset += sizeof(uint32_t);

                if (offset + key_len > data_len) return false;
                std::string key(reinterpret_cast<char*>(buffer.data() + offset), key_len);
                offset += key_len;

                if (offset + sizeof(uint32_t) > data_len) return false;
                uint32_t val_len = 0;
                std::memcpy(&val_len, buffer.data() + offset, sizeof(uint32_t));
                offset += sizeof(uint32_t);

                if (offset + val_len > data_len) return false;
                std::string val(reinterpret_cast<char*>(buffer.data() + offset), val_len);
                offset += val_len;

                if (offset + sizeof(uint64_t) > data_len) return false;
                uint64_t expire_at = 0;
                std::memcpy(&expire_at, buffer.data() + offset, sizeof(uint64_t));
                offset += sizeof(uint64_t);

                store.restore_string_entry(key, val, expire_at);
            }
        } else {
            // V2 format: supports string and list entries
            for (uint64_t i = 0; i < count; ++i) {
                if (offset + sizeof(uint8_t) > data_len) return false;
                uint8_t entry_type = buffer[offset++];

                if (offset + sizeof(uint32_t) > data_len) return false;
                uint32_t key_len = 0;
                std::memcpy(&key_len, buffer.data() + offset, sizeof(uint32_t));
                offset += sizeof(uint32_t);

                if (offset + key_len > data_len) return false;
                std::string key(reinterpret_cast<char*>(buffer.data() + offset), key_len);
                offset += key_len;

                if (entry_type == static_cast<uint8_t>(Store::EntryType::String)) {
                    if (offset + sizeof(uint32_t) > data_len) return false;
                    uint32_t val_len = 0;
                    std::memcpy(&val_len, buffer.data() + offset, sizeof(uint32_t));
                    offset += sizeof(uint32_t);

                    if (offset + val_len > data_len) return false;
                    std::string val(reinterpret_cast<char*>(buffer.data() + offset), val_len);
                    offset += val_len;

                    if (offset + sizeof(uint64_t) > data_len) return false;
                    uint64_t expire_at = 0;
                    std::memcpy(&expire_at, buffer.data() + offset, sizeof(uint64_t));
                    offset += sizeof(uint64_t);

                    store.restore_string_entry(key, val, expire_at);
                } else if (entry_type == static_cast<uint8_t>(Store::EntryType::List)) {
                    if (offset + sizeof(uint32_t) > data_len) return false;
                    uint32_t elem_count = 0;
                    std::memcpy(&elem_count, buffer.data() + offset, sizeof(uint32_t));
                    offset += sizeof(uint32_t);

                    std::vector<std::string> elements;
                    elements.reserve(elem_count);

                    for (uint32_t j = 0; j < elem_count; ++j) {
                        if (offset + sizeof(uint32_t) > data_len) return false;
                        uint32_t elem_len = 0;
                        std::memcpy(&elem_len, buffer.data() + offset, sizeof(uint32_t));
                        offset += sizeof(uint32_t);

                        if (offset + elem_len > data_len) return false;
                        std::string elem(reinterpret_cast<char*>(buffer.data() + offset), elem_len);
                        offset += elem_len;

                        elements.push_back(std::move(elem));
                    }

                    if (offset + sizeof(uint64_t) > data_len) return false;
                    uint64_t expire_at = 0;
                    std::memcpy(&expire_at, buffer.data() + offset, sizeof(uint64_t));
                    offset += sizeof(uint64_t);

                    store.restore_list_entry(key, elements, expire_at);
                } else {
                    return false;
                }
            }
        }

        last_save_time_.store(timestamp);
        return true;
    }

    void start_auto_save(Store& store) {
        if (save_interval_secs_ == 0 || auto_save_running_.exchange(true)) {
            return;
        }

        auto_save_thread_ = std::thread([this, &store]() {
            while (auto_save_running_) {
                {
                    std::unique_lock<std::mutex> lock(auto_save_cv_mutex_);
                    auto_save_cv_.wait_for(lock, std::chrono::seconds(1), [this]() {
                        return !auto_save_running_.load();
                    });
                }
                if (!auto_save_running_) break;

                uint64_t now = static_cast<uint64_t>(time(nullptr));
                if (now >= last_save_time_.load() + save_interval_secs_) {
                    if (store.dirty_count() >= save_changes_) {
                        if (save_async(store)) {
                            store.reset_dirty();
                        }
                    }
                }
            }
        });
    }

    void stop_auto_save() {
        if (!auto_save_running_.exchange(false)) {
            return;
        }
        auto_save_cv_.notify_all();
        if (auto_save_thread_.joinable()) {
            auto_save_thread_.join();
        }
    }

private:
    bool perform_save(const std::vector<Store::DumpEntry>& entries, const std::string& target_path) {
        uint64_t now_epoch = static_cast<uint64_t>(time(nullptr));
        std::string tmp_path = target_path + ".tmp." + std::to_string(GETPID()) + "_" + std::to_string(now_epoch);

        FILE* fp = fopen(tmp_path.c_str(), "wb");
        if (!fp) {
            std::cerr << "[kvllay] Failed to open temporary snapshot file " << tmp_path << std::endl;
            return false;
        }

        // Buffer for high-performance sequential I/O
        constexpr size_t BUF_SIZE = 65536;
        std::vector<char> io_buffer(BUF_SIZE);
        setvbuf(fp, io_buffer.data(), _IOFBF, BUF_SIZE);

        uint32_t crc = 0;

        auto write_data = [&](const void* data, size_t size) -> bool {
            if (fwrite(data, 1, size, fp) != size) {
                return false;
            }
            crc = Crc32::update(crc, data, size);
            return true;
        };

        // 1. Magic header
        if (!write_data(MAGIC2, sizeof(MAGIC2))) {
            fclose(fp);
            remove(tmp_path.c_str());
            return false;
        }

        // 2. Epoch timestamp
        if (!write_data(&now_epoch, sizeof(now_epoch))) {
            fclose(fp);
            remove(tmp_path.c_str());
            return false;
        }

        // 3. Entry count
        uint64_t count = entries.size();
        if (!write_data(&count, sizeof(count))) {
            fclose(fp);
            remove(tmp_path.c_str());
            return false;
        }

        // 4. Entries
        for (const auto& entry : entries) {
            uint8_t type = static_cast<uint8_t>(entry.type);
            if (!write_data(&type, sizeof(type))) goto write_failed;

            uint32_t key_len = static_cast<uint32_t>(entry.key.size());
            if (!write_data(&key_len, sizeof(key_len))) goto write_failed;
            if (key_len > 0 && !write_data(entry.key.data(), key_len)) goto write_failed;

            if (entry.type == Store::EntryType::String) {
                uint32_t val_len = static_cast<uint32_t>(entry.string_val.size());
                if (!write_data(&val_len, sizeof(val_len))) goto write_failed;
                if (val_len > 0 && !write_data(entry.string_val.data(), val_len)) goto write_failed;
            } else if (entry.type == Store::EntryType::List) {
                uint32_t elem_count = static_cast<uint32_t>(entry.list_val.size());
                if (!write_data(&elem_count, sizeof(elem_count))) goto write_failed;
                for (const auto& elem : entry.list_val) {
                    uint32_t elem_len = static_cast<uint32_t>(elem.size());
                    if (!write_data(&elem_len, sizeof(elem_len))) goto write_failed;
                    if (elem_len > 0 && !write_data(elem.data(), elem_len)) goto write_failed;
                }
            }

            uint64_t expire_at = entry.expire_at_epoch_ms;
            if (!write_data(&expire_at, sizeof(expire_at))) goto write_failed;
        }

        // 5. CRC32 footer (not included in CRC computation)
        if (fwrite(&crc, 1, sizeof(crc), fp) != sizeof(crc)) {
            goto write_failed;
        }

        fflush(fp);

        // Sync to physical storage
#ifdef _WIN32
        {
            int fd = _fileno(fp);
            HANDLE h = (HANDLE)_get_osfhandle(fd);
            if (h != INVALID_HANDLE_VALUE) {
                FlushFileBuffers(h);
            }
        }
#else
        {
            int fd = fileno(fp);
            #if defined(__APPLE__) || defined(__FreeBSD__)
            fsync(fd);
            #else
            fdatasync(fd);
            #endif
        }
#endif

        fclose(fp);
        fp = nullptr;

        // Atomic file rename
        if (!atomic_rename(tmp_path, target_path)) {
            std::cerr << "[kvllay] Failed to rename " << tmp_path << " to " << target_path << std::endl;
            remove(tmp_path.c_str());
            return false;
        }

        last_save_time_.store(now_epoch);
        return true;

    write_failed:
        if (fp) fclose(fp);
        remove(tmp_path.c_str());
        std::cerr << "[kvllay] Write failed during snapshot generation" << std::endl;
        return false;
    }

    static bool atomic_rename(const std::string& from, const std::string& to) {
#ifdef _WIN32
        return MoveFileExA(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) != 0;
#else
        return ::rename(from.c_str(), to.c_str()) == 0;
#endif
    }

    std::string snapshot_path_;
    uint64_t save_interval_secs_;
    uint64_t save_changes_;
    std::atomic<uint64_t> last_save_time_;
    std::atomic<bool> saving_in_progress_;

    std::atomic<bool> auto_save_running_;
    std::thread auto_save_thread_;
    std::mutex auto_save_cv_mutex_;
    std::condition_variable auto_save_cv_;
};

} // namespace kvllay

#endif // KVLLAY_SNAPSHOT_HPP
