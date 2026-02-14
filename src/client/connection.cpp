/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * ScratchBird Client Library Implementation
 *
 * Local Server Architecture - Phase 4
 */

#include "scratchbird/client/connection.h"
#include "scratchbird/client/sql_helpers.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/protocol/wire_protocol.h"
#include "scratchbird/core/firebird_datetime.h"
#include "scratchbird/parser/v3_compiler.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <iomanip>
#include <thread>
#include <iostream>
#include <condition_variable>
#include <unordered_map>
#include <csignal>
#include <fcntl.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#endif

namespace scratchbird {
namespace client {

namespace {
std::string bytesToHex(const std::vector<uint8_t>& data);
std::string formatUuidBytes(const std::vector<uint8_t>& data);

int64_t floorDiv(int64_t a, int64_t b) {
    if (b == 0) {
        return 0;
    }
    int64_t q = a / b;
    int64_t r = a % b;
    if ((r != 0) && ((r < 0) != (b < 0))) {
        --q;
    }
    return q;
}

bool parseTimeString(const std::string& text, int64_t& micros_out, int32_t& offset_seconds_out) {
    offset_seconds_out = 0;
    std::string time_part = text;
    size_t z_pos = time_part.find('Z');
    if (z_pos != std::string::npos) {
        time_part = time_part.substr(0, z_pos);
    }
    size_t offset_pos = time_part.find_last_of("+-");
    if (offset_pos != std::string::npos && offset_pos > 0 && time_part[offset_pos - 1] != 'e') {
        std::string offset = time_part.substr(offset_pos);
        time_part = time_part.substr(0, offset_pos);
        int sign = offset[0] == '-' ? -1 : 1;
        int hours = 0;
        int minutes = 0;
        if (std::sscanf(offset.c_str() + 1, "%d:%d", &hours, &minutes) >= 1) {
            offset_seconds_out = sign * (hours * 3600 + minutes * 60);
        }
    }

    int hour = 0;
    int minute = 0;
    int second = 0;
    int micros = 0;
    size_t dot = time_part.find('.');
    std::string base = time_part;
    std::string frac;
    if (dot != std::string::npos) {
        base = time_part.substr(0, dot);
        frac = time_part.substr(dot + 1);
    }
    if (std::sscanf(base.c_str(), "%d:%d:%d", &hour, &minute, &second) < 2) {
        return false;
    }
    if (!frac.empty()) {
        if (frac.size() > 6) {
            frac.resize(6);
        }
        while (frac.size() < 6) {
            frac.push_back('0');
        }
        micros = std::atoi(frac.c_str());
    }
    micros_out = (static_cast<int64_t>(hour) * 3600 +
                  static_cast<int64_t>(minute) * 60 +
                  static_cast<int64_t>(second)) * 1000000LL + micros;
    return true;
}

int32_t daysSince2000FromDateString(const std::string& text) {
    int32_t mjd = core::FirebirdDateTime::parseDate(text);
    if (mjd < 0) {
        return 0;
    }
    const int32_t base_mjd = core::FirebirdDateTime::dateToMJD(2000, 1, 1);
    return mjd - base_mjd;
}

std::string formatDateFromDaysSince2000(int32_t days_since_2000) {
    const int32_t base_mjd = core::FirebirdDateTime::dateToMJD(2000, 1, 1);
    int32_t mjd = base_mjd + days_since_2000;
    return core::FirebirdDateTime::formatDate(mjd);
}

std::string formatTimeFromMicros(int64_t micros) {
    const int64_t micros_per_day = 24LL * 60LL * 60LL * 1000000LL;
    int64_t normalized = micros % micros_per_day;
    if (normalized < 0) {
        normalized += micros_per_day;
    }
    int64_t total_seconds = normalized / 1000000;
    int64_t micro_remainder = normalized % 1000000;
    int hour = static_cast<int>(total_seconds / 3600);
    int minute = static_cast<int>((total_seconds / 60) % 60);
    int second = static_cast<int>(total_seconds % 60);
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hour << ":"
        << std::setw(2) << minute << ":" << std::setw(2) << second;
    if (micro_remainder != 0) {
        oss << "." << std::setw(6) << micro_remainder;
    }
    return oss.str();
}

std::string formatTimestampFromMicros(int64_t micros) {
    int64_t seconds = floorDiv(micros, 1000000);
    int64_t micro_remainder = micros - seconds * 1000000;
    if (micro_remainder < 0) {
        micro_remainder += 1000000;
        --seconds;
    }
    int64_t days = floorDiv(seconds, 86400);
    int64_t seconds_of_day = seconds - days * 86400;
    if (seconds_of_day < 0) {
        seconds_of_day += 86400;
        --days;
    }
    int hour = static_cast<int>(seconds_of_day / 3600);
    int minute = static_cast<int>((seconds_of_day / 60) % 60);
    int second = static_cast<int>(seconds_of_day % 60);
    int32_t mjd = static_cast<int32_t>(days + core::FirebirdDateTime::UNIX_EPOCH_MJD);
    std::ostringstream oss;
    oss << core::FirebirdDateTime::formatDate(mjd) << " "
        << std::setfill('0') << std::setw(2) << hour << ":"
        << std::setw(2) << minute << ":" << std::setw(2) << second;
    if (micro_remainder != 0) {
        oss << "." << std::setw(6) << micro_remainder;
    }
    return oss.str();
}

std::string bytesToHex(const std::vector<uint8_t>& data) {
    static const char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(data.size() * 2);
    for (uint8_t byte : data) {
        out.push_back(kHex[(byte >> 4) & 0x0F]);
        out.push_back(kHex[byte & 0x0F]);
    }
    return out;
}

std::string formatUuidBytes(const std::vector<uint8_t>& data) {
    if (data.size() != 16) {
        return "";
    }
    char buf[37];
    std::snprintf(buf, sizeof(buf),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  data[0], data[1], data[2], data[3],
                  data[4], data[5], data[6], data[7],
                  data[8], data[9], data[10], data[11],
                  data[12], data[13], data[14], data[15]);
    return std::string(buf);
}

int64_t microsFromTimestampString(const std::string& text) {
    std::string date_part;
    std::string time_part;
    size_t split = text.find('T');
    if (split == std::string::npos) {
        split = text.find(' ');
    }
    if (split == std::string::npos) {
        return 0;
    }
    date_part = text.substr(0, split);
    time_part = text.substr(split + 1);

    int year = 0;
    int month = 0;
    int day = 0;
    if (std::sscanf(date_part.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
        return 0;
    }

    int64_t time_micros = 0;
    int32_t offset_seconds = 0;
    if (!parseTimeString(time_part, time_micros, offset_seconds)) {
        return 0;
    }

    int32_t mjd = core::FirebirdDateTime::dateToMJD(year, month, day);
    int64_t base_seconds =
        static_cast<int64_t>(mjd - core::FirebirdDateTime::UNIX_EPOCH_MJD) * 86400;
    int64_t total_seconds = base_seconds + time_micros / 1000000;
    int64_t micros = (time_micros % 1000000);
    if (micros < 0) {
        micros += 1000000;
        --total_seconds;
    }
    total_seconds -= offset_seconds;
    return total_seconds * 1000000 + micros;
}
} // namespace

// Helper to check if status is OK
inline bool isOk(core::Status status) {
    return status == core::Status::OK;
}

// ============================================================================
// ResultSet Implementation
// ============================================================================

class ResultSetImpl {
public:
    std::vector<ColumnMeta> columns_;
    std::vector<std::vector<protocol::ProtocolCodec::ColumnValue>> rows_;
    int64_t current_row_ = -1;
    int64_t row_count_ = -1;
    int64_t rows_affected_ = 0;
    std::string command_tag_;

