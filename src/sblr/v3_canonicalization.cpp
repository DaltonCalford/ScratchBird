#include "scratchbird/sblr/v3_canonicalization.h"
#include "scratchbird/sblr/v3_codec.h"

#include <algorithm>
#include <cstring>

namespace scratchbird::sblr::v3 {

static bool valueToBytes(const ConstantPoolEntry& entry, std::vector<uint8_t>& out) {
    out.clear();
    out.push_back(entry.tag);
    std::string err;
    switch (entry.tag) {
        case 0x01: { // int64
            auto v = std::get_if<int64_t>(&entry.value.data);
            if (!v) return false;
            for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>((*v >> (i * 8)) & 0xFF));
            return true;
        }
        case 0x02: {
            auto v = std::get_if<uint64_t>(&entry.value.data);
            if (!v) return false;
            for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>((*v >> (i * 8)) & 0xFF));
            return true;
        }
        case 0x03: {
            auto v = std::get_if<double>(&entry.value.data);
            if (!v) return false;
            uint64_t raw;
            std::memcpy(&raw, v, sizeof(raw));
            for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>((raw >> (i * 8)) & 0xFF));
            return true;
        }
        case 0x04: {
            auto v = std::get_if<uint64_t>(&entry.value.data);
            if (!v) return false;
            encodeVaruint(*v, out);
            return true;
        }
        case 0x05: {
            auto v = std::get_if<Value::Bytes>(&entry.value.data);
            if (!v) return false;
            encodeVaruint(v->size(), out);
            out.insert(out.end(), v->begin(), v->end());
            return true;
        }
        case 0x06: {
            auto v = std::get_if<Value::Bytes>(&entry.value.data);
            if (!v || v->size() != 16) return false;
            out.insert(out.end(), v->begin(), v->end());
            return true;
        }
        case 0x07: {
            auto obj = std::get_if<Value::Object>(&entry.value.data);
            if (!obj) return false;
            auto scale_it = obj->find("scale");
            auto bytes_it = obj->find("bcd");
            if (scale_it == obj->end() || bytes_it == obj->end()) return false;
            int64_t scale = std::get<int64_t>(scale_it->second.data);
            for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((scale >> (i * 8)) & 0xFF));
            auto b = std::get<Value::Bytes>(bytes_it->second.data);
            encodeVaruint(b.size(), out);
            out.insert(out.end(), b.begin(), b.end());
            return true;
        }
        case 0x08: {
            auto v = std::get_if<bool>(&entry.value.data);
            if (!v) return false;
            out.push_back(*v ? 1 : 0);
            return true;
        }
        case 0x09: {
            auto v = std::get_if<uint64_t>(&entry.value.data);
            if (!v) return false;
            encodeVaruint(*v, out);
            return true;
        }
        default:
            return false;
    }
}

void canonicalizeSymbols(std::vector<std::string>& symbols, std::vector<size_t>* remap) {
    std::vector<std::pair<std::string, size_t>> items;
    items.reserve(symbols.size());
    for (size_t i = 0; i < symbols.size(); ++i) {
        items.emplace_back(symbols[i], i);
    }
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
        if (a.first == b.first) return a.second < b.second;
        return a.first < b.first;
    });

    std::vector<std::string> out;
    std::vector<size_t> map(symbols.size(), 0);
    for (const auto& item : items) {
        if (out.empty() || out.back() != item.first) {
            out.push_back(item.first);
        }
        map[item.second] = out.size() - 1;
    }
    symbols.swap(out);
    if (remap) *remap = std::move(map);
}

void canonicalizeConstants(std::vector<ConstantPoolEntry>& pool, std::vector<size_t>* remap) {
    struct EntryView {
        ConstantPoolEntry entry;
        std::vector<uint8_t> bytes;
        size_t old_index;
    };

    std::vector<EntryView> items;
    items.reserve(pool.size());
    for (size_t i = 0; i < pool.size(); ++i) {
        EntryView v{pool[i], {}, i};
        if (!valueToBytes(pool[i], v.bytes)) {
            v.bytes = {pool[i].tag};
        }
        items.push_back(std::move(v));
    }

    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
        if (a.entry.tag != b.entry.tag) return a.entry.tag < b.entry.tag;
        return a.bytes < b.bytes;
    });

    std::vector<ConstantPoolEntry> out;
    std::vector<size_t> map(pool.size(), 0);
    for (const auto& item : items) {
        if (out.empty() || item.bytes != items[out.size() - 1].bytes) {
            out.push_back(item.entry);
        }
        map[item.old_index] = out.size() - 1;
    }

    pool.swap(out);
    if (remap) *remap = std::move(map);
}

static void sortListByString(Value::List& list) {
    std::sort(list.begin(), list.end(), [](const Value& a, const Value& b) {
        const auto* sa = std::get_if<std::string>(&a.data);
        const auto* sb = std::get_if<std::string>(&b.data);
        if (!sa || !sb) return false;
        return *sa < *sb;
    });
}

static void sortOptionKvList(Value::List& list) {
    std::sort(list.begin(), list.end(), [](const Value& a, const Value& b) {
        const auto* oa = std::get_if<Value::Object>(&a.data);
        const auto* ob = std::get_if<Value::Object>(&b.data);
        if (!oa || !ob) return false;
        auto ita = oa->find("key");
        auto itb = ob->find("key");
        if (ita == oa->end() || itb == ob->end()) return false;
        const auto* sa = std::get_if<std::string>(&ita->second.data);
        const auto* sb = std::get_if<std::string>(&itb->second.data);
        if (!sa || !sb) return false;
        return *sa < *sb;
    });
}

void canonicalizePayload(const SchemaDef& schema, Value& payload) {
    if (!std::holds_alternative<Value::Object>(payload.data)) return;
    auto& obj = std::get<Value::Object>(payload.data);
    for (const auto& field : schema.fields) {
        auto it = obj.find(field.name);
        if (it == obj.end()) continue;
        if (field.name == "options" && field.ref == "OPTION_KV") {
            if (auto list = std::get_if<Value::List>(&it->second.data)) {
                sortOptionKvList(*list);
            }
        }
        if (field.name == "privileges") {
            if (auto list = std::get_if<Value::List>(&it->second.data)) {
                sortListByString(*list);
            }
        }
        if (field.name == "columns" && field.ref == "ident") {
            if (auto list = std::get_if<Value::List>(&it->second.data)) {
                sortListByString(*list);
            }
        }
    }
}

}  // namespace scratchbird::sblr::v3
