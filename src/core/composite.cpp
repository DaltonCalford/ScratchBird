#include "scratchbird/core/composite.h"
#include <sstream>
#include <cstring>

namespace scratchbird::core
{
    // ===== CompositeRecord Implementation =====

    CompositeRecord::CompositeRecord(const std::string& type_name, const std::vector<CompositeField>& fields)
        : type_name_(type_name), fields_(fields) {

        // Build field index map
        for (size_t i = 0; i < fields_.size(); ++i) {
            field_indices_[fields_[i].name] = i;
        }

        // Initialize values with defaults
        values_.reserve(fields_.size());
        for (const auto& field : fields_) {
            switch (field.type) {
                case CompositeFieldType::INT32:
                    values_.emplace_back(int32_t(0));
                    break;
                case CompositeFieldType::INT64:
                    values_.emplace_back(int64_t(0));
                    break;
                case CompositeFieldType::FLOAT32:
                    values_.emplace_back(0.0f);
                    break;
                case CompositeFieldType::FLOAT64:
                    values_.emplace_back(0.0);
                    break;
                case CompositeFieldType::STRING:
                    values_.emplace_back(std::string(""));
                    break;
                case CompositeFieldType::BOOL:
                    values_.emplace_back(false);
                    break;
                case CompositeFieldType::COMPOSITE:
                    values_.emplace_back(std::shared_ptr<CompositeRecord>(nullptr));
                    break;
            }
        }
    }

    auto CompositeRecord::getField(const std::string& field_name) const -> std::optional<CompositeFieldValue> {
        auto it = field_indices_.find(field_name);
        if (it == field_indices_.end()) {
            return std::nullopt;
        }
        return values_[it->second];
    }

    auto CompositeRecord::setField(const std::string& field_name, const CompositeFieldValue& value) -> bool {
        auto it = field_indices_.find(field_name);
        if (it == field_indices_.end()) {
            return false;
        }

        size_t index = it->second;
        CompositeFieldType expected_type = fields_[index].type;

        // Type checking
        bool type_matches = false;
        switch (expected_type) {
            case CompositeFieldType::INT32:
                type_matches = std::holds_alternative<int32_t>(value);
                break;
            case CompositeFieldType::INT64:
                type_matches = std::holds_alternative<int64_t>(value);
                break;
            case CompositeFieldType::FLOAT32:
                type_matches = std::holds_alternative<float>(value);
                break;
            case CompositeFieldType::FLOAT64:
                type_matches = std::holds_alternative<double>(value);
                break;
            case CompositeFieldType::STRING:
                type_matches = std::holds_alternative<std::string>(value);
                break;
            case CompositeFieldType::BOOL:
                type_matches = std::holds_alternative<bool>(value);
                break;
            case CompositeFieldType::COMPOSITE:
                type_matches = std::holds_alternative<std::shared_ptr<CompositeRecord>>(value);
                break;
        }

        if (!type_matches) {
            return false;
        }

        values_[index] = value;
        return true;
    }

    auto CompositeRecord::getFieldByIndex(size_t index) const -> std::optional<CompositeFieldValue> {
        if (index >= values_.size()) {
            return std::nullopt;
        }
        return values_[index];
    }

    auto CompositeRecord::setFieldByIndex(size_t index, const CompositeFieldValue& value) -> bool {
        if (index >= values_.size()) {
            return false;
        }
        values_[index] = value;
        return true;
    }

    bool CompositeRecord::hasField(const std::string& field_name) const {
        return field_indices_.find(field_name) != field_indices_.end();
    }