    void clear() {
        columns_.clear();
        rows_.clear();
        current_row_ = -1;
        row_count_ = -1;
        rows_affected_ = 0;
        command_tag_.clear();
    }
};

ResultSet::ResultSet() : impl_(std::make_unique<ResultSetImpl>()) {}
ResultSet::~ResultSet() = default;

ResultSet::ResultSet(ResultSet&& other) noexcept = default;
ResultSet& ResultSet::operator=(ResultSet&& other) noexcept = default;

size_t ResultSet::getColumnCount() const {
    return impl_->columns_.size();
}

std::string ResultSet::getColumnName(size_t index) const {
    if (index >= impl_->columns_.size()) return "";
    return impl_->columns_[index].name;
}

int ResultSet::getColumnIndex(const std::string& name) const {
    // Case-insensitive comparison
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

    for (size_t i = 0; i < impl_->columns_.size(); ++i) {
        std::string col_lower = impl_->columns_[i].name;
        std::transform(col_lower.begin(), col_lower.end(), col_lower.begin(), ::tolower);
        if (col_lower == lower_name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

protocol::WireType ResultSet::getColumnType(size_t index) const {
    if (index >= impl_->columns_.size()) return protocol::WireType::UNKNOWN;
    return impl_->columns_[index].type;
}

const std::vector<ColumnMeta>& ResultSet::getColumns() const {
    return impl_->columns_;
}

int64_t ResultSet::getRowCount() const {
    return impl_->row_count_ >= 0 ? impl_->row_count_ : static_cast<int64_t>(impl_->rows_.size());
}

int64_t ResultSet::getRowsAffected() const {
    return impl_->rows_affected_;
}

bool ResultSet::isEmpty() const {
    return impl_->rows_.empty();
}

const std::vector<protocol::ProtocolCodec::ColumnValue>& ResultSet::getRowValues(size_t index) const {
    static const std::vector<protocol::ProtocolCodec::ColumnValue> kEmptyRow;
    if (index >= impl_->rows_.size()) {
        return kEmptyRow;
    }
    return impl_->rows_[index];
}

const std::string& ResultSet::getCommandTag() const {
    return impl_->command_tag_;
}

bool ResultSet::next() {
    if (impl_->current_row_ + 1 < static_cast<int64_t>(impl_->rows_.size())) {
        ++impl_->current_row_;
        return true;
    }
    return false;
}

void ResultSet::reset() {
    impl_->current_row_ = -1;
}

int64_t ResultSet::getCurrentRow() const {
    return impl_->current_row_;
}

bool ResultSet::isNull(size_t column) const {
    if (impl_->current_row_ < 0 ||
        impl_->current_row_ >= static_cast<int64_t>(impl_->rows_.size()) ||
        column >= impl_->rows_[impl_->current_row_].size()) {
        return true;
    }
    return impl_->rows_[impl_->current_row_][column].is_null;
}

bool ResultSet::getBool(size_t column) const {
    if (isNull(column)) return false;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.empty()) return false;
    return val.data[0] != 0;
}

int16_t ResultSet::getInt16(size_t column) const {
    if (isNull(column)) return 0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() < 2) return 0;
    int16_t result;
    std::memcpy(&result, val.data.data(), sizeof(result));
    return result;
}

int32_t ResultSet::getInt32(size_t column) const {
    if (isNull(column)) return 0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() < 4) return 0;
    int32_t result;
    std::memcpy(&result, val.data.data(), sizeof(result));
    return result;
}

int64_t ResultSet::getInt64(size_t column) const {
    if (isNull(column)) return 0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() < 8) return 0;
    int64_t result;
    std::memcpy(&result, val.data.data(), sizeof(result));
    return result;
}

float ResultSet::getFloat(size_t column) const {
    if (isNull(column)) return 0.0f;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() < 4) return 0.0f;
    float result;
    std::memcpy(&result, val.data.data(), sizeof(result));
    return result;
}

double ResultSet::getDouble(size_t column) const {
    if (isNull(column)) return 0.0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() < 8) return 0.0;
    double result;
    std::memcpy(&result, val.data.data(), sizeof(result));
    return result;
}

std::string ResultSet::getString(size_t column) const {
    if (isNull(column)) return "";
    const auto& val = impl_->rows_[impl_->current_row_][column];
    protocol::WireType type = protocol::WireType::UNKNOWN;
    if (column < impl_->columns_.size()) {
        type = impl_->columns_[column].type;
    }
    switch (type) {
        case protocol::WireType::BOOLEAN:
            return (!val.data.empty() && val.data[0]) ? "true" : "false";
        case protocol::WireType::INT16: {
            if (val.data.size() < sizeof(int16_t)) return "0";
            int16_t out;
            std::memcpy(&out, val.data.data(), sizeof(out));
            return std::to_string(out);
        }
        case protocol::WireType::INT32: {
            if (val.data.size() < sizeof(int32_t)) return "0";
            int32_t out;
            std::memcpy(&out, val.data.data(), sizeof(out));
            return std::to_string(out);
        }
        case protocol::WireType::INT64: {
            if (val.data.size() < sizeof(int64_t)) return "0";
            int64_t out;
            std::memcpy(&out, val.data.data(), sizeof(out));
            return std::to_string(out);
        }
        case protocol::WireType::FLOAT32:
        case protocol::WireType::FLOAT64: {
            if (val.data.size() < sizeof(double)) return "0";
            double out;
            std::memcpy(&out, val.data.data(), sizeof(out));
            std::ostringstream oss;
            oss.precision(17);
            oss << out;
            return oss.str();
        }
        case protocol::WireType::DATE:
            return formatDateFromDaysSince2000(getDate(column));
        case protocol::WireType::TIME:
            return formatTimeFromMicros(getTime(column));
        case protocol::WireType::TIMESTAMP:
        case protocol::WireType::TIMESTAMPTZ:
            return formatTimestampFromMicros(getTimestamp(column));
        case protocol::WireType::UUID:
            return getUUID(column);
        case protocol::WireType::BYTEA:
            return bytesToHex(val.data);
        default:
            break;
    }
    return std::string(reinterpret_cast<const char*>(val.data.data()), val.data.size());
}

std::vector<uint8_t> ResultSet::getBytes(size_t column) const {
    if (isNull(column)) return {};
    return impl_->rows_[impl_->current_row_][column].data;
}

int64_t ResultSet::getTimestamp(size_t column) const {
    if (isNull(column)) return 0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    protocol::WireType type = protocol::WireType::UNKNOWN;
    if (column < impl_->columns_.size()) {
        type = impl_->columns_[column].type;
    }
    if (type != protocol::WireType::TIMESTAMP &&
        type != protocol::WireType::TIMESTAMPTZ &&
        val.data.size() >= sizeof(int64_t)) {
        int64_t out;
        std::memcpy(&out, val.data.data(), sizeof(out));
        return out;
    }
    if (val.data.size() == sizeof(int64_t) &&
        (type == protocol::WireType::TIMESTAMP || type == protocol::WireType::TIMESTAMPTZ)) {
        int64_t out;
        std::memcpy(&out, val.data.data(), sizeof(out));
        return out;
    }
    std::string text(reinterpret_cast<const char*>(val.data.data()), val.data.size());
    return microsFromTimestampString(text);
}

int32_t ResultSet::getDate(size_t column) const {
    if (isNull(column)) return 0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    protocol::WireType type = protocol::WireType::UNKNOWN;
    if (column < impl_->columns_.size()) {
        type = impl_->columns_[column].type;
    }
    if (type != protocol::WireType::DATE && val.data.size() >= sizeof(int32_t)) {
        int32_t out;
        std::memcpy(&out, val.data.data(), sizeof(out));
        return out;
    }
    if (val.data.size() == sizeof(int32_t) && type == protocol::WireType::DATE) {
        int32_t out;
        std::memcpy(&out, val.data.data(), sizeof(out));
        return out;
    }
    std::string text(reinterpret_cast<const char*>(val.data.data()), val.data.size());
    return daysSince2000FromDateString(text);
}

int64_t ResultSet::getTime(size_t column) const {
    if (isNull(column)) return 0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    protocol::WireType type = protocol::WireType::UNKNOWN;
    if (column < impl_->columns_.size()) {
        type = impl_->columns_[column].type;
    }
    if (type != protocol::WireType::TIME && val.data.size() >= sizeof(int64_t)) {
        int64_t out;
        std::memcpy(&out, val.data.data(), sizeof(out));
        return out;
    }
    if (val.data.size() == sizeof(int64_t) && type == protocol::WireType::TIME) {
        int64_t out;
        std::memcpy(&out, val.data.data(), sizeof(out));
        return out;
    }
    std::string text(reinterpret_cast<const char*>(val.data.data()), val.data.size());
    int64_t micros = 0;
    int32_t offset_seconds = 0;
    if (!parseTimeString(text, micros, offset_seconds)) {
        return 0;
    }
    return micros - static_cast<int64_t>(offset_seconds) * 1000000LL;
}

std::string ResultSet::getUUID(size_t column) const {
    if (isNull(column)) return "";
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() == 16) {
        return formatUuidBytes(val.data);
    }
    return std::string(reinterpret_cast<const char*>(val.data.data()), val.data.size());
}

const uint8_t* ResultSet::getRaw(size_t column, size_t* length) const {
    if (isNull(column)) {
        if (length) *length = 0;
        return nullptr;
    }
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (length) *length = val.data.size();
    return val.data.data();
}

// By-name accessors
bool ResultSet::isNull(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx < 0 || isNull(static_cast<size_t>(idx));
}

bool ResultSet::getBool(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getBool(static_cast<size_t>(idx)) : false;
}

