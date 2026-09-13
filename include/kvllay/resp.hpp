#ifndef KVLLAY_RESP_HPP
#define KVLLAY_RESP_HPP

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <charconv>

namespace kvllay {

enum class ParseStatus {
    Success,
    Incomplete,
    Error
};

class Resp {
public:
    static std::string simple_string(const std::string& str) {
        if (str == "OK") return ok();
        if (str == "PONG") return pong();
        return "+" + str + "\r\n";
    }

    static const std::string& ok() {
        static const std::string s = "+OK\r\n";
        return s;
    }

    static const std::string& pong() {
        static const std::string s = "+PONG\r\n";
        return s;
    }

    static std::string error(const std::string& err) {
        if (err.rfind("ERR ", 0) == 0 || err.rfind("WRONGTYPE ", 0) == 0) {
            return "-" + err + "\r\n";
        }
        return "-ERR " + err + "\r\n";
    }

    static std::string integer(long long val) {
        return ":" + std::to_string(val) + "\r\n";
    }

    static std::string bulk_string(const std::string& val) {
        return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
    }

    static const std::string& null_bulk_string() {
        static const std::string s = "$-1\r\n";
        return s;
    }

    static const std::string& empty_array() {
        static const std::string s = "*0\r\n";
        return s;
    }

    static std::string array(const std::vector<std::string>& items) {
        std::string res = "*" + std::to_string(items.size()) + "\r\n";
        for (const auto& item : items) {
            res += bulk_string(item);
        }
        return res;
    }

    static std::string array_of_bulk(const std::vector<std::optional<std::string>>& items) {
        std::string res = "*" + std::to_string(items.size()) + "\r\n";
        for (const auto& item : items) {
            if (item.has_value()) {
                res += bulk_string(*item);
            } else {
                res += null_bulk_string();
            }
        }
        return res;
    }

    static ParseStatus parse_command(std::string_view buffer, std::vector<std::string>& args, size_t& consumed_bytes) {
        args.clear();
        consumed_bytes = 0;

        if (buffer.empty()) {
            return ParseStatus::Incomplete;
        }

        if (buffer[0] == '*') {
            return parse_resp_array(buffer, args, consumed_bytes);
        }

        return parse_inline_command(buffer, args, consumed_bytes);
    }

private:
    static ParseStatus parse_resp_array(std::string_view buffer, std::vector<std::string>& args, size_t& consumed_bytes) {
        size_t pos = buffer.find("\r\n");
        if (pos == std::string_view::npos) {
            return ParseStatus::Incomplete;
        }

        std::string_view count_sv = buffer.substr(1, pos - 1);
        long long count = 0;
        auto [ptr1, ec1] = std::from_chars(count_sv.data(), count_sv.data() + count_sv.size(), count);
        if (ec1 != std::errc() || ptr1 != count_sv.data() + count_sv.size()) {
            return ParseStatus::Error;
        }

        if (count < 0) {
            consumed_bytes = pos + 2;
            return ParseStatus::Success;
        }

        size_t current = pos + 2;
        args.reserve(count);

        for (long long i = 0; i < count; ++i) {
            if (current >= buffer.size()) {
                return ParseStatus::Incomplete;
            }

            if (buffer[current] != '$') {
                return ParseStatus::Error;
            }

            size_t crlf = buffer.find("\r\n", current);
            if (crlf == std::string_view::npos) {
                return ParseStatus::Incomplete;
            }

            std::string_view len_sv = buffer.substr(current + 1, crlf - (current + 1));
            long long str_len = 0;
            auto [ptr2, ec2] = std::from_chars(len_sv.data(), len_sv.data() + len_sv.size(), str_len);
            if (ec2 != std::errc() || ptr2 != len_sv.data() + len_sv.size()) {
                return ParseStatus::Error;
            }

            if (str_len < 0) {
                args.emplace_back("");
                current = crlf + 2;
                continue;
            }

            size_t data_start = crlf + 2;
            size_t data_end = data_start + str_len;
            if (buffer.size() < data_end + 2) {
                return ParseStatus::Incomplete;
            }

            if (buffer.substr(data_end, 2) != "\r\n") {
                return ParseStatus::Error;
            }

            args.emplace_back(buffer.substr(data_start, str_len));
            current = data_end + 2;
        }

        consumed_bytes = current;
        return ParseStatus::Success;
    }

    static ParseStatus parse_inline_command(std::string_view buffer, std::vector<std::string>& args, size_t& consumed_bytes) {
        size_t line_end = buffer.find("\r\n");
        size_t delim_len = 2;
        if (line_end == std::string_view::npos) {
            line_end = buffer.find('\n');
            delim_len = 1;
        }

        if (line_end == std::string_view::npos) {
            return ParseStatus::Incomplete;
        }

        std::string_view line = buffer.substr(0, line_end);
        consumed_bytes = line_end + delim_len;

        size_t idx = 0;
        while (idx < line.size()) {
            while (idx < line.size() && std::isspace(static_cast<unsigned char>(line[idx]))) {
                idx++;
            }
            if (idx >= line.size()) break;

            if (line[idx] == '"' || line[idx] == '\'') {
                char quote = line[idx++];
                std::string token;
                while (idx < line.size() && line[idx] != quote) {
                    if (line[idx] == '\\' && idx + 1 < line.size()) {
                        idx++;
                    }
                    token += line[idx++];
                }
                if (idx < line.size() && line[idx] == quote) {
                    idx++;
                }
                args.push_back(token);
            } else {
                size_t start = idx;
                while (idx < line.size() && !std::isspace(static_cast<unsigned char>(line[idx]))) {
                    idx++;
                }
                args.emplace_back(line.substr(start, idx - start));
            }
        }

        return ParseStatus::Success;
    }
};

}

#endif // KVLLAY_RESP_HPP