    auto CompositeRecord::getFieldIndex(const std::string& field_name) const -> std::optional<size_t> {
        auto it = field_indices_.find(field_name);
        if (it == field_indices_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    std::string CompositeRecord::toString() const {
        std::ostringstream oss;
        oss << type_name_ << "{";

        for (size_t i = 0; i < fields_.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << fields_[i].name << ": ";

            std::visit([&oss](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    oss << "\"" << arg << "\"";
                } else if constexpr (std::is_same_v<T, bool>) {
                    oss << (arg ? "true" : "false");
                } else if constexpr (std::is_same_v<T, std::shared_ptr<CompositeRecord>>) {
                    if (arg) {
                        oss << arg->toString();
                    } else {
                        oss << "null";
                    }
                } else {
                    oss << arg;
                }
            }, values_[i]);
        }

        oss << "}";
        return oss.str();
    }

    // ===== Composite Implementation =====

    auto Composite::create(const std::string& type_name, const std::vector<CompositeField>& fields)
        -> CompositeRecord {
        return CompositeRecord(type_name, fields);
    }

    auto Composite::encode(const CompositeRecord& value) -> std::vector<uint8_t> {
        std::vector<uint8_t> buffer;

        // Type name (length + string)
        const std::string& type_name = value.getTypeName();
        uint32_t name_len = static_cast<uint32_t>(type_name.length());
        buffer.push_back(static_cast<uint8_t>(name_len & 0xFF));
        buffer.push_back(static_cast<uint8_t>((name_len >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((name_len >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((name_len >> 24) & 0xFF));
        for (char c : type_name) {
            buffer.push_back(static_cast<uint8_t>(c));
        }

        // Field count
        uint32_t field_count = static_cast<uint32_t>(value.getFieldCount());
        buffer.push_back(static_cast<uint8_t>(field_count & 0xFF));
        buffer.push_back(static_cast<uint8_t>((field_count >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((field_count >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((field_count >> 24) & 0xFF));

        // Field definitions
        for (const auto& field : value.getFields()) {
            // Field name
            uint32_t fname_len = static_cast<uint32_t>(field.name.length());
            buffer.push_back(static_cast<uint8_t>(fname_len & 0xFF));
            buffer.push_back(static_cast<uint8_t>((fname_len >> 8) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((fname_len >> 16) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((fname_len >> 24) & 0xFF));
            for (char c : field.name) {
                buffer.push_back(static_cast<uint8_t>(c));
            }

            // Field type
            buffer.push_back(static_cast<uint8_t>(field.type));
        }

        // Field values
        for (size_t i = 0; i < value.getFieldCount(); ++i) {
            auto field_value = value.getFieldByIndex(i);
            if (field_value) {
                encodeFieldValue(buffer, *field_value, value.getFields()[i].type);
            }
        }

        return buffer;
    }

    auto Composite::decode(const std::vector<uint8_t>& binary) -> std::optional<CompositeRecord> {
        if (binary.size() < 8) return std::nullopt;

        size_t pos = 0;

        // Type name
        uint32_t name_len = static_cast<uint32_t>(binary[pos]) |
                           (static_cast<uint32_t>(binary[pos + 1]) << 8) |
                           (static_cast<uint32_t>(binary[pos + 2]) << 16) |
                           (static_cast<uint32_t>(binary[pos + 3]) << 24);
        pos += 4;

        if (binary.size() < pos + name_len) return std::nullopt;
        std::string type_name(reinterpret_cast<const char*>(&binary[pos]), name_len);
        pos += name_len;

        // Field count
        if (binary.size() < pos + 4) return std::nullopt;
        uint32_t field_count = static_cast<uint32_t>(binary[pos]) |
                              (static_cast<uint32_t>(binary[pos + 1]) << 8) |
                              (static_cast<uint32_t>(binary[pos + 2]) << 16) |
                              (static_cast<uint32_t>(binary[pos + 3]) << 24);
        pos += 4;

        // Field definitions
        std::vector<CompositeField> fields;
        for (uint32_t i = 0; i < field_count; ++i) {
            // Field name
            if (binary.size() < pos + 4) return std::nullopt;
            uint32_t fname_len = static_cast<uint32_t>(binary[pos]) |
                                (static_cast<uint32_t>(binary[pos + 1]) << 8) |
                                (static_cast<uint32_t>(binary[pos + 2]) << 16) |
                                (static_cast<uint32_t>(binary[pos + 3]) << 24);
            pos += 4;

            if (binary.size() < pos + fname_len + 1) return std::nullopt;
            std::string field_name(reinterpret_cast<const char*>(&binary[pos]), fname_len);
            pos += fname_len;

            // Field type
            CompositeFieldType field_type = static_cast<CompositeFieldType>(binary[pos++]);

            fields.emplace_back(field_name, field_type);
        }

        // Create composite
        CompositeRecord result(type_name, fields);

        // Decode field values
        for (size_t i = 0; i < field_count; ++i) {
            auto value = decodeFieldValue(binary, pos, fields[i].type);
            if (!value) return std::nullopt;
            result.setFieldByIndex(i, *value);
        }

        return result;
    }

    auto Composite::fromMap(const std::string& type_name,
                           const std::vector<CompositeField>& fields,
                           const std::unordered_map<std::string, CompositeFieldValue>& values)
        -> std::optional<CompositeRecord> {

        CompositeRecord result(type_name, fields);

        for (const auto& [name, value] : values) {
            if (!result.setField(name, value)) {
                return std::nullopt;  // Field doesn't exist or type mismatch
            }
        }

        return result;
    }

    bool Composite::equals(const CompositeRecord& a, const CompositeRecord& b) {
        if (a.getTypeName() != b.getTypeName()) return false;
        if (a.getFieldCount() != b.getFieldCount()) return false;

        for (size_t i = 0; i < a.getFieldCount(); ++i) {
            if (a.getFields()[i].name != b.getFields()[i].name) return false;
            if (a.getFields()[i].type != b.getFields()[i].type) return false;

            auto val_a = a.getFieldByIndex(i);
            auto val_b = b.getFieldByIndex(i);

            if (!val_a || !val_b) return false;

            // Compare values (simplified - doesn't handle nested composites deeply)
            if (val_a->index() != val_b->index()) return false;
        }

        return true;
    }

    void Composite::encodeFieldValue(std::vector<uint8_t>& buffer, const CompositeFieldValue& value,
                                     CompositeFieldType type) {
        switch (type) {
            case CompositeFieldType::INT32: {
                int32_t val = std::get<int32_t>(value);
                uint32_t bits = static_cast<uint32_t>(val);
                buffer.push_back(static_cast<uint8_t>(bits & 0xFF));
                buffer.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
                buffer.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
                buffer.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
                break;
            }
            case CompositeFieldType::INT64: {
                int64_t val = std::get<int64_t>(value);
                uint64_t bits = static_cast<uint64_t>(val);
                for (int i = 0; i < 8; ++i) {
                    buffer.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
                }
                break;
            }
            case CompositeFieldType::FLOAT32: {
                float val = std::get<float>(value);
                uint32_t bits;
                std::memcpy(&bits, &val, sizeof(float));
                buffer.push_back(static_cast<uint8_t>(bits & 0xFF));
                buffer.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
                buffer.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
                buffer.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
                break;
            }
            case CompositeFieldType::FLOAT64: {
                double val = std::get<double>(value);
                uint64_t bits;
                std::memcpy(&bits, &val, sizeof(double));
                for (int i = 0; i < 8; ++i) {
                    buffer.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
                }
                break;
            }
            case CompositeFieldType::STRING: {
                const std::string& val = std::get<std::string>(value);
                uint32_t len = static_cast<uint32_t>(val.length());
                buffer.push_back(static_cast<uint8_t>(len & 0xFF));
                buffer.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
                buffer.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
                buffer.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
                for (char c : val) {
                    buffer.push_back(static_cast<uint8_t>(c));
                }
                break;
            }
            case CompositeFieldType::BOOL: {
                bool val = std::get<bool>(value);
                buffer.push_back(val ? 1 : 0);
                break;
            }
            case CompositeFieldType::COMPOSITE: {
                auto nested = std::get<std::shared_ptr<CompositeRecord>>(value);
                if (nested) {
                    auto nested_binary = encode(*nested);
                    buffer.insert(buffer.end(), nested_binary.begin(), nested_binary.end());
                }
                break;
            }
        }
    }

    auto Composite::decodeFieldValue(const std::vector<uint8_t>& binary, size_t& pos,
                                     CompositeFieldType type)
        -> std::optional<CompositeFieldValue> {

        switch (type) {
            case CompositeFieldType::INT32: {
                if (binary.size() < pos + 4) return std::nullopt;
                uint32_t bits = static_cast<uint32_t>(binary[pos]) |
                               (static_cast<uint32_t>(binary[pos + 1]) << 8) |
                               (static_cast<uint32_t>(binary[pos + 2]) << 16) |
                               (static_cast<uint32_t>(binary[pos + 3]) << 24);
                pos += 4;
                return static_cast<int32_t>(bits);
            }
            case CompositeFieldType::INT64: {
                if (binary.size() < pos + 8) return std::nullopt;
                uint64_t bits = 0;
                for (int i = 0; i < 8; ++i) {
                    bits |= static_cast<uint64_t>(binary[pos++]) << (i * 8);
                }
                return static_cast<int64_t>(bits);
            }
            case CompositeFieldType::FLOAT32: {
                if (binary.size() < pos + 4) return std::nullopt;
                uint32_t bits = static_cast<uint32_t>(binary[pos]) |
                               (static_cast<uint32_t>(binary[pos + 1]) << 8) |
                               (static_cast<uint32_t>(binary[pos + 2]) << 16) |
                               (static_cast<uint32_t>(binary[pos + 3]) << 24);
                pos += 4;
                float val;
                std::memcpy(&val, &bits, sizeof(float));
                return val;
            }
            case CompositeFieldType::FLOAT64: {
                if (binary.size() < pos + 8) return std::nullopt;
                uint64_t bits = 0;
                for (int i = 0; i < 8; ++i) {
                    bits |= static_cast<uint64_t>(binary[pos++]) << (i * 8);
                }
                double val;
                std::memcpy(&val, &bits, sizeof(double));
                return val;
            }
            case CompositeFieldType::STRING: {
                if (binary.size() < pos + 4) return std::nullopt;
                uint32_t len = static_cast<uint32_t>(binary[pos]) |
                              (static_cast<uint32_t>(binary[pos + 1]) << 8) |
                              (static_cast<uint32_t>(binary[pos + 2]) << 16) |
                              (static_cast<uint32_t>(binary[pos + 3]) << 24);
                pos += 4;
                if (binary.size() < pos + len) return std::nullopt;
                std::string val(reinterpret_cast<const char*>(&binary[pos]), len);
                pos += len;
                return val;
            }
            case CompositeFieldType::BOOL: {
                if (binary.size() < pos + 1) return std::nullopt;
                bool val = binary[pos++] != 0;
                return val;
            }
            case CompositeFieldType::COMPOSITE: {
                // Decode nested composite (simplified)
                return std::shared_ptr<CompositeRecord>(nullptr);
            }
        }

        return std::nullopt;
    }

} // namespace scratchbird::core