int16_t ResultSet::getInt16(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getInt16(static_cast<size_t>(idx)) : 0;
}

int32_t ResultSet::getInt32(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getInt32(static_cast<size_t>(idx)) : 0;
}

int64_t ResultSet::getInt64(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getInt64(static_cast<size_t>(idx)) : 0;
}

float ResultSet::getFloat(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getFloat(static_cast<size_t>(idx)) : 0.0f;
}

double ResultSet::getDouble(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getDouble(static_cast<size_t>(idx)) : 0.0;
}

std::string ResultSet::getString(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getString(static_cast<size_t>(idx)) : "";
}

std::vector<uint8_t> ResultSet::getBytes(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getBytes(static_cast<size_t>(idx)) : std::vector<uint8_t>{};
}

int64_t ResultSet::getTimestamp(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getTimestamp(static_cast<size_t>(idx)) : 0;
}

int32_t ResultSet::getDate(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getDate(static_cast<size_t>(idx)) : 0;
}

int64_t ResultSet::getTime(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getTime(static_cast<size_t>(idx)) : 0;
}

std::string ResultSet::getUUID(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getUUID(static_cast<size_t>(idx)) : "";
}

// ============================================================================
// PreparedStatement Implementation
// ============================================================================

class PreparedStatementImpl {
public:
    std::string sql_;
    uint32_t statement_id_ = 0;
    size_t param_count_ = 0;
    std::vector<protocol::ProtocolCodec::ColumnValue> params_;
    std::vector<protocol::WireType> param_types_;
    bool valid_ = false;
};

PreparedStatement::PreparedStatement() : impl_(std::make_unique<PreparedStatementImpl>()) {}
PreparedStatement::~PreparedStatement() = default;

PreparedStatement::PreparedStatement(PreparedStatement&& other) noexcept = default;
PreparedStatement& PreparedStatement::operator=(PreparedStatement&& other) noexcept = default;

const std::string& PreparedStatement::getSQL() const {
    return impl_->sql_;
}

size_t PreparedStatement::getParameterCount() const {
    return impl_->param_count_;
}

bool PreparedStatement::isValid() const {
    return impl_->valid_;
}

void PreparedStatement::setNull(size_t index) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue(nullptr);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::NULL_TYPE;
    }
}

void PreparedStatement::setNull(size_t index, protocol::WireType type) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue(nullptr);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = type;
    }
}

void PreparedStatement::setBool(size_t index, bool value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromBool(value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::BOOLEAN;
    }
}

void PreparedStatement::setInt16(size_t index, int16_t value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromInt32(value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::INT16;
    }
}

void PreparedStatement::setInt32(size_t index, int32_t value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromInt32(value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::INT32;
    }
}

void PreparedStatement::setInt64(size_t index, int64_t value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromInt64(value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::INT64;
    }
}

void PreparedStatement::setFloat(size_t index, float value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromDouble(value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::FLOAT32;
    }
}

void PreparedStatement::setDouble(size_t index, double value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromDouble(value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::FLOAT64;
    }
}

void PreparedStatement::setString(size_t index, const std::string& value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromString(value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::VARCHAR;
    }
}

void PreparedStatement::setBytes(size_t index, const std::vector<uint8_t>& value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromBytes(value.data(), value.size());
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::BYTEA;
    }
}

void PreparedStatement::setBytes(size_t index, const uint8_t* data, size_t length) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromBytes(data, length);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::BYTEA;
    }
}

void PreparedStatement::setTimestamp(size_t index, int64_t microseconds) {
    setInt64(index, microseconds);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::TIMESTAMP;
    }
}

void PreparedStatement::setDate(size_t index, int32_t days) {
    setInt32(index, days);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::DATE;
    }
}

void PreparedStatement::setTime(size_t index, int64_t microseconds) {
    setInt64(index, microseconds);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::TIME;
    }
}

void PreparedStatement::setUUID(size_t index, const std::vector<uint8_t>& value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromBytes(value.data(), value.size());
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::UUID;
    }
}

void PreparedStatement::setUUID(size_t index, const std::string& value) {
    setString(index, value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::UUID;
    }
}

void PreparedStatement::clearParameters() {
    for (auto& p : impl_->params_) {
        p = protocol::ProtocolCodec::ColumnValue(nullptr);
    }
    for (auto& t : impl_->param_types_) {
        t = protocol::WireType::UNKNOWN;
    }
}

// ============================================================================
// Connection Implementation
// ============================================================================

class ConnectionImpl {
public:
    ConnectionConfig config_;
    ConnectionState state_ = ConnectionState::DISCONNECTED;
    std::string last_error_;

    // IPC client and protocol session
    std::unique_ptr<server::IPCClient> ipc_client_;
    std::unique_ptr<protocol::ProtocolSession> protocol_session_;

    // Session info
    uint8_t session_id_[16] = {0};
    std::string server_version_;
    bool in_transaction_ = false;
    bool auto_commit_ = true;
    std::istream* copy_input_stream_ = nullptr;
    std::ostream* copy_output_stream_ = nullptr;
    std::function<void(uint64_t, uint64_t)> progress_callback_;

    // ============================
    // Connection helpers
    // ============================

    core::Status doConnect(core::ErrorContext* ctx) {
        // Check if server is running, with retry for race conditions
        // Two clients might both see no server and both try to start one.
        // The first one wins the database lock, the second one should connect to it.
        if (config_.auto_start_server) {
            int max_retries = 3;
            for (int retry = 0; retry < max_retries; ++retry) {
                if (server::isServerRunning(config_.database_name)) {
                    break;  // Server is running, proceed to connect
                }

                auto status = Connection::startServer(
                    config_.database_name,
                    config_.server_executable,
                    config_.auto_start_timeout_ms,
                    ctx
                );

                if (isOk(status)) {
                    break;  // Server started successfully
                }

                // Server start failed - might be because another client started one
                // Wait briefly and check if a server is now running
                std::this_thread::sleep_for(std::chrono::milliseconds(200 * (retry + 1)));

                if (server::isServerRunning(config_.database_name)) {
                    break;  // Another client started the server, we can connect to it
                }

                // Last retry - report the failure
                if (retry == max_retries - 1) {
                    last_error_ = "Failed to auto-start server (database may be locked by another process)";
                    return status;
                }
            }
        }

        // Create IPC client
        server::IPCClientConfig ipc_config;
        ipc_config.database_name = config_.database_name;
        ipc_config.method = config_.ipc_method;
        ipc_config.tcp_port = config_.tcp_port;
        ipc_config.connect_timeout_ms = config_.connect_timeout_ms;
        ipc_config.read_timeout_ms = config_.read_timeout_ms;
        ipc_config.write_timeout_ms = config_.write_timeout_ms;
        if (!config_.socket_path.empty()) {
            ipc_config.socket_path = config_.socket_path;
        }

        ipc_client_ = server::IPCClient::create(ipc_config, ctx);
        if (!ipc_client_) {
            last_error_ = "Failed to create IPC client";
            return core::Status::CONNECTION_FAILURE;
        }

        // Connect to server
        state_ = ConnectionState::CONNECTING;
        auto status = ipc_client_->connect(ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to connect to server";
            state_ = ConnectionState::ERROR_STATE;
            return status;
        }

        // Create protocol session
        protocol_session_ = std::make_unique<protocol::ProtocolSession>(
            ipc_client_->getConnection()
        );

        // Send CONNECT_REQUEST
        auto connect_msg = protocol::ProtocolCodec::buildConnectRequest(
            config_.database_name,
            "scratchbird_client",
            getpid()
        );

        status = protocol_session_->sendMessage(connect_msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send connect request";
            state_ = ConnectionState::ERROR_STATE;
            return status;
        }

        // Receive CONNECT_RESPONSE
        protocol::Message response;
        status = protocol_session_->receiveMessage(response, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive connect response";
            state_ = ConnectionState::ERROR_STATE;
            return status;
        }

        if (response.getType() != protocol::MessageType::CONNECT_RESPONSE) {
            last_error_ = "Unexpected response type";
            state_ = ConnectionState::ERROR_STATE;
            return core::Status::PROTOCOL_VIOLATION;
        }

        bool success;
        std::string error_msg;
        status = protocol::ProtocolCodec::parseConnectResponse(
            response, success, session_id_, error_msg, nullptr, ctx
        );
        if (!isOk(status) || !success) {
            last_error_ = error_msg.empty() ? "Connection refused" : error_msg;
            state_ = ConnectionState::ERROR_STATE;
            return core::Status::CONNECTION_FAILURE;
        }

        // Authenticate if credentials provided (or bootstrap on fresh DB).
        if (!config_.manual_auth) {
            if (config_.username.empty()) {
                config_.username = "bootstrap";
            }
            status = doAuthenticate(ctx);
            if (!isOk(status)) {
                return status;
            }
        }

        state_ = ConnectionState::CONNECTED;
        auto_commit_ = config_.auto_commit;
        return core::Status::OK;
    }

