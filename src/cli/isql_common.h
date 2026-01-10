#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <termios.h>
#include <unistd.h>
#include <variant>
#include <vector>

#include "scratchbird/fdw/fdw_types.h"

namespace scratchbird {
namespace cli {

struct OutputConfig {
    bool tuples_only = false;
    bool no_align = false;
    bool show_row_count = true;
    std::string field_separator = "|";
    std::string null_display = "NULL";
};

inline std::string trim(std::string_view input) {
    size_t start = 0;
    while (start < input.size() &&
           std::isspace(static_cast<unsigned char>(input[start]))) {
        ++start;
    }
    size_t end = input.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(input[end - 1]))) {
        --end;
    }
    return std::string(input.substr(start, end - start));
}

inline std::string promptHidden(const std::string& prompt) {
    std::string value;
    if (!isatty(STDIN_FILENO)) {
        return value;
    }

    termios oldt{};
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) {
        return value;
    }
    termios newt = oldt;
    newt.c_lflag &= ~ECHO;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) != 0) {
        return value;
    }

    std::cerr << prompt;
    std::getline(std::cin, value);
    std::cerr << "\n";

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return value;
}

inline std::vector<std::string> splitStatements(const std::string& input,
                                                std::string* remainder_out,
                                                bool allow_hash_comments) {
    std::vector<std::string> statements;
    size_t start = 0;

    bool in_single = false;
    bool in_double = false;
    bool in_backtick = false;
    bool in_line_comment = false;
    bool in_block_comment = false;
    std::string dollar_tag;

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        char next = (i + 1 < input.size()) ? input[i + 1] : '\0';

        if (in_line_comment) {
            if (c == '\n') {
                in_line_comment = false;
            }
            continue;
        }

        if (in_block_comment) {
            if (c == '*' && next == '/') {
                in_block_comment = false;
                ++i;
            }
            continue;
        }

        if (!in_single && !in_double && !in_backtick && dollar_tag.empty()) {
            if (c == '-' && next == '-') {
                in_line_comment = true;
                ++i;
                continue;
            }
            if (allow_hash_comments && c == '#') {
                in_line_comment = true;
                continue;
            }
            if (c == '/' && next == '*') {
                in_block_comment = true;
                ++i;
                continue;
            }
        }

        if (!in_single && !in_double && !in_backtick) {
            if (!dollar_tag.empty()) {
                if (c == '$' && input.compare(i, dollar_tag.size(), dollar_tag) == 0) {
                    i += dollar_tag.size() - 1;
                    dollar_tag.clear();
                    continue;
                }
            } else if (c == '$') {
                size_t end = input.find('$', i + 1);
                if (end != std::string::npos) {
                    bool valid_tag = true;
                    for (size_t j = i + 1; j < end; ++j) {
                        char t = input[j];
                        if (!(std::isalnum(static_cast<unsigned char>(t)) || t == '_')) {
                            valid_tag = false;
                            break;
                        }
                    }
                    if (valid_tag) {
                        dollar_tag = input.substr(i, end - i + 1);
                        i = end;
                        continue;
                    }
                }
            }
        }

        if (dollar_tag.empty()) {
            if (!in_double && !in_backtick && c == '\'') {
                if (in_single && next == '\'') {
                    ++i;
                } else {
                    in_single = !in_single;
                }
                continue;
            }
            if (!in_single && !in_backtick && c == '"') {
                in_double = !in_double;
                continue;
            }
            if (!in_single && !in_double && c == '`') {
                in_backtick = !in_backtick;
                continue;
            }
        }

        if (!in_single && !in_double && !in_backtick && dollar_tag.empty() && c == ';') {
            std::string statement = trim(std::string_view(input).substr(start, i - start));
            if (!statement.empty()) {
                statements.push_back(statement);
            }
            start = i + 1;
        }
    }

    if (remainder_out) {
        *remainder_out = input.substr(start);
    }
    return statements;
}

inline std::string valueToString(const fdw::RemoteValue& value,
                                 const std::string& null_display) {
    return std::visit([&](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return null_display;
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int16_t> ||
                             std::is_same_v<T, int32_t> ||
                             std::is_same_v<T, int64_t>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, float> ||
                             std::is_same_v<T, double>) {
            std::ostringstream out;
            out << std::setprecision(15) << arg;
            return out.str();
        } else if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            std::ostringstream out;
            out << "0x" << std::hex << std::setfill('0');
            for (uint8_t byte : arg) {
                out << std::setw(2) << static_cast<int>(byte);
            }
            return out.str();
        } else {
            return null_display;
        }
    }, value);
}

inline void printResultSet(std::ostream& out,
                           const fdw::RemoteQueryResult& result,
                           const OutputConfig& config) {
    if (result.columns.empty()) {
        return;
    }

    std::vector<std::string> headers;
    headers.reserve(result.columns.size());
    for (const auto& col : result.columns) {
        headers.push_back(col.name);
    }

    std::vector<std::vector<std::string>> rows;
    rows.reserve(result.rows.size());
    for (const auto& row : result.rows) {
        std::vector<std::string> values;
        values.reserve(row.size());
        for (const auto& cell : row) {
            values.push_back(valueToString(cell, config.null_display));
        }
        rows.push_back(std::move(values));
    }

    if (config.no_align) {
        auto printLine = [&](const std::vector<std::string>& values) {
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0) {
                    out << config.field_separator;
                }
                out << values[i];
            }
            out << "\n";
        };

        if (!config.tuples_only) {
            printLine(headers);
        }
        for (const auto& values : rows) {
            printLine(values);
        }
    } else {
        std::vector<size_t> widths(headers.size(), 0);
        for (size_t i = 0; i < headers.size(); ++i) {
            widths[i] = headers[i].size();
        }
        for (const auto& values : rows) {
            for (size_t i = 0; i < values.size(); ++i) {
                widths[i] = std::max(widths[i], values[i].size());
            }
        }

        auto printSeparator = [&]() {
            out << "+";
            for (size_t w : widths) {
                for (size_t i = 0; i < w + 2; ++i) {
                    out << "-";
                }
                out << "+";
            }
            out << "\n";
        };

        auto printRow = [&](const std::vector<std::string>& values) {
            out << "|";
            for (size_t i = 0; i < values.size(); ++i) {
                out << " " << std::left << std::setw(static_cast<int>(widths[i])) << values[i] << " |";
            }
            out << "\n";
        };

        if (!config.tuples_only) {
            printSeparator();
            printRow(headers);
            printSeparator();
        }

        for (const auto& values : rows) {
            printRow(values);
        }

        if (!config.tuples_only) {
            printSeparator();
        }
    }

    if (config.show_row_count) {
        out << "(" << rows.size() << " row" << (rows.size() == 1 ? "" : "s") << ")\n";
    }
}

} // namespace cli
} // namespace scratchbird
