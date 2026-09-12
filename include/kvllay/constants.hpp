#ifndef KVLLAY_CONSTANTS_HPP
#define KVLLAY_CONSTANTS_HPP

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace kvllay {
namespace constants {

// Versioning
inline constexpr const char* VERSION = "1.0.0";

// Server Identity & Defaults
inline constexpr const char* SERVER_NAME = "kvllay";
inline const std::string REDIS_VERSION_STRING = std::string(SERVER_NAME) + "-" + VERSION;
inline constexpr int DEFAULT_PORT = 6379;
inline constexpr const char* DEFAULT_HOST = "0.0.0.0";

// Network & Buffering
inline constexpr size_t CLIENT_BUFFER_SIZE = 4096;

// Store & Active Eviction Defaults
inline constexpr uint64_t DEFAULT_EVICTION_INTERVAL_MS = 100;
inline constexpr size_t DEFAULT_EVICTION_BATCH_LIMIT = 100;

// Protocol Constants
inline constexpr const char* CRLF = "\r\n";

} // namespace constants
} // namespace kvllay

#endif // KVLLAY_CONSTANTS_HPP