    core::Status doAuthenticate(core::ErrorContext* ctx) {
        auto auth_msg = protocol::ProtocolCodec::buildAuthRequest(
            session_id_,
            config_.username,
            config_.password
        );

        auto status = protocol_session_->sendMessage(auth_msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send auth request";
            return status;
        }

        protocol::Message response;
        status = protocol_session_->receiveMessage(response, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive auth response";
            return status;
        }

        if (response.getType() != protocol::MessageType::AUTH_RESPONSE) {
            last_error_ = "Unexpected response type";
            return core::Status::PROTOCOL_VIOLATION;
        }

        bool success;
        uint32_t user_id;
        std::string error_msg;
        status = protocol::ProtocolCodec::parseAuthResponse(
            response, success, user_id, error_msg, ctx
        );
        if (!isOk(status) || !success) {
            last_error_ = error_msg.empty() ? "Authentication failed" : error_msg;
            return core::Status::INVALID_PASSWORD;
        }

        return core::Status::OK;
    }

    core::Status doSendAuthRequest(protocol::AuthMethod method,
                                   const std::vector<uint8_t>& payload,
                                   Connection::AuthResponse& response,
                                   core::ErrorContext* ctx) {
        if (!protocol_session_) {
            last_error_ = "Not connected";
            return core::Status::CONNECTION_FAILURE;
        }

        auto auth_msg = protocol::ProtocolCodec::buildAuthRequest(
            session_id_,
            config_.username,
            method,
            payload
        );

        auto status = protocol_session_->sendMessage(auth_msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send auth request";
            return status;
        }

        protocol::Message response_msg;
        status = protocol_session_->receiveMessage(response_msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive auth response";
            return status;
        }

        if (response_msg.getType() != protocol::MessageType::AUTH_RESPONSE) {
            last_error_ = "Unexpected response type";
            return core::Status::PROTOCOL_VIOLATION;
        }

        protocol::AuthStatus auth_status = protocol::AuthStatus::ERROR;
        uint32_t user_id;
        std::string error_msg;
        std::vector<uint8_t> data;
        status = protocol::ProtocolCodec::parseAuthResponse(
            response_msg, auth_status, user_id, error_msg, &data, ctx
        );
        if (!isOk(status)) {
            last_error_ = "Failed to parse auth response";
            return status;
        }

        response.status = auth_status;
        response.user_id = user_id;
        response.error_message = error_msg;
        response.data = std::move(data);
        return core::Status::OK;
    }

    core::Status doExecuteQueryMessage(const protocol::Message& query_msg,
                                       ResultSet* results,
                                       core::ErrorContext* ctx) {
        if (results) {
            results->impl_->clear();
        }
        if (ctx)
        {
            ctx->code = core::Status::OK;
            ctx->sqlstate = core::SQLSTATE_SUCCESS;
            ctx->message.clear();
        }

        auto status = protocol_session_->sendMessage(query_msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send query";
            std::fprintf(stderr,
                         "[ipc_debug] client send query failed status=%d msg=%s\n",
                         static_cast<int>(status),
                         ctx && !ctx->message.empty() ? ctx->message.c_str() : "none");
            return status;
        }

        const uint32_t copy_window = config_.copy_window_bytes == 0 ? 65536 : config_.copy_window_bytes;
        const uint32_t copy_chunk = config_.copy_chunk_bytes == 0 ? 16384 : config_.copy_chunk_bytes;

        auto handle_copy_out = [&]() -> core::Status {
            std::ostream* out = copy_output_stream_ ? copy_output_stream_ : &std::cout;
            uint32_t window = 0;
            bool stream_ready = false;

            while (true) {
                protocol::Message response;
                auto status = protocol_session_->receiveMessage(response, ctx);
                if (!isOk(status)) {
                    last_error_ = "Failed to receive COPY OUT response";
                    return status;
                }

                switch (response.getType()) {
                    case protocol::MessageType::STREAM_READY: {
                        stream_ready = true;
                        window = copy_window;
                        auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                            protocol::StreamControlType::START, window, 0);
                        status = protocol_session_->sendMessage(ctrl, ctx);
                        if (!isOk(status)) {
                            last_error_ = "Failed to send STREAM_CONTROL";
                            return status;
                        }
                        break;
                    }
                    case protocol::MessageType::COPY_DATA: {
                        const uint8_t* data = nullptr;
                        size_t len = 0;
                        protocol::ProtocolCodec::parseCopyData(response, &data, &len, ctx);
                        if (len > 0) {
                            out->write(reinterpret_cast<const char*>(data),
                                       static_cast<std::streamsize>(len));
                            if (!(*out)) {
                                last_error_ = "COPY OUT write failed";
                                return core::Status::IO_ERROR;
                            }
                        }
                        if (window > 0) {
                            if (len >= window) {
                                window = 0;
                            } else {
                                window -= static_cast<uint32_t>(len);
                            }
                        }
                        if (stream_ready && window == 0) {
                            window = copy_window;
                            auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                                protocol::StreamControlType::ACK, window, 0);
                            status = protocol_session_->sendMessage(ctrl, ctx);
                            if (!isOk(status)) {
                                last_error_ = "Failed to send STREAM_CONTROL ACK";
                                return status;
                            }
                        }
                        break;
                    }
                    case protocol::MessageType::COPY_DONE:
                        return core::Status::OK;

                    case protocol::MessageType::COPY_FAIL: {
                        std::string message;
                        protocol::ProtocolCodec::parseCopyFail(response, message, ctx);
                        last_error_ = message.empty() ? "COPY OUT failed" : message;
                        return core::Status::INTERNAL_ERROR;
                    }
                    case protocol::MessageType::QUERY_ERROR: {
                        uint32_t error_code;
                        std::string sqlstate, message, detail, hint;
                        protocol::ProtocolCodec::parseQueryError(
                            response, error_code, sqlstate, message, detail, hint, ctx
                        );
                        if (ctx)
                        {
                            ctx->set(static_cast<core::Status>(error_code),
                                     message.c_str(), __FILE__, __LINE__, __func__);
                        }
                        last_error_ = message;
                        if (!detail.empty()) last_error_ += " (" + detail + ")";
                        return static_cast<core::Status>(error_code);
                    }
                    case protocol::MessageType::STREAM_END:
                        break;

                    default:
                        break;
                }
            }
        };

        auto handle_copy_in = [&]() -> core::Status {
            std::istream* in = copy_input_stream_ ? copy_input_stream_ : &std::cin;
            uint32_t window = 0;
            bool done = false;
            bool stream_started = false;

            while (!done) {
                if (window == 0) {
                    protocol::Message response;
                    auto status = protocol_session_->receiveMessage(response, ctx);
                    if (!isOk(status)) {
                        last_error_ = "Failed to receive COPY IN control";
                        return status;
                    }

                    switch (response.getType()) {
                        case protocol::MessageType::STREAM_READY:
                            if (!stream_started) {
                                window = copy_window;
                                auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                                    protocol::StreamControlType::START, window, 0);
                                status = protocol_session_->sendMessage(ctrl, ctx);
                                if (!isOk(status)) {
                                    last_error_ = "Failed to send STREAM_CONTROL START";
                                    return status;
                                }
                                stream_started = true;
                            }
                            break;
                        case protocol::MessageType::STREAM_CONTROL: {
                            protocol::StreamControlType control;
                            uint32_t new_window = 0;
                            uint32_t timeout_ms = 0;
                            status = protocol::ProtocolCodec::parseStreamControl(
                                response, control, new_window, timeout_ms, ctx);
                            if (!isOk(status)) {
                                last_error_ = "Malformed STREAM_CONTROL";
                                return status;
                            }
                            (void)timeout_ms;
                            if (control == protocol::StreamControlType::PAUSE) {
                                break;
                            }
                            if (control == protocol::StreamControlType::CANCEL) {
                                last_error_ = "COPY IN canceled by server";
                                return core::Status::CANCELLED;
                            }
                            window += new_window;
                            break;
                        }
                        case protocol::MessageType::COPY_FAIL: {
                            std::string message;
                            protocol::ProtocolCodec::parseCopyFail(response, message, ctx);
                            last_error_ = message.empty() ? "COPY IN failed" : message;
                            return core::Status::INTERNAL_ERROR;
                        }
                        case protocol::MessageType::QUERY_ERROR: {
                            uint32_t error_code;
                            std::string sqlstate, message, detail, hint;
                            protocol::ProtocolCodec::parseQueryError(
                                response, error_code, sqlstate, message, detail, hint, ctx
                            );
                            if (ctx)
                            {
                                ctx->set(static_cast<core::Status>(error_code),
                                         message.c_str(), __FILE__, __LINE__, __func__);
                            }
                            last_error_ = message;
                            if (!detail.empty()) last_error_ += " (" + detail + ")";
                            return static_cast<core::Status>(error_code);
                        }
                        default:
                            break;
                    }
                    if (window == 0) {
                        continue;
                    }
                }

                size_t to_read = std::min<size_t>(window, copy_chunk);
                std::string buffer(to_read, '\0');
                in->read(buffer.data(), static_cast<std::streamsize>(to_read));
                std::streamsize got = in->gcount();

                if (got > 0) {
                    auto msg = protocol::ProtocolCodec::buildCopyData(
                        reinterpret_cast<const uint8_t*>(buffer.data()),
                        static_cast<size_t>(got));
                    auto status = protocol_session_->sendMessage(msg, ctx);
                    if (!isOk(status)) {
                        last_error_ = "Failed to send COPY_DATA";
                        return status;
                    }
                    if (got >= static_cast<std::streamsize>(window)) {
                        window = 0;
                    } else {
                        window -= static_cast<uint32_t>(got);
                    }
                } else {
                    auto msg = protocol::ProtocolCodec::buildCopyDone();
                    auto status = protocol_session_->sendMessage(msg, ctx);
                    if (!isOk(status)) {
                        last_error_ = "Failed to send COPY_DONE";
                        return status;
                    }
                    done = true;
                }
            }
            return core::Status::OK;
        };

        struct StreamBuffer {
            std::vector<uint8_t> data;
            uint64_t expected_bytes = 0;
            bool complete = false;
        };
        std::unordered_map<uint64_t, StreamBuffer> stream_buffers;
        std::unordered_map<uint64_t, std::vector<std::pair<size_t, size_t>>> stream_bindings;
        uint32_t stream_window = 0;

        auto apply_stream_data = [&](uint64_t stream_id) {
            if (!results) {
                return;
            }
            auto buffer_it = stream_buffers.find(stream_id);
            if (buffer_it == stream_buffers.end() || !buffer_it->second.complete) {
                return;
            }
            auto binding_it = stream_bindings.find(stream_id);
            if (binding_it == stream_bindings.end()) {
                return;
            }
            const auto& data = buffer_it->second.data;
            for (const auto& binding : binding_it->second) {
                if (binding.first >= results->impl_->rows_.size()) {
                    continue;
                }
                auto& row = results->impl_->rows_[binding.first];
                if (binding.second >= row.size()) {
                    continue;
                }
                row[binding.second].data = data;
                row[binding.second].is_stream = false;
            }
            stream_bindings.erase(binding_it);
            stream_buffers.erase(buffer_it);
        };

        while (true) {
            protocol::Message response;
            status = protocol_session_->receiveMessage(response, ctx);
            if (!isOk(status)) {
                last_error_ = "Failed to receive response";
                std::fprintf(stderr,
                             "[ipc_debug] client receive response failed status=%d msg=%s\n",
                             static_cast<int>(status),
                             ctx && !ctx->message.empty() ? ctx->message.c_str() : "none");
                return status;
            }

            switch (response.getType()) {
                case protocol::MessageType::QUERY_ERROR: {
                    uint32_t error_code;
                    std::string sqlstate, message, detail, hint;
                    protocol::ProtocolCodec::parseQueryError(
                        response, error_code, sqlstate, message, detail, hint, ctx
                    );
                    last_error_ = message;
                    if (!detail.empty()) last_error_ += " (" + detail + ")";
                    return static_cast<core::Status>(error_code);
                }

                case protocol::MessageType::ROW_DESCRIPTION: {
                    if (results) {
                        std::vector<protocol::ProtocolCodec::ColumnInfo> cols;
                        protocol::ProtocolCodec::parseRowDescription(response, cols, ctx);
                        results->impl_->columns_.clear();
                        for (size_t i = 0; i < cols.size(); ++i) {
                            ColumnMeta meta;
                            meta.name = cols[i].name;
                            meta.type = cols[i].type;
                            meta.type_modifier = cols[i].type_modifier;
                            meta.index = i;
                            results->impl_->columns_.push_back(meta);
                        }
                    }
                    break;
                }

                case protocol::MessageType::ROW_DATA: {
                    if (results) {
                        std::vector<protocol::ProtocolCodec::ColumnValue> values;
                        protocol::ProtocolCodec::parseRowData(response, values, ctx);
                        const size_t row_index = results->impl_->rows_.size();
                        for (size_t i = 0; i < values.size(); ++i) {
                            if (!values[i].is_stream) {
                                continue;
                            }
                            stream_bindings[values[i].stream_id].push_back({row_index, i});
                            auto buffer_it = stream_buffers.find(values[i].stream_id);
                            if (buffer_it != stream_buffers.end() && buffer_it->second.complete) {
                                values[i].data = buffer_it->second.data;
                                values[i].is_stream = false;
                            }
                        }
                        results->impl_->rows_.push_back(std::move(values));
                    }
                    break;
                }

                case protocol::MessageType::COMMAND_COMPLETE: {
                    if (results) {
                        std::string tag;
                        int64_t rows_affected;
                        protocol::ProtocolCodec::parseCommandComplete(
                            response, tag, rows_affected, ctx
                        );
                        results->impl_->command_tag_ = tag;
                        results->impl_->rows_affected_ = rows_affected;
                    }
                    break;
                }

                case protocol::MessageType::END_OF_RESULTS: {
                    if (results) {
                        results->impl_->row_count_ = static_cast<int64_t>(results->impl_->rows_.size());
                    }
                    return core::Status::OK;
                }
                case protocol::MessageType::PORTAL_SUSPENDED: {
                    if (results) {
                        results->impl_->row_count_ = static_cast<int64_t>(results->impl_->rows_.size());
                    }
                    return core::Status::OK;
                }

                case protocol::MessageType::COPY_IN_RESPONSE: {
                    status = handle_copy_in();
                    if (!isOk(status)) {
                        return status;
                    }
                    break;
                }
                case protocol::MessageType::COPY_OUT_RESPONSE: {
                    status = handle_copy_out();
                    if (!isOk(status)) {
                        return status;
                    }
                    break;
                }
                case protocol::MessageType::COPY_FAIL: {
                    std::string message;
                    protocol::ProtocolCodec::parseCopyFail(response, message, ctx);
                    last_error_ = message.empty() ? "COPY failed" : message;
                    return core::Status::INTERNAL_ERROR;
                }
                case protocol::MessageType::STREAM_READY: {
                    uint64_t stream_id = 0;
                    uint64_t total_rows = 0;
                    uint64_t estimated_bytes = 0;
                    protocol::ProtocolCodec::parseStreamReady(response, stream_id,
                                                              total_rows, estimated_bytes, ctx);
                    (void)total_rows;
                    stream_window = copy_window;
                    auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                        protocol::StreamControlType::START, stream_window, 0);
                    status = protocol_session_->sendMessage(ctrl, ctx);
                    if (!isOk(status)) {
                        last_error_ = "Failed to send STREAM_CONTROL";
                        return status;
                    }
                    if (results) {
                        auto& buffer = stream_buffers[stream_id];
                        buffer.expected_bytes = estimated_bytes;
                    }
                    break;
                }
                case protocol::MessageType::STREAM_DATA: {
                    uint64_t stream_id = 0;
                    uint32_t chunk_rows = 0;
                    const uint8_t* data = nullptr;
                    size_t len = 0;
                    protocol::ProtocolCodec::parseStreamData(
                        response, stream_id, chunk_rows, &data, &len, ctx);
                    (void)chunk_rows;
                    if (results && data && len > 0) {
                        auto& buffer = stream_buffers[stream_id];
                        buffer.data.insert(buffer.data.end(), data, data + len);
                    }
                    if (stream_window > 0) {
                        if (len >= stream_window) {
                            stream_window = 0;
                        } else {
                            stream_window -= static_cast<uint32_t>(len);
                        }
                    }
                    if (stream_window == 0) {
                        stream_window = copy_window;
                        auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                            protocol::StreamControlType::ACK, stream_window, 0);
                        status = protocol_session_->sendMessage(ctrl, ctx);
                        if (!isOk(status)) {
                            last_error_ = "Failed to send STREAM_CONTROL ACK";
                            return status;
                        }
                    }
                    break;
                }
                case protocol::MessageType::STREAM_END: {
                    uint64_t stream_id = 0;
                    uint64_t total_rows = 0;
                    uint64_t total_bytes = 0;
                    protocol::ProtocolCodec::parseStreamEnd(response, stream_id,
                                                            total_rows, total_bytes, ctx);
                    (void)total_rows;
                    (void)total_bytes;
                    if (results) {
                        auto& buffer = stream_buffers[stream_id];
                        buffer.complete = true;
                        apply_stream_data(stream_id);
                    }
                    stream_window = 0;
                    break;
                }
                case protocol::MessageType::QUERY_PROGRESS: {
                    uint64_t rows = 0;
                    uint64_t bytes = 0;
                    protocol::ProtocolCodec::parseQueryProgress(response, rows, bytes, ctx);
                    if (progress_callback_) {
                        progress_callback_(rows, bytes);
                    }
                    break;
                }

                case protocol::MessageType::TRANSACTION_STATUS: {
                    // NET-L1: Parse transaction status and update connection state
                    if (response.getPayloadSize() >= sizeof(protocol::TransactionStatusPayload)) {
                        const auto* ts_payload = reinterpret_cast<const protocol::TransactionStatusPayload*>(
                            response.getPayload());
                        // Status: 0 = idle, 1 = in transaction, 2 = failed
                        in_transaction_ = (ts_payload->status == 1);
                        if (ts_payload->status == 2) {
                            // Transaction failed - reset to idle
                            state_ = ConnectionState::CONNECTED;
                        } else if (ts_payload->status == 1) {
                            state_ = ConnectionState::IN_TRANSACTION;
                        } else {
                            state_ = ConnectionState::CONNECTED;
                        }
                    }
                    break;
                }

                default:
                    // Ignore unexpected messages
                    break;
            }
        }
    }

    core::Status doExecuteQuery(const std::string& sql, uint8_t flags, ResultSet* results,
                                core::ErrorContext* ctx) {
        parser::v3::Compiler compiler;
        auto compile_result = compiler.compile(sql);
        if (!compile_result.ok) {
            last_error_ = compile_result.error.empty()
                ? "Compilation failed before submit"
                : compile_result.error;
            if (ctx) {
                ctx->set(core::Status::INVALID_ARGUMENT, last_error_.c_str(),
                         __FILE__, __LINE__, __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }

        auto query_msg = protocol::ProtocolCodec::buildQueryBytecode(
            session_id_, compile_result.bytecode, sql, flags);
        return doExecuteQueryMessage(query_msg, results, ctx);
    }

    core::Status doExecuteBytecode(const std::vector<uint8_t>& bytecode,
                                   const std::string& sql,
                                   ResultSet* results,
                                   core::ErrorContext* ctx) {
        auto query_msg = protocol::ProtocolCodec::buildQueryBytecode(session_id_, bytecode, sql, 0);
        return doExecuteQueryMessage(query_msg, results, ctx);
    }

    core::Status doBeginTransaction(core::ErrorContext* ctx) {
        auto msg = protocol::ProtocolCodec::buildBeginTransaction(session_id_, 0, false);
        auto status = protocol_session_->sendMessage(msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send BEGIN";
            return status;
        }

        protocol::Message response;
        status = protocol_session_->receiveMessage(response, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive response";
            return status;
        }

        if (response.getType() == protocol::MessageType::QUERY_ERROR) {
            uint32_t code;
            std::string sqlstate, message, detail, hint;
            protocol::ProtocolCodec::parseQueryError(
                response, code, sqlstate, message, detail, hint, ctx
            );
            last_error_ = message;
            return static_cast<core::Status>(code);
        }

        in_transaction_ = true;
        state_ = ConnectionState::IN_TRANSACTION;
        return core::Status::OK;
    }

    core::Status doCommit(core::ErrorContext* ctx) {
        auto msg = protocol::ProtocolCodec::buildCommit(session_id_);
        auto status = protocol_session_->sendMessage(msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send COMMIT";
            return status;
        }

        protocol::Message response;
        status = protocol_session_->receiveMessage(response, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive response";
            return status;
        }

        if (response.getType() == protocol::MessageType::QUERY_ERROR) {
            uint32_t code;
            std::string sqlstate, message, detail, hint;
            protocol::ProtocolCodec::parseQueryError(
                response, code, sqlstate, message, detail, hint, ctx
            );
            last_error_ = message;
            return static_cast<core::Status>(code);
        }

        in_transaction_ = false;
        state_ = ConnectionState::CONNECTED;
        return core::Status::OK;
    }

    core::Status doRollback(core::ErrorContext* ctx) {
        auto msg = protocol::ProtocolCodec::buildRollback(session_id_);
        auto status = protocol_session_->sendMessage(msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send ROLLBACK";
            return status;
        }

        protocol::Message response;
        status = protocol_session_->receiveMessage(response, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive response";
            return status;
        }

        if (response.getType() == protocol::MessageType::QUERY_ERROR) {
            uint32_t code;
            std::string sqlstate, message, detail, hint;
            protocol::ProtocolCodec::parseQueryError(
                response, code, sqlstate, message, detail, hint, ctx
            );
            last_error_ = message;
            return static_cast<core::Status>(code);
        }

        in_transaction_ = false;
        state_ = ConnectionState::CONNECTED;
        return core::Status::OK;
    }
};

// ============================================================================
// Connection Public API
// ============================================================================

Connection::Connection() : impl_(std::make_unique<ConnectionImpl>()) {}
Connection::~Connection() {
    if (impl_) {
        disconnect();
    }
}

Connection::Connection(Connection&& other) noexcept = default;
Connection& Connection::operator=(Connection&& other) noexcept = default;

core::Status Connection::connect(const std::string& database,
                                 const std::string& username,
                                 const std::string& password,
                                 core::ErrorContext* ctx) {
    ConnectionConfig config;
    config.database_name = database;
    config.username = username;
    config.password = password;
    return connect(config, ctx);
}

core::Status Connection::connect(const ConnectionConfig& config,
                                 core::ErrorContext* ctx) {
    if (isConnected()) {
        disconnect();
    }

    impl_->config_ = config;
    return impl_->doConnect(ctx);
}

void Connection::disconnect() {
    if (!isConnected()) return;

    // Rollback any active transaction
    if (impl_->in_transaction_) {
        rollback();
    }

    // Send disconnect message
    if (impl_->protocol_session_) {
        auto msg = protocol::ProtocolCodec::buildDisconnect();
        impl_->protocol_session_->sendMessage(msg);
    }

    // Close IPC client
    if (impl_->ipc_client_) {
        impl_->ipc_client_->disconnect();
    }

    impl_->protocol_session_.reset();
    impl_->ipc_client_.reset();
    impl_->state_ = ConnectionState::DISCONNECTED;
    impl_->in_transaction_ = false;
}

bool Connection::isConnected() const {
    if (!impl_) return false;
    return impl_->state_ == ConnectionState::CONNECTED ||
           impl_->state_ == ConnectionState::IN_TRANSACTION;
}

ConnectionState Connection::getState() const {
    if (!impl_) return ConnectionState::DISCONNECTED;
    return impl_->state_;
}

std::string Connection::getLastError() const {
    return impl_->last_error_;
}

core::Status Connection::ping(core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    auto msg = protocol::ProtocolCodec::buildPing(timestamp, 0);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send ping";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive pong";
        return status;
    }

    if (response.getType() != protocol::MessageType::PONG) {
        impl_->last_error_ = "Unexpected response to ping";
        return core::Status::PROTOCOL_VIOLATION;
    }

    return core::Status::OK;
}

core::Status Connection::cancelQuery(core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto msg = protocol::ProtocolCodec::buildQueryCancel();
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send cancel request";
        return status;
    }

    return core::Status::OK;
}

core::Status Connection::requestStatus(protocol::StatusRequestType request_type,
                                       StatusResponse* out,
                                       core::ErrorContext* ctx) {
    if (!out) {
        return core::Status::INVALID_ARGUMENT;
    }
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto msg = protocol::ProtocolCodec::buildStatusRequest(request_type);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send status request";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive status response";
        return status;
    }

    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        uint32_t error_code;
        std::string sqlstate, message, detail, hint;
        protocol::ProtocolCodec::parseQueryError(
            response, error_code, sqlstate, message, detail, hint, ctx
        );
        impl_->last_error_ = message;
        return static_cast<core::Status>(error_code);
    }

    if (response.getType() != protocol::MessageType::STATUS_RESPONSE) {
        impl_->last_error_ = "Unexpected response to status request";
        return core::Status::PROTOCOL_VIOLATION;
    }

    out->entries.clear();
    protocol::StatusRequestType parsed_type = request_type;
    std::vector<protocol::ProtocolCodec::StatusEntry> entries;
    status = protocol::ProtocolCodec::parseStatusResponse(response, parsed_type, entries, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to parse status response";
        return status;
    }

    out->request_type = parsed_type;
    out->entries.reserve(entries.size());
    for (const auto& entry : entries) {
        StatusEntry out_entry;
        out_entry.key = entry.key;
        out_entry.value = entry.value;
        out->entries.push_back(std::move(out_entry));
    }

    return core::Status::OK;
}

core::Status Connection::subscribe(const std::string& channel,
                                   const std::string& filter,
                                   uint8_t subscribe_type,
                                   core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }
    auto msg = protocol::ProtocolCodec::buildSubscribe(subscribe_type, channel, filter);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send subscribe request";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive subscribe response";
        return status;
    }
    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        uint32_t error_code;
        std::string sqlstate, message, detail, hint;
        protocol::ProtocolCodec::parseQueryError(
            response, error_code, sqlstate, message, detail, hint, ctx
        );
        impl_->last_error_ = message;
        return static_cast<core::Status>(error_code);
    }
    return core::Status::OK;
}

core::Status Connection::unsubscribe(const std::string& channel,
                                     core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }
    auto msg = protocol::ProtocolCodec::buildUnsubscribe(channel);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send unsubscribe request";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive unsubscribe response";
        return status;
    }
    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        uint32_t error_code;
        std::string sqlstate, message, detail, hint;
        protocol::ProtocolCodec::parseQueryError(
            response, error_code, sqlstate, message, detail, hint, ctx
        );
        impl_->last_error_ = message;
        return static_cast<core::Status>(error_code);
    }
    return core::Status::OK;
}

core::Status Connection::receiveNotification(Notification* out,
                                             core::ErrorContext* ctx) {
    if (!out) {
        return core::Status::INVALID_ARGUMENT;
    }
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    protocol::Message response;
    auto status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive notification";
        return status;
    }

    if (response.getType() != protocol::MessageType::NOTIFICATION) {
        impl_->last_error_ = "No notification";
        return core::Status::NOT_FOUND;
    }

    protocol::ProtocolCodec::parseNotification(response, out->processId, out->channel,
                                               out->payload, out->changeType,
                                               out->rowId, ctx);
    return core::Status::OK;
}

void Connection::setProgressCallback(std::function<void(uint64_t, uint64_t)> callback) {
    if (!impl_) {
        return;
    }
    impl_->progress_callback_ = std::move(callback);
}

void Connection::setCopyInputStream(std::istream* in) {
    if (!impl_) {
        return;
    }
    impl_->copy_input_stream_ = in;
}

void Connection::setCopyOutputStream(std::ostream* out) {
    if (!impl_) {
        return;
    }
    impl_->copy_output_stream_ = out;
}

core::Status Connection::sendAuthRequest(protocol::AuthMethod method,
                                         const std::vector<uint8_t>& payload,
                                         AuthResponse& response,
                                         core::ErrorContext* ctx) {
    if (!impl_) {
        return core::Status::INVALID_ARGUMENT;
    }

    if (impl_->state_ != ConnectionState::CONNECTED &&
        impl_->state_ != ConnectionState::CONNECTING) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    return impl_->doSendAuthRequest(method, payload, response, ctx);
}

core::Status Connection::executeQuery(const std::string& sql,
                                       ResultSet* results,
                                       core::ErrorContext* ctx) {
    return executeQuery(sql, results, 0, ctx);
}

core::Status Connection::executeQuery(const std::string& sql,
                                       ResultSet* results,
                                       uint8_t flags,
                                       core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto status = impl_->doExecuteQuery(sql, flags, results, ctx);
    if (!isOk(status) && ctx && ctx->message.empty() && !impl_->last_error_.empty())
    {
        ctx->set(status, impl_->last_error_.c_str(), __FILE__, __LINE__, __func__);
    }
    return status;
}

core::Status Connection::executeBytecode(const std::vector<uint8_t>& bytecode,
                                         const std::string& sql,
                                         ResultSet* results,
                                         core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    return impl_->doExecuteBytecode(bytecode, sql, results, ctx);
}

core::Status Connection::executeBytecode(const std::vector<uint8_t>& bytecode,
                                         ResultSet* results,
                                         core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    return impl_->doExecuteBytecode(bytecode, std::string(), results, ctx);
}

core::Status Connection::execute(const std::string& sql,
                                  int64_t* rows_affected,
                                  core::ErrorContext* ctx) {
    ResultSet results;
    auto status = executeQuery(sql, &results, ctx);
    if (isOk(status) && rows_affected) {
        *rows_affected = results.getRowsAffected();
    }
    return status;
}

core::Status Connection::prepare(const std::string& sql,
                                  PreparedStatement* stmt,
                                  core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    size_t param_count = countParameters(sql);

    stmt->impl_->sql_ = sql;
    stmt->impl_->param_count_ = param_count;
    stmt->impl_->params_.resize(param_count);
    stmt->impl_->param_types_.assign(param_count, protocol::WireType::UNKNOWN);
    stmt->impl_->valid_ = true;

    // Note: In a full implementation, we would send PREPARE to server
    // For now, we prepare client-side and substitute parameters

    return core::Status::OK;
}

core::Status Connection::executeQuery(PreparedStatement& stmt,
                                       ResultSet* results,
                                       core::ErrorContext* ctx) {
    if (!stmt.isValid()) {
        impl_->last_error_ = "Invalid prepared statement";
        return core::Status::INVALID_ARGUMENT;
    }

    // NET-1: Parameter substitution with proper escaping for SQL injection prevention
    // Substitute $1, $2, etc. with escaped parameter values
    std::string sql = substituteParameters(stmt.impl_->sql_, stmt.impl_->params_,
                                           stmt.impl_->param_types_);

    return executeQuery(sql, results, ctx);
}

core::Status Connection::execute(PreparedStatement& stmt,
                                  int64_t* rows_affected,
                                  core::ErrorContext* ctx) {
    ResultSet results;
    auto status = executeQuery(stmt, &results, ctx);
    if (isOk(status) && rows_affected) {
        *rows_affected = results.getRowsAffected();
    }
    return status;
}

void Connection::closeStatement(PreparedStatement& stmt) {
    stmt.impl_->valid_ = false;
    stmt.impl_->params_.clear();
}

core::Status Connection::beginTransaction(core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    if (impl_->in_transaction_) {
        impl_->last_error_ = "Already in transaction";
        return core::Status::INVALID_TRANSACTION_STATE;
    }

    return impl_->doBeginTransaction(ctx);
}

core::Status Connection::commit(core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    if (!impl_->in_transaction_) {
        impl_->last_error_ = "Not in transaction";
        return core::Status::INVALID_TRANSACTION_STATE;
    }

    return impl_->doCommit(ctx);
}

core::Status Connection::rollback(core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    if (!impl_->in_transaction_) {
        // Silently succeed if not in transaction
        return core::Status::OK;
    }

    return impl_->doRollback(ctx);
}

core::Status Connection::savepoint(const std::string& name,
                                    core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto msg = protocol::ProtocolCodec::buildSavepoint(impl_->session_id_, name);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send SAVEPOINT";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive response";
        return status;
    }

    return core::Status::OK;
}

core::Status Connection::releaseSavepoint(const std::string& name,
                                           core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto msg = protocol::ProtocolCodec::buildReleaseSavepoint(impl_->session_id_, name);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send RELEASE SAVEPOINT";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive response";
        return status;
    }

    // NET-M2: Proper response validation
    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        uint32_t code;
        std::string sqlstate, message, detail, hint;
        protocol::ProtocolCodec::parseQueryError(
            response, code, sqlstate, message, detail, hint, ctx
        );
        impl_->last_error_ = message;
        return static_cast<core::Status>(code);
    }

    return core::Status::OK;
}

core::Status Connection::rollbackTo(const std::string& name,
                                     core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto msg = protocol::ProtocolCodec::buildRollbackTo(impl_->session_id_, name);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send ROLLBACK TO";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive response";
        return status;
    }

    // NET-M3: Proper response validation
    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        uint32_t code;
        std::string sqlstate, message, detail, hint;
        protocol::ProtocolCodec::parseQueryError(
            response, code, sqlstate, message, detail, hint, ctx
        );
        impl_->last_error_ = message;
        return static_cast<core::Status>(code);
    }

    return core::Status::OK;
}

bool Connection::inTransaction() const {
    return impl_->in_transaction_;
}

void Connection::setAutoCommit(bool enabled) {
    impl_->auto_commit_ = enabled;
}

bool Connection::getAutoCommit() const {
    return impl_->auto_commit_;
}

std::string Connection::getDatabaseName() const {
    return impl_->config_.database_name;
}

std::string Connection::getUsername() const {
    return impl_->config_.username;
}

std::string Connection::getServerVersion() const {
    return impl_->server_version_;
}

const ConnectionConfig& Connection::getConfig() const {
    return impl_->config_;
}

// ============================================================================
// Static Server Control
// ============================================================================

core::Status Connection::startServer(const std::string& database_name,
                                       const std::string& server_path,
                                       uint32_t timeout_ms,
                                       core::ErrorContext* ctx) {
    if (isServerRunning(database_name)) {
        return core::Status::OK;  // Already running
    }

    // Find sb_server executable
    std::string exe_path = server_path;
    if (exe_path.empty()) {
        // Search common locations
        const char* path_env = std::getenv("PATH");
        if (path_env) {
            std::string path_str = path_env;
            std::stringstream ss(path_str);
            std::string dir;
            while (std::getline(ss, dir, ':')) {
                std::string candidate = dir + "/sb_server";
                std::ifstream f(candidate);
                if (f.good()) {
                    exe_path = candidate;
                    break;
                }
            }
        }

        // Also check relative to current directory
        if (exe_path.empty()) {
            std::ifstream f("./sb_server");
            if (f.good()) exe_path = "./sb_server";
        }
        if (exe_path.empty()) {
            std::ifstream f("./build/bin/sb_server");
            if (f.good()) exe_path = "./build/bin/sb_server";
        }
    }

    if (exe_path.empty()) {
        if (ctx) ctx->message = "sb_server not found in PATH";
        return core::Status::NOT_FOUND;
    }

#ifdef _WIN32
    // Windows implementation
    STARTUPINFO si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    std::string cmdline = exe_path + " --database=" + database_name + " --daemon";

    if (!CreateProcess(NULL, const_cast<char*>(cmdline.c_str()),
                       NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, NULL, &si, &pi)) {
        if (ctx) ctx->message = "Failed to start server process";
        return core::Status::INTERNAL_ERROR;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    // Unix implementation
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - redirect output to /dev/null
        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd >= 0) {
            dup2(null_fd, STDOUT_FILENO);
            dup2(null_fd, STDERR_FILENO);
            close(null_fd);
        }

        // Create new session (detach from parent)
        setsid();

        // Execute server
        execl(exe_path.c_str(), "sb_server",
              "--database", database_name.c_str(),
              "--daemon", nullptr);

        // execl failed
        _exit(1);
    } else if (pid < 0) {
        if (ctx) ctx->message = "fork() failed";
        return core::Status::INTERNAL_ERROR;
    }
#endif

    // Wait for server to start (poll PID file)
    // Note: If another client also tried to start a server for the same database,
    // our forked process will fail to acquire the database lock and exit.
    // We detect this by checking if a different server (different PID) started.
    auto start_time = std::chrono::steady_clock::now();
    while (true) {
        if (isServerRunning(database_name)) {
            // Server is running - could be ours or another client's
            // Either way, we can connect to it
            return core::Status::OK;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time
        ).count();

        if (static_cast<uint32_t>(elapsed) >= timeout_ms) {
            // Our server didn't start - check if our child process is still running
            // If not, it might have failed due to database lock conflict
            if (ctx) ctx->message = "Server failed to start within timeout (database may be locked by another process)";
            return core::Status::LOCK_TIMEOUT;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

core::Status Connection::stopServer(const std::string& database_name,
                                     core::ErrorContext* ctx) {
    int32_t pid = getServerPID(database_name);
    if (pid <= 0) {
        return core::Status::OK;  // Not running
    }

#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess) {
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
    }
#else
    kill(pid, SIGTERM);
#endif

    // Wait for server to stop
    for (int i = 0; i < 50; ++i) {
        if (!isServerRunning(database_name)) {
            return core::Status::OK;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Force kill
#ifndef _WIN32
    kill(pid, SIGKILL);
#endif

    return core::Status::OK;
}

bool Connection::isServerRunning(const std::string& database_name) {
    return server::isServerRunning(database_name);
}

int32_t Connection::getServerPID(const std::string& database_name) {
    std::string pid_path = server::getPIDFilePath(database_name);
    std::ifstream f(pid_path);
    if (!f.is_open()) return 0;

    int32_t pid = 0;
    f >> pid;
    return pid;
}

// ============================================================================
// PooledConnection Implementation
// ============================================================================

PooledConnection::PooledConnection(Connection* conn, std::function<void(Connection*)> returner)
    : connection_(conn), returner_(std::move(returner)) {}

PooledConnection::~PooledConnection() {
    if (connection_ && returner_) {
        returner_(connection_);
    }
}

PooledConnection::PooledConnection(PooledConnection&& other) noexcept
    : connection_(other.connection_), returner_(std::move(other.returner_)) {
    other.connection_ = nullptr;
}

PooledConnection& PooledConnection::operator=(PooledConnection&& other) noexcept {
    if (this != &other) {
        if (connection_ && returner_) {
            returner_(connection_);
        }
        connection_ = other.connection_;
        returner_ = std::move(other.returner_);
        other.connection_ = nullptr;
    }
    return *this;
}

// ============================================================================
// ConnectionPool Implementation
// ============================================================================

class ConnectionPool::PoolImpl {
public:
    PoolConfig config_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::unique_ptr<Connection>> available_;
    std::atomic<uint32_t> in_use_{0};
    std::atomic<uint32_t> total_{0};
    std::atomic<uint64_t> acquisitions_{0};
    std::atomic<uint64_t> releases_{0};
    std::atomic<uint64_t> waits_{0};
    std::atomic<bool> shutdown_{false};

    void returnConnection(Connection* conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        --in_use_;
        ++releases_;

        if (!shutdown_ && conn->isConnected()) {
            available_.push(std::unique_ptr<Connection>(conn));
            cv_.notify_one();
        } else {
            delete conn;
            --total_;
        }
    }
};

ConnectionPool::ConnectionPool(const PoolConfig& config)
    : impl_(std::make_unique<PoolImpl>()) {
    impl_->config_ = config;

    // Pre-create minimum connections
    for (uint32_t i = 0; i < config.min_connections; ++i) {
        auto conn = std::make_unique<Connection>();
        if (isOk(conn->connect(config.connection_config))) {
            impl_->available_.push(std::move(conn));
            ++impl_->total_;
        }
    }
}

ConnectionPool::~ConnectionPool() {
    shutdown();
}

PooledConnection ConnectionPool::acquire(core::ErrorContext* ctx) {
    std::unique_lock<std::mutex> lock(impl_->mutex_);
    ++impl_->acquisitions_;

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(impl_->config_.acquire_timeout_ms);

    while (impl_->available_.empty()) {
        // Can we create a new connection?
        if (impl_->total_ < impl_->config_.max_connections) {
            ++impl_->total_;
            lock.unlock();

            auto conn = std::make_unique<Connection>();
            if (isOk(conn->connect(impl_->config_.connection_config, ctx))) {
                ++impl_->in_use_;
                return PooledConnection(conn.release(),
                    [this](Connection* c) { impl_->returnConnection(c); });
            }

            lock.lock();
            --impl_->total_;
        }

        // Wait for a connection to be returned
        ++impl_->waits_;
        if (impl_->cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            if (ctx) ctx->message = "Connection pool timeout";
            return PooledConnection(nullptr, nullptr);
        }
    }

    auto conn = std::move(impl_->available_.front());
    impl_->available_.pop();
    ++impl_->in_use_;

    // Validate connection if configured
    if (impl_->config_.validate_on_acquire) {
        lock.unlock();
        if (!isOk(conn->ping())) {
            // Connection is dead, try to reconnect
            conn->disconnect();
            if (!isOk(conn->connect(impl_->config_.connection_config, ctx))) {
                lock.lock();
                --impl_->in_use_;
                --impl_->total_;
                return acquire(ctx);  // Try again
            }
        }
    } else {
        lock.unlock();
    }

    return PooledConnection(conn.release(),
        [this](Connection* c) { impl_->returnConnection(c); });
}

ConnectionPool::Stats ConnectionPool::getStats() const {
    Stats stats;
    stats.total_connections = impl_->total_;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        stats.available_connections = static_cast<uint32_t>(impl_->available_.size());
    }
    stats.in_use_connections = impl_->in_use_;
    stats.total_acquisitions = impl_->acquisitions_;
    stats.total_releases = impl_->releases_;
    stats.wait_count = impl_->waits_;
    return stats;
}

void ConnectionPool::shutdown() {
    impl_->shutdown_ = true;

    std::lock_guard<std::mutex> lock(impl_->mutex_);
    while (!impl_->available_.empty()) {
        impl_->available_.pop();
        --impl_->total_;
    }
    impl_->cv_.notify_all();
}

} // namespace client
} // namespace scratchbird
