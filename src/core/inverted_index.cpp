/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/inverted_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/tsquery.h"
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cctype>
#include <ctime>
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <algorithm>

namespace scratchbird::core
{

namespace {
    constexpr uint64_t kMaxSegmentPostingBytes = 64ULL * 1024 * 1024;
    constexpr uint32_t kMergeFactor = 10;

    struct TokenOccurrence
    {
        std::string term;
        uint32_t position = 0;
        uint32_t start_offset = 0;
        uint32_t end_offset = 0;
    };

    uint16_t readUint16LE(const uint8_t* data)
    {
        return static_cast<uint16_t>(data[0]) |
            (static_cast<uint16_t>(data[1]) << 8);
    }

    void writeUint16LE(uint8_t* data, uint16_t value)
    {
        data[0] = static_cast<uint8_t>(value & 0xFF);
        data[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    }

    struct SnowballLib
    {
        using sb_symbol = unsigned char;
        struct sb_stemmer;
        using sb_stemmer_new_t = sb_stemmer* (*)(const char*, const char*);
        using sb_stemmer_delete_t = void (*)(sb_stemmer*);
        using sb_stemmer_stem_t = const sb_symbol* (*)(sb_stemmer*, const sb_symbol*, int);
        using sb_stemmer_length_t = int (*)(sb_stemmer*);

        void* handle = nullptr;
        sb_stemmer_new_t stemmer_new = nullptr;
        sb_stemmer_delete_t stemmer_delete = nullptr;
        sb_stemmer_stem_t stemmer_stem = nullptr;
        sb_stemmer_length_t stemmer_length = nullptr;
        bool available = false;
    };

    SnowballLib& snowballLib()
    {
        static SnowballLib lib;
        static std::once_flag once;
        std::call_once(once, []() {
#ifdef _WIN32
            (void)lib;
            return;
#else
            const char* candidates[] = {"libstemmer.so", "libstemmer.so.0"};
            for (const char* name : candidates)
            {
                lib.handle = dlopen(name, RTLD_LAZY);
                if (lib.handle)
                {
                    break;
                }
            }
            if (!lib.handle)
            {
                return;
            }
            lib.stemmer_new = reinterpret_cast<SnowballLib::sb_stemmer_new_t>(
                dlsym(lib.handle, "sb_stemmer_new"));
            lib.stemmer_delete = reinterpret_cast<SnowballLib::sb_stemmer_delete_t>(
                dlsym(lib.handle, "sb_stemmer_delete"));
            lib.stemmer_stem = reinterpret_cast<SnowballLib::sb_stemmer_stem_t>(
                dlsym(lib.handle, "sb_stemmer_stem"));
            lib.stemmer_length = reinterpret_cast<SnowballLib::sb_stemmer_length_t>(
                dlsym(lib.handle, "sb_stemmer_length"));
            lib.available = lib.stemmer_new && lib.stemmer_delete && lib.stemmer_stem && lib.stemmer_length;
            if (!lib.available)
            {
                dlclose(lib.handle);
                lib.handle = nullptr;
            }
#endif
        });
        return lib;
    }

    std::string snowballStem(std::string_view term, const char* language)
    {
        SnowballLib& lib = snowballLib();
        if (!lib.available)
        {
            return {};
        }
        thread_local SnowballLib::sb_stemmer* stemmer = nullptr;
        thread_local std::string stemmer_lang;
        if (!stemmer || stemmer_lang != language)
        {
            if (stemmer)
            {
                lib.stemmer_delete(stemmer);
                stemmer = nullptr;
            }
            stemmer = lib.stemmer_new(language, "UTF_8");
            stemmer_lang = language;
        }
        if (!stemmer)
        {
            return {};
        }
        const auto* result = lib.stemmer_stem(
            stemmer,
            reinterpret_cast<const SnowballLib::sb_symbol*>(term.data()),
            static_cast<int>(term.size()));
        int len = lib.stemmer_length(stemmer);
        if (!result || len <= 0)
        {
            return std::string(term);
        }
        return std::string(reinterpret_cast<const char*>(result), static_cast<size_t>(len));
    }

    std::string stemTerm(std::string_view term, uint16_t language)
    {
        const char* lang = "english";
        if (language != 0)
        {
            lang = "english";
        }
        std::string stemmed = snowballStem(term, lang);
        if (!stemmed.empty())
        {
            return stemmed;
        }
        return std::string(term);
    }

    bool filterTokenLength(std::string& token, uint16_t min_len, uint16_t max_len)
    {
        if (min_len > 0 && token.size() < min_len)
        {
            return false;
        }
        if (max_len > 0 && token.size() > max_len)
        {
            return false;
        }
        return true;
    }

    std::vector<std::string> tokenizeAscii(std::string_view text,
                                           bool filter_stop_words,
                                           uint16_t min_len,
                                           uint16_t max_len,
                                           bool use_stemming,
                                           uint16_t language)
    {
        static const char* stop_words[] = {
            "the", "and", "or", "a", "an", "of", "to", "in", "for",
            "on", "with", "by", "is", "it"
        };
        std::vector<std::string> tokens;
        std::string current;
        current.reserve(32);

        auto is_stop = [&](const std::string& word) -> bool {
            if (!filter_stop_words) {
                return false;
            }
            for (const char* stop : stop_words) {
                if (word == stop) {
                    return true;
                }
            }
            return false;
        };

        for (char ch : text)
        {
            unsigned char uc = static_cast<unsigned char>(ch);
            if (std::isalnum(uc))
            {
                current.push_back(static_cast<char>(std::tolower(uc)));
            }
            else if (!current.empty())
            {
                if (!is_stop(current))
                {
                    std::string token = current;
                    if (use_stemming)
                    {
                        token = stemTerm(token, language);
                    }
                    if (filterTokenLength(token, min_len, max_len))
                    {
                        tokens.push_back(std::move(token));
                    }
                }
                current.clear();
            }
        }

        if (!current.empty() && !is_stop(current))
        {
            std::string token = current;
            if (use_stemming)
            {
                token = stemTerm(token, language);
            }
            if (filterTokenLength(token, min_len, max_len))
            {
                tokens.push_back(std::move(token));
            }
        }

        return tokens;
    }

    std::vector<std::pair<std::string, uint32_t>> tokenizeAsciiWithPositions(std::string_view text,
                                                                             bool filter_stop_words)
    {
        static const char* stop_words[] = {
            "the", "and", "or", "a", "an", "of", "to", "in", "for",
            "on", "with", "by", "is", "it"
        };
        std::vector<std::pair<std::string, uint32_t>> tokens;
        std::string current;
        current.reserve(32);
        uint32_t position = 1;

        auto is_stop = [&](const std::string& word) -> bool {
            if (!filter_stop_words) {
                return false;
            }
            for (const char* stop : stop_words) {
                if (word == stop) {
                    return true;
                }
            }
            return false;
        };

        for (char ch : text)
        {
            unsigned char uc = static_cast<unsigned char>(ch);
            if (std::isalnum(uc))
            {
                current.push_back(static_cast<char>(std::tolower(uc)));
            }
            else if (!current.empty())
            {
                if (!is_stop(current))
                {
                    tokens.emplace_back(current, position);
                }
                position += 1;
                current.clear();
            }
        }

        if (!current.empty())
        {
            if (!is_stop(current))
            {
                tokens.emplace_back(current, position);
            }
        }

        return tokens;
    }

    std::vector<TokenOccurrence> tokenizeAsciiWithOffsets(std::string_view text,
                                                          bool filter_stop_words,
                                                          uint16_t min_len,
                                                          uint16_t max_len,
                                                          bool use_stemming,
                                                          uint16_t language)
    {
        static const char* stop_words[] = {
            "the", "and", "or", "a", "an", "of", "to", "in", "for",
            "on", "with", "by", "is", "it"
        };
        std::vector<TokenOccurrence> tokens;
        std::string current;
        current.reserve(32);
        uint32_t position = 1;
        uint32_t start_offset = 0;

        auto is_stop = [&](const std::string& word) -> bool {
            if (!filter_stop_words) {
                return false;
            }
            for (const char* stop : stop_words) {
                if (word == stop) {
                    return true;
                }
            }
            return false;
        };

        for (size_t i = 0; i < text.size(); ++i)
        {
            unsigned char uc = static_cast<unsigned char>(text[i]);
            if (std::isalnum(uc))
            {
                if (current.empty())
                {
                    start_offset = static_cast<uint32_t>(i);
                }
                current.push_back(static_cast<char>(std::tolower(uc)));
            }
            else if (!current.empty())
            {
                if (!is_stop(current))
                {
                    std::string token = current;
                    if (use_stemming)
                    {
                        token = stemTerm(token, language);
                    }
                    if (filterTokenLength(token, min_len, max_len))
                    {
                        TokenOccurrence occ;
                        occ.term = std::move(token);
                        occ.position = position;
                        occ.start_offset = start_offset;
                        occ.end_offset = static_cast<uint32_t>(i);
                        tokens.push_back(std::move(occ));
                    }
                }
                position += 1;
                current.clear();
            }
        }

        if (!current.empty())
        {
            if (!is_stop(current))
            {
                std::string token = current;
                if (use_stemming)
                {
                    token = stemTerm(token, language);
                }
                if (filterTokenLength(token, min_len, max_len))
                {
                    TokenOccurrence occ;
                    occ.term = std::move(token);
                    occ.position = position;
                    occ.start_offset = start_offset;
                    occ.end_offset = static_cast<uint32_t>(text.size());
                    tokens.push_back(std::move(occ));
                }
            }
        }

        return tokens;
    }

    void sortUnique(std::vector<TID>& tids)
    {
        std::sort(tids.begin(), tids.end());
        tids.erase(std::unique(tids.begin(), tids.end()), tids.end());
    }

    std::string normalizeTerm(std::string_view term, const InvertedIndexConfig& config)
    {
        std::string out(term);
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if ((config.features & II_FEATURE_STEMMING) != 0)
        {
            out = stemTerm(out, config.language);
        }
        if (!filterTokenLength(out, config.min_term_length, config.max_term_length))
        {
            return {};
        }
        return out;
    }

    std::vector<TID> unionTids(const std::vector<TID>& left, const std::vector<TID>& right)
    {
        std::vector<TID> result;
        result.reserve(left.size() + right.size());
        std::set_union(left.begin(), left.end(), right.begin(), right.end(),
                       std::back_inserter(result));
        return result;
    }

    std::vector<TID> intersectTids(const std::vector<TID>& left, const std::vector<TID>& right)
    {
        std::vector<TID> result;
        std::set_intersection(left.begin(), left.end(), right.begin(), right.end(),
                              std::back_inserter(result));
        return result;
    }

    std::vector<TID> subtractTids(const std::vector<TID>& universe, const std::vector<TID>& negate)
    {
        std::vector<TID> result;
        std::set_difference(universe.begin(), universe.end(),
                            negate.begin(), negate.end(),
                            std::back_inserter(result));
        return result;
    }

    void encodeVByte(uint64_t value, std::vector<uint8_t>& out)
    {
        while (value >= 0x80)
        {
            out.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
            value >>= 7;
        }
        out.push_back(static_cast<uint8_t>(value & 0x7F));
    }

    bool decodeVByte(const uint8_t* data, size_t length, size_t* offset, uint64_t* value_out)
    {
        if (!data || !offset || !value_out)
        {
            return false;
        }
        uint64_t value = 0;
        uint32_t shift = 0;
        while (*offset < length)
        {
            uint8_t byte = data[*offset];
            ++(*offset);
            value |= static_cast<uint64_t>(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0)
            {
                *value_out = value;
                return true;
            }
            shift += 7;
            if (shift >= 64)
            {
                return false;
            }
        }
        return false;
    }
}

InvertedIndex::InvertedIndex(Database* db,
                             const ID& index_uuid,
                             const ID& table_uuid,
                             const ID& column_uuid,
                             GPID meta_gpid,
                             InvertedIndexConfig config)
    : db_(db)
    , index_uuid_(index_uuid)
    , table_uuid_(table_uuid)
    , column_uuid_(column_uuid)
    , meta_gpid_(meta_gpid)
    , tablespace_id_(getTablespaceID(meta_gpid))
    , config_(config)
{
}

uint32_t InvertedIndex::hashTerm(std::string_view term) const
{
    uint32_t hash = 2166136261u;
    for (char ch : term)
    {
        uint8_t byte = static_cast<uint8_t>(ch);
        if (byte >= 'A' && byte <= 'Z')
        {
            byte = static_cast<uint8_t>(byte - 'A' + 'a');
        }
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

Status InvertedIndex::loadMeta(SBInvertedIndexMetaPage* meta_out, ErrorContext* ctx) const
{
    if (!meta_out || !db_)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid meta output");
        return Status::INVALID_ARGUMENT;
    }

    BufferPool* buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint8_t* meta_data = nullptr;
    Status status = buffer_pool->pinPageGlobal(meta_gpid_, reinterpret_cast<void**>(&meta_data), ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::memcpy(meta_out, meta_data, SB_INVERTED_META_PAGE_HEADER_SIZE);
    buffer_pool->unpinPageGlobal(meta_gpid_, false, ctx);
    return Status::OK;
}

Status InvertedIndex::loadSegmentMeta(uint32_t segment_id,
                                      GPID* seg_gpid_out,
                                      SBInvertedIndexSegmentMeta* seg_out,
                                      ErrorContext* ctx) const
{
    if (!seg_gpid_out || !seg_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid segment outputs");
        return Status::INVALID_ARGUMENT;
    }

    SBInvertedIndexMetaPage meta{};
    Status status = loadMeta(&meta, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (segment_id >= meta.ii_num_segments)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Segment id out of range");
        return Status::NOT_FOUND;
    }

    GPID seg_gpid = meta.ii_segment_pages[segment_id];
    if (seg_gpid == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Segment meta page missing");
        return Status::NOT_FOUND;
    }

    BufferPool* buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint8_t* seg_data = nullptr;
    status = buffer_pool->pinPageGlobal(seg_gpid, reinterpret_cast<void**>(&seg_data), ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::memcpy(seg_out, seg_data, SB_INVERTED_SEGMENT_META_HEADER_SIZE);
    buffer_pool->unpinPageGlobal(seg_gpid, false, ctx);

    *seg_gpid_out = seg_gpid;
    return Status::OK;
}

Status InvertedIndex::updateSegmentMeta(GPID seg_gpid,
                                        const SBInvertedIndexSegmentMeta& seg,
                                        ErrorContext* ctx) const
{
    BufferPool* buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint8_t* seg_data = nullptr;
    Status status = buffer_pool->pinPageGlobal(seg_gpid, reinterpret_cast<void**>(&seg_data), ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::memcpy(seg_data, &seg, SB_INVERTED_SEGMENT_META_HEADER_SIZE);
    buffer_pool->unpinPageGlobal(seg_gpid, true, ctx);
    return Status::OK;
}

Status InvertedIndex::updateMeta(const SBInvertedIndexMetaPage& meta, ErrorContext* ctx) const
{
    BufferPool* buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint8_t* meta_data = nullptr;
    Status status = buffer_pool->pinPageGlobal(meta_gpid_, reinterpret_cast<void**>(&meta_data), ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::memcpy(meta_data, &meta, SB_INVERTED_META_PAGE_HEADER_SIZE);
    buffer_pool->unpinPageGlobal(meta_gpid_, true, ctx);
    return Status::OK;
}

Status InvertedIndex::appendDocStats(uint32_t segment_id,
                                     const TID& tid,
                                     uint32_t doc_length,
                                     uint32_t unique_terms,
                                     ErrorContext* ctx)
{
    SBInvertedIndexSegmentMeta seg{};
    GPID seg_gpid = 0;
    Status status = loadSegmentMeta(segment_id, &seg_gpid, &seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    PageManager* page_mgr = db_->page_manager();
    BufferPool* buffer_pool = db_->buffer_pool();
    if (!page_mgr || !buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing page manager or buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t page_size = db_->page_size();
    uint32_t max_entries = maxDocStatsPerPage(page_size);
    if (max_entries == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid page size for docstats entries");
        return Status::INVALID_ARGUMENT;
    }

    GPID page_gpid = seg.seg_docstats_page;
    if (page_gpid == 0)
    {
        status = page_mgr->allocatePageInTablespace(tablespace_id_, &page_gpid, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        seg.seg_docstats_page = page_gpid;
        status = updateSegmentMeta(seg_gpid, seg, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        uint8_t* page_data = nullptr;
        status = buffer_pool->pinPageGlobal(page_gpid, reinterpret_cast<void**>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        std::memset(page_data, 0, page_size);
        auto* page = reinterpret_cast<SBDocumentStatsPage*>(page_data);
        page->docstats_header.magic = K_MAGIC_SBRD;
        page->docstats_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
        page->docstats_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_INVERTED_META);
        page->docstats_header.page_size = page_size;
        page->docstats_header.page_id = static_cast<uint32_t>(getPageNumber(page_gpid));
        page->docstats_header.generation = 1;
        page->docstats_header.checksum = 0;
        page->docstats_header.flags = 0;
        page->docstats_header.lsn = 0;
        pageSetLower(page->docstats_header, SB_DOCUMENT_STATS_PAGE_HEADER_SIZE);
        pageSetUpper(page->docstats_header, page_size);
        pageSetSpecial(page->docstats_header, page_size);
        page->docstats_next_page = 0;
        page->docstats_num_entries = 0;
        buffer_pool->unpinPageGlobal(page_gpid, true, ctx);
    }

    GPID current = page_gpid;
    GPID prev = 0;
    while (current != 0)
    {
        uint8_t* page_data = nullptr;
        status = buffer_pool->pinPageGlobal(current, reinterpret_cast<void**>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        auto* page = reinterpret_cast<SBDocumentStatsPage*>(page_data);
        if (page->docstats_num_entries < max_entries)
        {
            auto* entries = reinterpret_cast<InvertedDocStatsEntry*>(page->docstats_data);
            entries[page->docstats_num_entries] = InvertedDocStatsEntry{
                tid.gpid, tid.slot, 0, doc_length, unique_terms
            };
            page->docstats_num_entries += 1;
            buffer_pool->unpinPageGlobal(current, true, ctx);
            return Status::OK;
        }

        prev = current;
        current = page->docstats_next_page;
        buffer_pool->unpinPageGlobal(prev, false, ctx);
    }

    GPID new_page = 0;
    status = page_mgr->allocatePageInTablespace(tablespace_id_, &new_page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint8_t* prev_data = nullptr;
    status = buffer_pool->pinPageGlobal(prev, reinterpret_cast<void**>(&prev_data), ctx);
    if (status != Status::OK)
    {
        return status;
    }
    auto* prev_page = reinterpret_cast<SBDocumentStatsPage*>(prev_data);
    prev_page->docstats_next_page = new_page;
    buffer_pool->unpinPageGlobal(prev, true, ctx);

    uint8_t* new_data = nullptr;
    status = buffer_pool->pinPageGlobal(new_page, reinterpret_cast<void**>(&new_data), ctx);
    if (status != Status::OK)
    {
        return status;
    }
    std::memset(new_data, 0, page_size);
    auto* new_page_ptr = reinterpret_cast<SBDocumentStatsPage*>(new_data);
    new_page_ptr->docstats_header.magic = K_MAGIC_SBRD;
    new_page_ptr->docstats_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
    new_page_ptr->docstats_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_INVERTED_META);
    new_page_ptr->docstats_header.page_size = page_size;
    new_page_ptr->docstats_header.page_id = static_cast<uint32_t>(getPageNumber(new_page));
    new_page_ptr->docstats_header.generation = 1;
    new_page_ptr->docstats_header.checksum = 0;
    new_page_ptr->docstats_header.flags = 0;
    new_page_ptr->docstats_header.lsn = 0;
    pageSetLower(new_page_ptr->docstats_header, SB_DOCUMENT_STATS_PAGE_HEADER_SIZE);
    pageSetUpper(new_page_ptr->docstats_header, page_size);
    pageSetSpecial(new_page_ptr->docstats_header, page_size);
    new_page_ptr->docstats_next_page = 0;
    new_page_ptr->docstats_num_entries = 1;
    auto* entries = reinterpret_cast<InvertedDocStatsEntry*>(new_page_ptr->docstats_data);
    entries[0] = InvertedDocStatsEntry{tid.gpid, tid.slot, 0, doc_length, unique_terms};
    buffer_pool->unpinPageGlobal(new_page, true, ctx);

    return Status::OK;
}

Status InvertedIndex::updateDocStats(uint32_t segment_id,
                                     const TID& tid,
                                     uint32_t doc_length,
                                     uint32_t unique_terms,
                                     ErrorContext* ctx)
{
    SBInvertedIndexSegmentMeta seg{};
    GPID seg_gpid = 0;
    Status status = loadSegmentMeta(segment_id, &seg_gpid, &seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (seg.seg_docstats_page == 0)
    {
        return appendDocStats(segment_id, tid, doc_length, unique_terms, ctx);
    }

    BufferPool* buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    GPID current = seg.seg_docstats_page;
    while (current != 0)
    {
        uint8_t* page_data = nullptr;
        status = buffer_pool->pinPageGlobal(current, reinterpret_cast<void**>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        auto* page = reinterpret_cast<SBDocumentStatsPage*>(page_data);
        auto* entries = reinterpret_cast<InvertedDocStatsEntry*>(page->docstats_data);
        for (uint32_t i = 0; i < page->docstats_num_entries; ++i)
        {
            if (entries[i].gpid == tid.gpid && entries[i].slot == tid.slot)
            {
                entries[i].doc_length = doc_length;
                entries[i].num_unique_terms = unique_terms;
                buffer_pool->unpinPageGlobal(current, true, ctx);
                return Status::OK;
            }
        }
        GPID next = page->docstats_next_page;
        buffer_pool->unpinPageGlobal(current, false, ctx);
        current = next;
    }

    return appendDocStats(segment_id, tid, doc_length, unique_terms, ctx);
}

Status InvertedIndex::loadDocStatsMap(uint32_t segment_id, bool clear_existing, ErrorContext* ctx)
{
    SBInvertedIndexSegmentMeta seg{};
    GPID seg_gpid = 0;
    Status status = loadSegmentMeta(segment_id, &seg_gpid, &seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (seg.seg_docstats_page == 0)
    {
        return Status::OK;
    }

    BufferPool* buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    if (clear_existing)
    {
        doc_lengths_.clear();
    }
    GPID current = seg.seg_docstats_page;
    while (current != 0)
    {
        uint8_t* page_data = nullptr;
        status = buffer_pool->pinPageGlobal(current, reinterpret_cast<void**>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        auto* page = reinterpret_cast<SBDocumentStatsPage*>(page_data);
        auto* entries = reinterpret_cast<InvertedDocStatsEntry*>(page->docstats_data);
        for (uint32_t i = 0; i < page->docstats_num_entries; ++i)
        {
            if (entries[i].doc_length == 0)
            {
                continue;
            }
            TID tid{entries[i].gpid, entries[i].slot};
            doc_lengths_[tid] = entries[i].doc_length;
        }
        GPID next = page->docstats_next_page;
        buffer_pool->unpinPageGlobal(current, false, ctx);
        current = next;
    }
    return Status::OK;
}

Status InvertedIndex::loadAllTerms(uint32_t segment_id,
                                   std::vector<std::pair<std::string, TermDictionaryEntry>>* terms_out,
                                   ErrorContext* ctx) const
{
    if (!terms_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid terms output");
        return Status::INVALID_ARGUMENT;
    }
    terms_out->clear();

    SBInvertedIndexSegmentMeta seg{};
    GPID seg_gpid = 0;
    Status status = loadSegmentMeta(segment_id, &seg_gpid, &seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (seg.seg_dict_first_page == 0)
    {
        return Status::OK;
    }

    BufferPool* buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    GPID current = seg.seg_dict_first_page;
    while (current != 0)
    {
        uint8_t* page_data = nullptr;
        status = buffer_pool->pinPageGlobal(current, reinterpret_cast<void**>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        auto* page = reinterpret_cast<SBTermDictionaryPage*>(page_data);
        uint16_t count = page->dict_num_entries;
        auto* entries = reinterpret_cast<TermDictionaryEntry*>(page->dict_entries);
        for (uint16_t i = 0; i < count; ++i)
        {
            size_t len = 0;
            while (len < sizeof(entries[i].term) && entries[i].term[len] != '\0')
            {
                ++len;
            }
            std::string term(entries[i].term, len);
            terms_out->emplace_back(std::move(term), entries[i]);
        }
        GPID next = page->dict_next_page;
        buffer_pool->unpinPageGlobal(current, false, ctx);
        current = next;
    }
    return Status::OK;
}

Status InvertedIndex::createSegment(uint32_t* segment_id_out, GPID* seg_gpid_out, ErrorContext* ctx)
{
    if (!segment_id_out || !seg_gpid_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid segment outputs");
        return Status::INVALID_ARGUMENT;
    }

    SBInvertedIndexMetaPage meta{};
    Status status = loadMeta(&meta, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (meta.ii_num_segments >= 256)
    {
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "InvertedIndex segment limit reached");
        return Status::DATA_CORRUPTED;
    }

    uint32_t new_segment_id = meta.ii_num_segments;
    GPID seg_meta_gpid = 0;
    PageManager* page_mgr = db_->page_manager();
    BufferPool* buffer_pool = db_->buffer_pool();
    if (!page_mgr || !buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing page manager or buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    status = page_mgr->allocatePageInTablespace(tablespace_id_, &seg_meta_gpid, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint32_t page_size = db_->page_size();
    uint8_t* seg_data = nullptr;
    status = buffer_pool->pinPageGlobal(seg_meta_gpid, reinterpret_cast<void**>(&seg_data), ctx);
    if (status != Status::OK)
    {
        return status;
    }
    std::memset(seg_data, 0, page_size);
    auto* seg = reinterpret_cast<SBInvertedIndexSegmentMeta*>(seg_data);
    seg->seg_header.magic = K_MAGIC_SBRD;
    seg->seg_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
    seg->seg_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_INVERTED_DICT);
    seg->seg_header.page_size = page_size;
    seg->seg_header.page_id = static_cast<uint32_t>(getPageNumber(seg_meta_gpid));
    seg->seg_header.generation = 1;
    seg->seg_header.checksum = 0;
    seg->seg_header.flags = 0;
    seg->seg_header.lsn = 0;
    pageSetLower(seg->seg_header, SB_INVERTED_SEGMENT_META_HEADER_SIZE);
    pageSetUpper(seg->seg_header, page_size);
    pageSetSpecial(seg->seg_header, page_size);
    seg->seg_id = new_segment_id;
    seg->seg_flags = SEG_FLAG_ACTIVE;
    seg->seg_created_at = static_cast<uint64_t>(std::time(nullptr));
    seg->seg_merged_at = 0;
    buffer_pool->unpinPageGlobal(seg_meta_gpid, true, ctx);

    if (meta.ii_num_segments > 0)
    {
        uint32_t previous_active = meta.ii_active_segment;
        if (previous_active < meta.ii_num_segments)
        {
            SBInvertedIndexSegmentMeta prev_seg{};
            GPID prev_gpid = 0;
            Status prev_status = loadSegmentMeta(previous_active, &prev_gpid, &prev_seg, ctx);
            if (prev_status == Status::OK)
            {
                prev_seg.seg_flags &= ~SEG_FLAG_ACTIVE;
                updateSegmentMeta(prev_gpid, prev_seg, ctx);
            }
        }
    }

    meta.ii_segment_pages[new_segment_id] = seg_meta_gpid;
    meta.ii_num_segments += 1;
    meta.ii_active_segment = new_segment_id;
    status = updateMeta(meta, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    *segment_id_out = new_segment_id;
    *seg_gpid_out = seg_meta_gpid;
    return Status::OK;
}

Status InvertedIndex::maybeRotateSegment(SBInvertedIndexMetaPage* meta, ErrorContext* ctx)
{
    if (!meta)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid meta for segment rotation");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t segment_id = meta->ii_active_segment;
    SBInvertedIndexSegmentMeta seg{};
    GPID seg_gpid = 0;
    Status status = loadSegmentMeta(segment_id, &seg_gpid, &seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (seg.seg_total_posting_bytes < kMaxSegmentPostingBytes)
    {
        return Status::OK;
    }

    uint32_t new_segment_id = 0;
    GPID new_seg_gpid = 0;
    return createSegment(&new_segment_id, &new_seg_gpid, ctx);
}

Status InvertedIndex::maybeMergeSegments(SBInvertedIndexMetaPage* meta, ErrorContext* ctx)
{
    if (!meta)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid meta for merge evaluation");
        return Status::INVALID_ARGUMENT;
    }

    if (meta->ii_num_segments < kMergeFactor)
    {
        return Status::OK;
    }

    std::vector<uint32_t> candidates;
    for (uint32_t i = 0; i < meta->ii_num_segments; ++i)
    {
        SBInvertedIndexSegmentMeta seg{};
        GPID seg_gpid = 0;
        Status status = loadSegmentMeta(i, &seg_gpid, &seg, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        if ((seg.seg_flags & SEG_FLAG_MERGED) != 0)
        {
            continue;
        }
        candidates.push_back(i);
    }

    if (candidates.size() < kMergeFactor)
    {
        return Status::OK;
    }

    return mergeSegments(candidates, ctx);
}

Status InvertedIndex::mergeSegments(const std::vector<uint32_t>& segment_ids, ErrorContext* ctx)
{
    if (segment_ids.empty())
    {
        return Status::OK;
    }

    uint32_t merged_segment_id = 0;
    GPID merged_seg_gpid = 0;
    Status status = createSegment(&merged_segment_id, &merged_seg_gpid, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    bool store_offsets = (config_.features & II_FEATURE_OFFSETS) != 0;
    bool store_payloads = (config_.features & II_FEATURE_PAYLOADS) != 0;
    bool store_positions = (config_.features & II_FEATURE_POSITIONS) != 0 ||
        (config_.features & (II_FEATURE_OFFSETS | II_FEATURE_PAYLOADS)) != 0;
    struct PositionExtras
    {
        uint32_t position = 0;
        uint32_t start = 0;
        uint32_t end = 0;
        std::vector<uint8_t> payload;
    };
    std::map<std::string, std::vector<TID>> term_postings;
    std::map<std::string, std::map<TID, std::vector<PositionExtras>>> term_postings_positions;
    std::unordered_map<std::string, uint64_t> term_total_freq;
    std::map<TID, std::pair<uint32_t, uint32_t>> doc_stats;
    uint64_t total_doc_length = 0;

    for (uint32_t seg_id : segment_ids)
    {
        std::vector<std::pair<std::string, TermDictionaryEntry>> terms;
        status = loadAllTerms(seg_id, &terms, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        for (const auto& term_pair : terms)
        {
            const std::string& term = term_pair.first;
            const TermDictionaryEntry& entry = term_pair.second;
            if (store_positions)
            {
                std::vector<PostingWithPositions> postings;
                Status list_status = readPostingListWithPositions(seg_id, entry.posting_offset,
                                                                  entry.posting_length, &postings, ctx);
                if (list_status != Status::OK)
                {
                    return list_status;
                }
                for (const auto& posting : postings)
                {
                    auto& pos_list = term_postings_positions[term][posting.tid];
                    for (size_t i = 0; i < posting.positions.size(); ++i)
                    {
                        PositionExtras extra;
                        extra.position = posting.positions[i];
                        if (store_offsets && i < posting.offsets.size())
                        {
                            extra.start = posting.offsets[i].first;
                            extra.end = posting.offsets[i].second;
                        }
                        if (store_payloads && i < posting.payloads.size())
                        {
                            extra.payload = posting.payloads[i];
                        }
                        pos_list.push_back(std::move(extra));
                    }
                    term_total_freq[term] += posting.positions.size();
                }
            }
            else
            {
                std::vector<TID> tids;
                Status list_status = readPostingList(seg_id, entry.posting_offset, entry.posting_length, &tids, ctx);
                if (list_status != Status::OK)
                {
                    return list_status;
                }
                auto& merged = term_postings[term];
                merged.insert(merged.end(), tids.begin(), tids.end());
                term_total_freq[term] += entry.total_frequency;
            }
        }

        SBInvertedIndexSegmentMeta seg{};
        GPID seg_gpid = 0;
        status = loadSegmentMeta(seg_id, &seg_gpid, &seg, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (seg.seg_docstats_page == 0)
        {
            continue;
        }

        BufferPool* buffer_pool = db_->buffer_pool();
        if (!buffer_pool)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing buffer pool");
            return Status::INVALID_ARGUMENT;
        }

        GPID current = seg.seg_docstats_page;
        while (current != 0)
        {
            uint8_t* page_data = nullptr;
            status = buffer_pool->pinPageGlobal(current, reinterpret_cast<void**>(&page_data), ctx);
            if (status != Status::OK)
            {
                return status;
            }
            auto* page = reinterpret_cast<SBDocumentStatsPage*>(page_data);
            auto* entries = reinterpret_cast<InvertedDocStatsEntry*>(page->docstats_data);
            for (uint32_t i = 0; i < page->docstats_num_entries; ++i)
            {
                uint32_t doc_len = entries[i].doc_length;
                uint32_t unique_terms = entries[i].num_unique_terms;
                if (doc_len == 0)
                {
                    continue;
                }
                TID tid{entries[i].gpid, entries[i].slot};
                doc_stats[tid] = {doc_len, unique_terms};
            }
            GPID next = page->docstats_next_page;
            buffer_pool->unpinPageGlobal(current, false, ctx);
            current = next;
        }
    }

    for (const auto& [tid, stats] : doc_stats)
    {
        status = updateDocStats(merged_segment_id, tid, stats.first, stats.second, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        total_doc_length += stats.first;
    }

    if (store_positions)
    {
        for (auto& [term, postings_map] : term_postings_positions)
        {
            std::vector<PostingWithPositions> postings;
            postings.reserve(postings_map.size());
            uint64_t total_freq = 0;
            for (auto& [tid, positions] : postings_map)
            {
                std::sort(positions.begin(), positions.end(),
                          [](const PositionExtras& a, const PositionExtras& b) {
                              return a.position < b.position;
                          });
                positions.erase(std::unique(positions.begin(), positions.end(),
                                            [](const PositionExtras& a, const PositionExtras& b) {
                                                return a.position == b.position;
                                            }),
                               positions.end());
                PostingWithPositions posting;
                posting.tid = tid;
                posting.positions.reserve(positions.size());
                if (store_offsets)
                {
                    posting.offsets.reserve(positions.size());
                }
                if (store_payloads)
                {
                    posting.payloads.reserve(positions.size());
                }
                for (const auto& item : positions)
                {
                    posting.positions.push_back(item.position);
                    if (store_offsets)
                    {
                        posting.offsets.push_back({item.start, item.end});
                    }
                    if (store_payloads)
                    {
                        posting.payloads.push_back(item.payload);
                    }
                }
                total_freq += posting.positions.size();
                postings.push_back(std::move(posting));
            }
            TermDictionaryEntry entry{};
            size_t copy_len = term.size();
            if (copy_len >= sizeof(entry.term))
            {
                copy_len = sizeof(entry.term) - 1;
            }
            std::memcpy(entry.term, term.data(), copy_len);
            entry.term[copy_len] = '\0';
            entry.term_hash = hashTerm(term);
            entry.doc_frequency = static_cast<uint32_t>(postings.size());
            entry.total_frequency = total_freq;
            entry.reserved = config_.compression_type;

            uint64_t new_offset = 0;
            uint32_t new_length = 0;
            status = writePostingListWithPositions(merged_segment_id, postings, &new_offset, &new_length, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            entry.posting_offset = new_offset;
            entry.posting_length = new_length;

            GPID page_gpid = 0;
            uint16_t entry_index = 0;
            status = insertTerm(merged_segment_id, entry, &page_gpid, &entry_index, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }
    }
    else
    {
        for (auto& [term, tids] : term_postings)
        {
            sortUnique(tids);
            TermDictionaryEntry entry{};
            size_t copy_len = term.size();
            if (copy_len >= sizeof(entry.term))
            {
                copy_len = sizeof(entry.term) - 1;
            }
            std::memcpy(entry.term, term.data(), copy_len);
            entry.term[copy_len] = '\0';
            entry.term_hash = hashTerm(term);
            entry.doc_frequency = static_cast<uint32_t>(tids.size());
            entry.total_frequency = term_total_freq[term];
            entry.reserved = config_.compression_type;

            uint64_t new_offset = 0;
            uint32_t new_length = 0;
            status = writePostingList(merged_segment_id, tids, &new_offset, &new_length, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            entry.posting_offset = new_offset;
            entry.posting_length = new_length;

            GPID page_gpid = 0;
            uint16_t entry_index = 0;
            status = insertTerm(merged_segment_id, entry, &page_gpid, &entry_index, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }
    }

    SBInvertedIndexSegmentMeta merged_seg{};
    status = loadSegmentMeta(merged_segment_id, &merged_seg_gpid, &merged_seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }
    merged_seg.seg_num_documents = doc_stats.size();
    merged_seg.seg_num_terms = store_positions ? term_postings_positions.size() : term_postings.size();
    merged_seg.seg_num_tokens = total_doc_length;
    if (merged_seg.seg_num_documents > 0)
    {
        merged_seg.seg_avg_doc_length =
            static_cast<uint32_t>(total_doc_length / merged_seg.seg_num_documents);
    }
    status = updateSegmentMeta(merged_seg_gpid, merged_seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    for (uint32_t seg_id : segment_ids)
    {
        SBInvertedIndexSegmentMeta seg{};
        GPID seg_gpid = 0;
        status = loadSegmentMeta(seg_id, &seg_gpid, &seg, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        seg.seg_flags |= SEG_FLAG_MERGED;
        seg.seg_flags &= ~SEG_FLAG_ACTIVE;
        seg.seg_merged_at = static_cast<uint64_t>(std::time(nullptr));
        status = updateSegmentMeta(seg_gpid, seg, ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    SBInvertedIndexMetaPage meta{};
    status = loadMeta(&meta, ctx);
    if (status != Status::OK)
    {
        return status;
    }
    meta.ii_last_merge_time = static_cast<uint64_t>(std::time(nullptr));
    status = updateMeta(meta, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    doc_lengths_.clear();
    return Status::OK;
}

Status InvertedIndex::findTerm(uint32_t segment_id,
                               std::string_view term,
                               TermDictionaryEntry* entry_out,
                               GPID* page_gpid_out,
                               uint16_t* entry_index_out,
                               ErrorContext* ctx) const
{
    if (!entry_out || !page_gpid_out || !entry_index_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid findTerm outputs");
        return Status::INVALID_ARGUMENT;
    }

    SBInvertedIndexSegmentMeta seg{};
    GPID seg_gpid = 0;
    Status status = loadSegmentMeta(segment_id, &seg_gpid, &seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (seg.seg_dict_first_page == 0)
    {
        return Status::NOT_FOUND;
    }

    BufferPool* buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t term_hash = hashTerm(term);
    GPID current = seg.seg_dict_first_page;
    while (current != 0)
    {
        uint8_t* page_data = nullptr;
        status = buffer_pool->pinPageGlobal(current, reinterpret_cast<void**>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto* page = reinterpret_cast<SBTermDictionaryPage*>(page_data);
        uint16_t count = page->dict_num_entries;
        auto* entries = reinterpret_cast<TermDictionaryEntry*>(page->dict_entries);
        for (uint16_t i = 0; i < count; ++i)
        {
            size_t stored_len = 0;
            while (stored_len < sizeof(entries[i].term) && entries[i].term[stored_len] != '\0')
            {
                ++stored_len;
            }
            if (entries[i].term_hash == term_hash &&
                stored_len == term.size() &&
                std::memcmp(entries[i].term, term.data(), stored_len) == 0)
            {
                *entry_out = entries[i];
                *page_gpid_out = current;
                *entry_index_out = i;
                buffer_pool->unpinPageGlobal(current, false, ctx);
                return Status::OK;
            }
        }

        GPID next = page->dict_next_page;
        buffer_pool->unpinPageGlobal(current, false, ctx);
        current = next;
    }

    return Status::NOT_FOUND;
}

Status InvertedIndex::insertTerm(uint32_t segment_id,
                                 const TermDictionaryEntry& entry,
                                 GPID* page_gpid_out,
                                 uint16_t* entry_index_out,
                                 ErrorContext* ctx)
{
    if (!page_gpid_out || !entry_index_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid insertTerm outputs");
        return Status::INVALID_ARGUMENT;
    }

    SBInvertedIndexSegmentMeta seg{};
    GPID seg_gpid = 0;
    Status status = loadSegmentMeta(segment_id, &seg_gpid, &seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    PageManager* page_mgr = db_->page_manager();
    BufferPool* buffer_pool = db_->buffer_pool();
    if (!page_mgr || !buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing page manager or buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t page_size = db_->page_size();
    uint32_t max_entries = maxTermsPerPage(page_size);
    if (max_entries == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid page size for dictionary entries");
        return Status::INVALID_ARGUMENT;
    }

    GPID dict_page_gpid = seg.seg_dict_first_page;
    if (dict_page_gpid == 0)
    {
        status = page_mgr->allocatePageInTablespace(tablespace_id_, &dict_page_gpid, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        seg.seg_dict_first_page = dict_page_gpid;
        seg.seg_dict_num_pages = 1;
        status = updateSegmentMeta(seg_gpid, seg, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        uint8_t* dict_data = nullptr;
        status = buffer_pool->pinPageGlobal(dict_page_gpid, reinterpret_cast<void**>(&dict_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        std::memset(dict_data, 0, page_size);
        auto* dict_page = reinterpret_cast<SBTermDictionaryPage*>(dict_data);
        dict_page->dict_header.magic = K_MAGIC_SBRD;
        dict_page->dict_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
        dict_page->dict_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_INVERTED_DICT);
        dict_page->dict_header.page_size = page_size;
        dict_page->dict_header.page_id = static_cast<uint32_t>(getPageNumber(dict_page_gpid));
        dict_page->dict_header.generation = 1;
        dict_page->dict_header.checksum = 0;
        dict_page->dict_header.flags = 0;
        dict_page->dict_header.lsn = 0;
        pageSetLower(dict_page->dict_header, SB_TERM_DICTIONARY_PAGE_HEADER_SIZE);
        pageSetUpper(dict_page->dict_header, page_size);
        pageSetSpecial(dict_page->dict_header, page_size);
        dict_page->dict_next_page = 0;
        dict_page->dict_num_entries = 0;
        dict_page->dict_first_term_hash = 0;
        buffer_pool->unpinPageGlobal(dict_page_gpid, true, ctx);
    }

    GPID current = dict_page_gpid;
    GPID prev = 0;
    while (current != 0)
    {
        uint8_t* dict_data = nullptr;
        status = buffer_pool->pinPageGlobal(current, reinterpret_cast<void**>(&dict_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        auto* page = reinterpret_cast<SBTermDictionaryPage*>(dict_data);
        if (page->dict_num_entries < max_entries)
        {
            auto* entries = reinterpret_cast<TermDictionaryEntry*>(page->dict_entries);
            entries[page->dict_num_entries] = entry;
            page->dict_num_entries += 1;
            if (page->dict_num_entries == 1)
            {
                page->dict_first_term_hash = entry.term_hash;
            }
            *page_gpid_out = current;
            *entry_index_out = static_cast<uint16_t>(page->dict_num_entries - 1);
            buffer_pool->unpinPageGlobal(current, true, ctx);
            return Status::OK;
        }

        prev = current;
        current = page->dict_next_page;
        buffer_pool->unpinPageGlobal(prev, false, ctx);
    }

    GPID new_page_gpid = 0;
    status = page_mgr->allocatePageInTablespace(tablespace_id_, &new_page_gpid, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint8_t* prev_data = nullptr;
    status = buffer_pool->pinPageGlobal(prev, reinterpret_cast<void**>(&prev_data), ctx);
    if (status != Status::OK)
    {
        return status;
    }
    auto* prev_page = reinterpret_cast<SBTermDictionaryPage*>(prev_data);
    prev_page->dict_next_page = new_page_gpid;
    buffer_pool->unpinPageGlobal(prev, true, ctx);

    uint8_t* new_data = nullptr;
    status = buffer_pool->pinPageGlobal(new_page_gpid, reinterpret_cast<void**>(&new_data), ctx);
    if (status != Status::OK)
    {
        return status;
    }
    std::memset(new_data, 0, page_size);
    auto* new_page = reinterpret_cast<SBTermDictionaryPage*>(new_data);
    new_page->dict_header.magic = K_MAGIC_SBRD;
    new_page->dict_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
    new_page->dict_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_INVERTED_DICT);
    new_page->dict_header.page_size = page_size;
    new_page->dict_header.page_id = static_cast<uint32_t>(getPageNumber(new_page_gpid));
    new_page->dict_header.generation = 1;
    new_page->dict_header.checksum = 0;
    new_page->dict_header.flags = 0;
    new_page->dict_header.lsn = 0;
    pageSetLower(new_page->dict_header, SB_TERM_DICTIONARY_PAGE_HEADER_SIZE);
    pageSetUpper(new_page->dict_header, page_size);
    pageSetSpecial(new_page->dict_header, page_size);
    new_page->dict_next_page = 0;
    new_page->dict_num_entries = 1;
    new_page->dict_first_term_hash = entry.term_hash;
    auto* entries = reinterpret_cast<TermDictionaryEntry*>(new_page->dict_entries);
    entries[0] = entry;
    buffer_pool->unpinPageGlobal(new_page_gpid, true, ctx);

    seg.seg_dict_num_pages += 1;
    status = updateSegmentMeta(seg_gpid, seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    *page_gpid_out = new_page_gpid;
    *entry_index_out = 0;
    return Status::OK;
}

Status InvertedIndex::writePostingList(uint32_t segment_id,
                                       const std::vector<TID>& tids,
                                       uint64_t* posting_offset_out,
                                       uint32_t* posting_length_out,
                                       ErrorContext* ctx)
{
    if (!posting_offset_out || !posting_length_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid posting list outputs");
        return Status::INVALID_ARGUMENT;
    }

    SBInvertedIndexSegmentMeta seg{};
    GPID seg_gpid = 0;
    Status status = loadSegmentMeta(segment_id, &seg_gpid, &seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint8_t compression_type = config_.compression_type;
    std::vector<uint8_t> serialized;
    if (compression_type == II_COMPRESSION_VBYTE)
    {
        serialized.reserve(tids.size() * 4);
        uint64_t prev_gpid = 0;
        uint16_t prev_slot = 0;
        for (const auto& tid : tids)
        {
            uint64_t gpid_delta = tid.gpid - prev_gpid;
            uint64_t slot_delta = 0;
            if (gpid_delta == 0)
            {
                slot_delta = static_cast<uint64_t>(tid.slot - prev_slot);
            }
            else
            {
                slot_delta = tid.slot;
            }
            encodeVByte(gpid_delta, serialized);
            encodeVByte(slot_delta, serialized);
            prev_gpid = tid.gpid;
            prev_slot = tid.slot;
        }
    }
    else
    {
        serialized.reserve(tids.size() * 10);
        for (const auto& tid : tids)
        {
            uint64_t gpid = tid.gpid;
            uint16_t slot = tid.slot;
            size_t offset = serialized.size();
            serialized.resize(offset + 10);
            std::memcpy(serialized.data() + offset, &gpid, sizeof(uint64_t));
            std::memcpy(serialized.data() + offset + sizeof(uint64_t), &slot, sizeof(uint16_t));
        }
    }

    *posting_offset_out = seg.seg_total_posting_bytes;
    *posting_length_out = static_cast<uint32_t>(serialized.size());

    PageManager* page_mgr = db_->page_manager();
    BufferPool* buffer_pool = db_->buffer_pool();
    if (!page_mgr || !buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing page manager or buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t page_size = db_->page_size();
    uint32_t header_size = SB_POSTING_LIST_PAGE_HEADER_SIZE;
    uint32_t capacity = page_size - header_size;

    GPID posting_gpid = seg.seg_posting_first_page;
    if (posting_gpid == 0)
    {
        status = page_mgr->allocatePageInTablespace(tablespace_id_, &posting_gpid, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        seg.seg_posting_first_page = posting_gpid;
        seg.seg_posting_num_pages = 1;
        status = updateSegmentMeta(seg_gpid, seg, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        uint8_t* page_data = nullptr;
        status = buffer_pool->pinPageGlobal(posting_gpid, reinterpret_cast<void**>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        std::memset(page_data, 0, page_size);
        auto* page = reinterpret_cast<SBPostingListPage*>(page_data);
        page->post_header.magic = K_MAGIC_SBRD;
        page->post_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
        page->post_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_INVERTED_POSTINGS);
        page->post_header.page_size = page_size;
        page->post_header.page_id = static_cast<uint32_t>(getPageNumber(posting_gpid));
        page->post_header.generation = 1;
        page->post_header.checksum = 0;
        page->post_header.flags = 0;
        page->post_header.lsn = 0;
        pageSetLower(page->post_header, SB_POSTING_LIST_PAGE_HEADER_SIZE);
        pageSetUpper(page->post_header, page_size);
        pageSetSpecial(page->post_header, page_size);
        page->post_next_page = 0;
        page->post_data_length = 0;
        page->post_compression_type = compression_type;
        buffer_pool->unpinPageGlobal(posting_gpid, true, ctx);
    }

    GPID current = posting_gpid;
    GPID prev = 0;
    while (current != 0)
    {
        uint8_t* page_data = nullptr;
        status = buffer_pool->pinPageGlobal(current, reinterpret_cast<void**>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        auto* page = reinterpret_cast<SBPostingListPage*>(page_data);
        if (page->post_next_page == 0)
        {
            prev = current;
            buffer_pool->unpinPageGlobal(current, false, ctx);
            break;
        }
        prev = current;
        current = page->post_next_page;
        buffer_pool->unpinPageGlobal(prev, false, ctx);
    }

    if (prev == 0)
    {
        return Status::NOT_FOUND;
    }

    size_t remaining = serialized.size();
    size_t data_offset = 0;
    GPID page_gpid = prev;
    while (remaining > 0)
    {
        uint8_t* page_data = nullptr;
        status = buffer_pool->pinPageGlobal(page_gpid, reinterpret_cast<void**>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        auto* page = reinterpret_cast<SBPostingListPage*>(page_data);
        uint32_t used = page->post_data_length;
        if (used > capacity)
        {
            buffer_pool->unpinPageGlobal(page_gpid, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting page data length exceeds capacity");
            return Status::DATA_CORRUPTED;
        }
        uint32_t available = capacity - used;
        if (available == 0)
        {
            buffer_pool->unpinPageGlobal(page_gpid, false, ctx);
            GPID new_page = 0;
            status = page_mgr->allocatePageInTablespace(tablespace_id_, &new_page, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            uint8_t* prev_data = nullptr;
            status = buffer_pool->pinPageGlobal(page_gpid, reinterpret_cast<void**>(&prev_data), ctx);
            if (status != Status::OK)
            {
                return status;
            }
            auto* prev_page = reinterpret_cast<SBPostingListPage*>(prev_data);
            prev_page->post_next_page = new_page;
            buffer_pool->unpinPageGlobal(page_gpid, true, ctx);

            uint8_t* new_data = nullptr;
            status = buffer_pool->pinPageGlobal(new_page, reinterpret_cast<void**>(&new_data), ctx);
            if (status != Status::OK)
            {
                return status;
            }
            std::memset(new_data, 0, page_size);
            auto* new_page_ptr = reinterpret_cast<SBPostingListPage*>(new_data);
            new_page_ptr->post_header.magic = K_MAGIC_SBRD;
            new_page_ptr->post_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            new_page_ptr->post_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_INVERTED_POSTINGS);
            new_page_ptr->post_header.page_size = page_size;
            new_page_ptr->post_header.page_id = static_cast<uint32_t>(getPageNumber(new_page));
            new_page_ptr->post_header.generation = 1;
            new_page_ptr->post_header.checksum = 0;
            new_page_ptr->post_header.flags = 0;
            new_page_ptr->post_header.lsn = 0;
            pageSetLower(new_page_ptr->post_header, SB_POSTING_LIST_PAGE_HEADER_SIZE);
            pageSetUpper(new_page_ptr->post_header, page_size);
            pageSetSpecial(new_page_ptr->post_header, page_size);
            new_page_ptr->post_next_page = 0;
            new_page_ptr->post_data_length = 0;
            new_page_ptr->post_compression_type = compression_type;
            buffer_pool->unpinPageGlobal(new_page, true, ctx);

            seg.seg_posting_num_pages += 1;
            status = updateSegmentMeta(seg_gpid, seg, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            page_gpid = new_page;
            continue;
        }

        size_t chunk = remaining < available ? remaining : available;
        std::memcpy(page->post_data + used, serialized.data() + data_offset, chunk);
        page->post_data_length = static_cast<uint32_t>(used + chunk);
        page->post_compression_type = compression_type;
        buffer_pool->unpinPageGlobal(page_gpid, true, ctx);

        data_offset += chunk;
        remaining -= chunk;

        if (remaining > 0)
        {
            page_gpid = page->post_next_page;
        }
    }

    seg.seg_total_posting_bytes += serialized.size();
    return updateSegmentMeta(seg_gpid, seg, ctx);
}

Status InvertedIndex::writePostingListWithPositions(uint32_t segment_id,
                                                    const std::vector<PostingWithPositions>& postings,
                                                    uint64_t* posting_offset_out,
                                                    uint32_t* posting_length_out,
                                                    ErrorContext* ctx)
{
    if (!posting_offset_out || !posting_length_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid posting list outputs");
        return Status::INVALID_ARGUMENT;
    }

    SBInvertedIndexSegmentMeta seg{};
    GPID seg_gpid = 0;
    Status status = loadSegmentMeta(segment_id, &seg_gpid, &seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    bool store_offsets = (config_.features & II_FEATURE_OFFSETS) != 0;
    bool store_payloads = (config_.features & II_FEATURE_PAYLOADS) != 0;
    uint8_t compression_type = config_.compression_type;
    std::vector<uint8_t> serialized;
    if (compression_type == II_COMPRESSION_VBYTE)
    {
        serialized.reserve(postings.size() * 6);
        uint64_t prev_gpid = 0;
        uint16_t prev_slot = 0;
        for (const auto& entry : postings)
        {
            uint64_t gpid_delta = entry.tid.gpid - prev_gpid;
            uint64_t slot_delta = 0;
            if (gpid_delta == 0)
            {
                slot_delta = static_cast<uint64_t>(entry.tid.slot - prev_slot);
            }
            else
            {
                slot_delta = entry.tid.slot;
            }
            encodeVByte(gpid_delta, serialized);
            encodeVByte(slot_delta, serialized);
            encodeVByte(entry.positions.size(), serialized);
            uint32_t prev_pos = 0;
            uint32_t prev_start = 0;
            for (size_t i = 0; i < entry.positions.size(); ++i)
            {
                uint32_t pos = entry.positions[i];
                uint32_t delta = pos - prev_pos;
                encodeVByte(delta, serialized);
                prev_pos = pos;
                if (store_offsets)
                {
                    uint32_t start = 0;
                    uint32_t end = 0;
                    if (i < entry.offsets.size())
                    {
                        start = entry.offsets[i].first;
                        end = entry.offsets[i].second;
                    }
                    uint32_t start_delta = start - prev_start;
                    uint32_t length = end >= start ? end - start : 0;
                    encodeVByte(start_delta, serialized);
                    encodeVByte(length, serialized);
                    prev_start = start;
                }
                if (store_payloads)
                {
                    const std::vector<uint8_t>* payload = nullptr;
                    if (i < entry.payloads.size())
                    {
                        payload = &entry.payloads[i];
                    }
                    uint64_t payload_len = payload ? payload->size() : 0;
                    encodeVByte(payload_len, serialized);
                    if (payload && !payload->empty())
                    {
                        serialized.insert(serialized.end(), payload->begin(), payload->end());
                    }
                }
            }
            prev_gpid = entry.tid.gpid;
            prev_slot = entry.tid.slot;
        }
    }
    else
    {
        size_t total = 0;
        for (const auto& entry : postings)
        {
            total += sizeof(uint64_t) + sizeof(uint16_t) + sizeof(uint32_t);
            total += entry.positions.size() * sizeof(uint32_t);
            if (store_offsets)
            {
                total += entry.positions.size() * sizeof(uint32_t) * 2;
            }
            if (store_payloads)
            {
                total += entry.positions.size() * sizeof(uint32_t);
                for (size_t i = 0; i < entry.positions.size(); ++i)
                {
                    if (i < entry.payloads.size())
                    {
                        total += entry.payloads[i].size();
                    }
                }
            }
        }
        serialized.reserve(total);
        for (const auto& entry : postings)
        {
            uint64_t gpid = entry.tid.gpid;
            uint16_t slot = entry.tid.slot;
            uint32_t pos_count = static_cast<uint32_t>(entry.positions.size());
            size_t offset = serialized.size();
            serialized.resize(offset + sizeof(uint64_t) + sizeof(uint16_t) + sizeof(uint32_t));
            std::memcpy(serialized.data() + offset, &gpid, sizeof(uint64_t));
            std::memcpy(serialized.data() + offset + sizeof(uint64_t), &slot, sizeof(uint16_t));
            std::memcpy(serialized.data() + offset + sizeof(uint64_t) + sizeof(uint16_t),
                        &pos_count, sizeof(uint32_t));
            for (uint32_t i = 0; i < pos_count; ++i)
            {
                size_t pos_offset = serialized.size();
                serialized.resize(pos_offset + sizeof(uint32_t));
                std::memcpy(serialized.data() + pos_offset, &entry.positions[i], sizeof(uint32_t));
                if (store_offsets)
                {
                    uint32_t start = 0;
                    uint32_t end = 0;
                    if (i < entry.offsets.size())
                    {
                        start = entry.offsets[i].first;
                        end = entry.offsets[i].second;
                    }
                    size_t off_offset = serialized.size();
                    serialized.resize(off_offset + sizeof(uint32_t) * 2);
                    std::memcpy(serialized.data() + off_offset, &start, sizeof(uint32_t));
                    std::memcpy(serialized.data() + off_offset + sizeof(uint32_t), &end, sizeof(uint32_t));
                }
                if (store_payloads)
                {
                    uint32_t payload_len = 0;
                    const std::vector<uint8_t>* payload = nullptr;
                    if (i < entry.payloads.size())
                    {
                        payload = &entry.payloads[i];
                        payload_len = static_cast<uint32_t>(payload->size());
                    }
                    size_t len_offset = serialized.size();
                    serialized.resize(len_offset + sizeof(uint32_t));
                    std::memcpy(serialized.data() + len_offset, &payload_len, sizeof(uint32_t));
                    if (payload_len > 0 && payload)
                    {
                        size_t data_offset = serialized.size();
                        serialized.resize(data_offset + payload_len);
                        std::memcpy(serialized.data() + data_offset, payload->data(), payload_len);
                    }
                }
            }
        }
    }

    *posting_offset_out = seg.seg_total_posting_bytes;
    *posting_length_out = static_cast<uint32_t>(serialized.size());

    PageManager* page_mgr = db_->page_manager();
    BufferPool* buffer_pool = db_->buffer_pool();
    if (!page_mgr || !buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing page manager or buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t page_size = db_->page_size();
    uint32_t header_size = SB_POSTING_LIST_PAGE_HEADER_SIZE;
    uint32_t capacity = page_size - header_size;

    GPID posting_gpid = seg.seg_posting_first_page;
    if (posting_gpid == 0)
    {
        status = page_mgr->allocatePageInTablespace(tablespace_id_, &posting_gpid, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        seg.seg_posting_first_page = posting_gpid;
        seg.seg_posting_num_pages = 1;
        status = updateSegmentMeta(seg_gpid, seg, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        uint8_t* page_data = nullptr;
        status = buffer_pool->pinPageGlobal(posting_gpid, reinterpret_cast<void**>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        std::memset(page_data, 0, page_size);
        auto* page = reinterpret_cast<SBPostingListPage*>(page_data);
        page->post_header.magic = K_MAGIC_SBRD;
        page->post_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
        page->post_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_INVERTED_POSTINGS);
        page->post_header.page_size = page_size;
        page->post_header.page_id = static_cast<uint32_t>(getPageNumber(posting_gpid));
        page->post_header.generation = 1;
        page->post_header.checksum = 0;
        page->post_header.flags = 0;
        page->post_header.lsn = 0;
        pageSetLower(page->post_header, SB_POSTING_LIST_PAGE_HEADER_SIZE);
        pageSetUpper(page->post_header, page_size);
        pageSetSpecial(page->post_header, page_size);
        page->post_next_page = 0;
        page->post_data_length = 0;
        page->post_compression_type = compression_type;
        buffer_pool->unpinPageGlobal(posting_gpid, true, ctx);
    }

    GPID current = posting_gpid;
    GPID prev = 0;
    while (current != 0)
    {
        uint8_t* page_data = nullptr;
        status = buffer_pool->pinPageGlobal(current, reinterpret_cast<void**>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        auto* page = reinterpret_cast<SBPostingListPage*>(page_data);
        if (page->post_next_page == 0)
        {
            prev = current;
            buffer_pool->unpinPageGlobal(current, false, ctx);
            break;
        }
        prev = current;
        current = page->post_next_page;
        buffer_pool->unpinPageGlobal(prev, false, ctx);
    }

    if (prev == 0)
    {
        return Status::NOT_FOUND;
    }

    size_t remaining = serialized.size();
    size_t data_offset = 0;
    GPID page_gpid = prev;
    while (remaining > 0)
    {
        uint8_t* page_data = nullptr;
        status = buffer_pool->pinPageGlobal(page_gpid, reinterpret_cast<void**>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        auto* page = reinterpret_cast<SBPostingListPage*>(page_data);
        uint32_t used = page->post_data_length;
        if (used > capacity)
        {
            buffer_pool->unpinPageGlobal(page_gpid, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting page data length exceeds capacity");
            return Status::DATA_CORRUPTED;
        }
        uint32_t available = capacity - used;
        if (available == 0)
        {
            buffer_pool->unpinPageGlobal(page_gpid, false, ctx);
            GPID new_page = 0;
            status = page_mgr->allocatePageInTablespace(tablespace_id_, &new_page, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            uint8_t* prev_data = nullptr;
            status = buffer_pool->pinPageGlobal(page_gpid, reinterpret_cast<void**>(&prev_data), ctx);
            if (status != Status::OK)
            {
                return status;
            }
            auto* prev_page = reinterpret_cast<SBPostingListPage*>(prev_data);
            prev_page->post_next_page = new_page;
            buffer_pool->unpinPageGlobal(page_gpid, true, ctx);

            uint8_t* new_data = nullptr;
            status = buffer_pool->pinPageGlobal(new_page, reinterpret_cast<void**>(&new_data), ctx);
            if (status != Status::OK)
            {
                return status;
            }
            std::memset(new_data, 0, page_size);
            auto* new_page_ptr = reinterpret_cast<SBPostingListPage*>(new_data);
            new_page_ptr->post_header.magic = K_MAGIC_SBRD;
            new_page_ptr->post_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            new_page_ptr->post_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_INVERTED_POSTINGS);
            new_page_ptr->post_header.page_size = page_size;
            new_page_ptr->post_header.page_id = static_cast<uint32_t>(getPageNumber(new_page));
            new_page_ptr->post_header.generation = 1;
            new_page_ptr->post_header.checksum = 0;
            new_page_ptr->post_header.flags = 0;
            new_page_ptr->post_header.lsn = 0;
            pageSetLower(new_page_ptr->post_header, SB_POSTING_LIST_PAGE_HEADER_SIZE);
            pageSetUpper(new_page_ptr->post_header, page_size);
            pageSetSpecial(new_page_ptr->post_header, page_size);
            new_page_ptr->post_next_page = 0;
            new_page_ptr->post_data_length = 0;
            new_page_ptr->post_compression_type = compression_type;
            buffer_pool->unpinPageGlobal(new_page, true, ctx);

            seg.seg_posting_num_pages += 1;
            status = updateSegmentMeta(seg_gpid, seg, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            page_gpid = new_page;
            continue;
        }

        size_t chunk = remaining < available ? remaining : available;
        std::memcpy(page->post_data + used, serialized.data() + data_offset, chunk);
        page->post_data_length = static_cast<uint32_t>(used + chunk);
        page->post_compression_type = compression_type;
        buffer_pool->unpinPageGlobal(page_gpid, true, ctx);

        data_offset += chunk;
        remaining -= chunk;

        if (remaining > 0)
        {
            page_gpid = page->post_next_page;
        }
    }

    seg.seg_total_posting_bytes += serialized.size();
    return updateSegmentMeta(seg_gpid, seg, ctx);
}

Status InvertedIndex::readPostingList(uint32_t segment_id,
                                      uint64_t posting_offset,
                                      uint32_t posting_length,
                                      std::vector<TID>* tids_out,
                                      ErrorContext* ctx) const
{
    if (!tids_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid tids output");
        return Status::INVALID_ARGUMENT;
    }
    tids_out->clear();

    SBInvertedIndexSegmentMeta seg{};
    GPID seg_gpid = 0;
    Status status = loadSegmentMeta(segment_id, &seg_gpid, &seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (seg.seg_posting_first_page == 0 || posting_length == 0)
    {
        return Status::OK;
    }

    bool store_offsets = (config_.features & II_FEATURE_OFFSETS) != 0;
    bool store_payloads = (config_.features & II_FEATURE_PAYLOADS) != 0;

    BufferPool* buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t page_size = db_->page_size();
    uint32_t header_size = SB_POSTING_LIST_PAGE_HEADER_SIZE;
    uint32_t capacity = page_size - header_size;

    std::vector<uint8_t> raw;
    raw.resize(posting_length);
    uint8_t compression_type = II_COMPRESSION_NONE;
    bool compression_set = false;

    GPID current = seg.seg_posting_first_page;
    uint64_t offset = posting_offset;
    size_t copied = 0;
    while (current != 0 && copied < posting_length)
    {
        uint8_t* page_data = nullptr;
        status = buffer_pool->pinPageGlobal(current, reinterpret_cast<void**>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        auto* page = reinterpret_cast<SBPostingListPage*>(page_data);
        uint32_t used = page->post_data_length;
        if (used > capacity)
        {
            buffer_pool->unpinPageGlobal(current, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting page data length exceeds capacity");
            return Status::DATA_CORRUPTED;
        }

        if (offset >= used)
        {
            offset -= used;
            GPID next = page->post_next_page;
            buffer_pool->unpinPageGlobal(current, false, ctx);
            current = next;
            continue;
        }

        uint32_t available = used - static_cast<uint32_t>(offset);
        size_t chunk = posting_length - copied;
        if (chunk > available)
        {
            chunk = available;
        }
        if (!compression_set)
        {
            compression_type = page->post_compression_type;
            compression_set = true;
        }
        std::memcpy(raw.data() + copied, page->post_data + offset, chunk);
        copied += chunk;
        offset = 0;
        GPID next = page->post_next_page;
        buffer_pool->unpinPageGlobal(current, false, ctx);
        current = next;
    }

    if (copied != posting_length)
    {
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting list truncated");
        return Status::DATA_CORRUPTED;
    }

    if (compression_type == II_COMPRESSION_VBYTE)
    {
        size_t decode_offset = 0;
        uint64_t prev_gpid = 0;
        uint16_t prev_slot = 0;
        while (decode_offset < raw.size())
        {
            uint64_t gpid_delta = 0;
            uint64_t slot_delta = 0;
            if (!decodeVByte(raw.data(), raw.size(), &decode_offset, &gpid_delta) ||
                !decodeVByte(raw.data(), raw.size(), &decode_offset, &slot_delta))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting list decode failed");
                return Status::DATA_CORRUPTED;
            }
            uint64_t gpid = 0;
            uint16_t slot = 0;
            if (gpid_delta == 0)
            {
                gpid = prev_gpid;
                slot = static_cast<uint16_t>(prev_slot + slot_delta);
            }
            else
            {
                gpid = prev_gpid + gpid_delta;
                slot = static_cast<uint16_t>(slot_delta);
            }
            tids_out->push_back(TID{gpid, slot});
            prev_gpid = gpid;
            prev_slot = slot;
        }
    }
    else if (compression_type == II_COMPRESSION_NONE)
    {
        if (posting_length % 10 != 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting list length is misaligned");
            return Status::DATA_CORRUPTED;
        }
        size_t tid_count = posting_length / 10;
        tids_out->reserve(tid_count);
        for (size_t i = 0; i < tid_count; ++i)
        {
            uint64_t gpid = 0;
            uint16_t slot = 0;
            std::memcpy(&gpid, raw.data() + i * 10, sizeof(uint64_t));
            std::memcpy(&slot, raw.data() + i * 10 + sizeof(uint64_t), sizeof(uint16_t));
            tids_out->push_back(TID{gpid, slot});
        }
    }
    else
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Unsupported posting list compression");
        return Status::NOT_SUPPORTED;
    }

    return Status::OK;
}

Status InvertedIndex::readPostingListWithPositions(uint32_t segment_id,
                                                   uint64_t posting_offset,
                                                   uint32_t posting_length,
                                                   std::vector<PostingWithPositions>* postings_out,
                                                   ErrorContext* ctx) const
{
    if (!postings_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid postings output");
        return Status::INVALID_ARGUMENT;
    }
    postings_out->clear();

    SBInvertedIndexSegmentMeta seg{};
    GPID seg_gpid = 0;
    Status status = loadSegmentMeta(segment_id, &seg_gpid, &seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (seg.seg_posting_first_page == 0 || posting_length == 0)
    {
        return Status::OK;
    }

    bool store_offsets = (config_.features & II_FEATURE_OFFSETS) != 0;
    bool store_payloads = (config_.features & II_FEATURE_PAYLOADS) != 0;

    BufferPool* buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t page_size = db_->page_size();
    uint32_t header_size = SB_POSTING_LIST_PAGE_HEADER_SIZE;
    uint32_t capacity = page_size - header_size;

    std::vector<uint8_t> raw;
    raw.resize(posting_length);
    uint8_t compression_type = II_COMPRESSION_NONE;
    bool compression_set = false;

    GPID current = seg.seg_posting_first_page;
    uint64_t offset = posting_offset;
    size_t copied = 0;
    while (current != 0 && copied < posting_length)
    {
        uint8_t* page_data = nullptr;
        status = buffer_pool->pinPageGlobal(current, reinterpret_cast<void**>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        auto* page = reinterpret_cast<SBPostingListPage*>(page_data);
        uint32_t used = page->post_data_length;
        if (used > capacity)
        {
            buffer_pool->unpinPageGlobal(current, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting page data length exceeds capacity");
            return Status::DATA_CORRUPTED;
        }

        if (offset >= used)
        {
            offset -= used;
            GPID next = page->post_next_page;
            buffer_pool->unpinPageGlobal(current, false, ctx);
            current = next;
            continue;
        }

        uint32_t available = used - static_cast<uint32_t>(offset);
        size_t chunk = posting_length - copied;
        if (chunk > available)
        {
            chunk = available;
        }
        if (!compression_set)
        {
            compression_type = page->post_compression_type;
            compression_set = true;
        }
        std::memcpy(raw.data() + copied, page->post_data + offset, chunk);
        copied += chunk;
        offset = 0;
        GPID next = page->post_next_page;
        buffer_pool->unpinPageGlobal(current, false, ctx);
        current = next;
    }

    if (copied != posting_length)
    {
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting list truncated");
        return Status::DATA_CORRUPTED;
    }

    if (compression_type == II_COMPRESSION_VBYTE)
    {
        size_t decode_offset = 0;
        uint64_t prev_gpid = 0;
        uint16_t prev_slot = 0;
        while (decode_offset < raw.size())
        {
            uint64_t gpid_delta = 0;
            uint64_t slot_delta = 0;
            uint64_t pos_count = 0;
            if (!decodeVByte(raw.data(), raw.size(), &decode_offset, &gpid_delta) ||
                !decodeVByte(raw.data(), raw.size(), &decode_offset, &slot_delta) ||
                !decodeVByte(raw.data(), raw.size(), &decode_offset, &pos_count))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting list decode failed");
                return Status::DATA_CORRUPTED;
            }
            uint64_t gpid = 0;
            uint16_t slot = 0;
            if (gpid_delta == 0)
            {
                gpid = prev_gpid;
                slot = static_cast<uint16_t>(prev_slot + slot_delta);
            }
            else
            {
                gpid = prev_gpid + gpid_delta;
                slot = static_cast<uint16_t>(slot_delta);
            }
            PostingWithPositions entry;
            entry.tid = TID{gpid, slot};
            entry.positions.reserve(static_cast<size_t>(pos_count));
            if (store_offsets)
            {
                entry.offsets.reserve(static_cast<size_t>(pos_count));
            }
            if (store_payloads)
            {
                entry.payloads.reserve(static_cast<size_t>(pos_count));
            }
            uint32_t prev_pos = 0;
            uint32_t prev_start = 0;
            for (uint64_t i = 0; i < pos_count; ++i)
            {
                uint64_t delta = 0;
                if (!decodeVByte(raw.data(), raw.size(), &decode_offset, &delta))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting list decode failed");
                    return Status::DATA_CORRUPTED;
                }
                prev_pos += static_cast<uint32_t>(delta);
                entry.positions.push_back(prev_pos);
                if (store_offsets)
                {
                    uint64_t start_delta = 0;
                    uint64_t length = 0;
                    if (!decodeVByte(raw.data(), raw.size(), &decode_offset, &start_delta) ||
                        !decodeVByte(raw.data(), raw.size(), &decode_offset, &length))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting list decode failed");
                        return Status::DATA_CORRUPTED;
                    }
                    uint32_t start = prev_start + static_cast<uint32_t>(start_delta);
                    uint32_t end = start + static_cast<uint32_t>(length);
                    entry.offsets.push_back({start, end});
                    prev_start = start;
                }
                if (store_payloads)
                {
                    uint64_t payload_len = 0;
                    if (!decodeVByte(raw.data(), raw.size(), &decode_offset, &payload_len))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting list decode failed");
                        return Status::DATA_CORRUPTED;
                    }
                    if (decode_offset + payload_len > raw.size())
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting list decode failed");
                        return Status::DATA_CORRUPTED;
                    }
                    std::vector<uint8_t> payload;
                    if (payload_len > 0)
                    {
                        payload.resize(static_cast<size_t>(payload_len));
                        std::memcpy(payload.data(), raw.data() + decode_offset,
                                    static_cast<size_t>(payload_len));
                        decode_offset += payload_len;
                    }
                    entry.payloads.push_back(std::move(payload));
                }
            }
            postings_out->push_back(std::move(entry));
            prev_gpid = gpid;
            prev_slot = slot;
        }
    }
    else if (compression_type == II_COMPRESSION_NONE)
    {
        size_t decode_offset = 0;
        while (decode_offset < raw.size())
        {
            size_t remaining = raw.size() - decode_offset;
            if (remaining < sizeof(uint64_t) + sizeof(uint16_t) + sizeof(uint32_t))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting list truncated");
                return Status::DATA_CORRUPTED;
            }
            uint64_t gpid = 0;
            uint16_t slot = 0;
            uint32_t pos_count = 0;
            std::memcpy(&gpid, raw.data() + decode_offset, sizeof(uint64_t));
            decode_offset += sizeof(uint64_t);
            std::memcpy(&slot, raw.data() + decode_offset, sizeof(uint16_t));
            decode_offset += sizeof(uint16_t);
            std::memcpy(&pos_count, raw.data() + decode_offset, sizeof(uint32_t));
            decode_offset += sizeof(uint32_t);
            PostingWithPositions entry;
            entry.tid = TID{gpid, slot};
            entry.positions.reserve(pos_count);
            if (store_offsets)
            {
                entry.offsets.reserve(pos_count);
            }
            if (store_payloads)
            {
                entry.payloads.reserve(pos_count);
            }
            for (uint32_t i = 0; i < pos_count; ++i)
            {
                if (decode_offset + sizeof(uint32_t) > raw.size())
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting list truncated");
                    return Status::DATA_CORRUPTED;
                }
                uint32_t pos = 0;
                std::memcpy(&pos, raw.data() + decode_offset, sizeof(uint32_t));
                decode_offset += sizeof(uint32_t);
                entry.positions.push_back(pos);
                if (store_offsets)
                {
                    if (decode_offset + sizeof(uint32_t) * 2 > raw.size())
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting list truncated");
                        return Status::DATA_CORRUPTED;
                    }
                    uint32_t start = 0;
                    uint32_t end = 0;
                    std::memcpy(&start, raw.data() + decode_offset, sizeof(uint32_t));
                    decode_offset += sizeof(uint32_t);
                    std::memcpy(&end, raw.data() + decode_offset, sizeof(uint32_t));
                    decode_offset += sizeof(uint32_t);
                    entry.offsets.push_back({start, end});
                }
                if (store_payloads)
                {
                    if (decode_offset + sizeof(uint32_t) > raw.size())
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting list truncated");
                        return Status::DATA_CORRUPTED;
                    }
                    uint32_t payload_len = 0;
                    std::memcpy(&payload_len, raw.data() + decode_offset, sizeof(uint32_t));
                    decode_offset += sizeof(uint32_t);
                    if (decode_offset + payload_len > raw.size())
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Posting list truncated");
                        return Status::DATA_CORRUPTED;
                    }
                    std::vector<uint8_t> payload;
                    if (payload_len > 0)
                    {
                        payload.resize(payload_len);
                        std::memcpy(payload.data(), raw.data() + decode_offset, payload_len);
                        decode_offset += payload_len;
                    }
                    entry.payloads.push_back(std::move(payload));
                }
            }
            postings_out->push_back(std::move(entry));
        }
    }
    else
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Unsupported posting list compression");
        return Status::NOT_SUPPORTED;
    }

    return Status::OK;
}

Status InvertedIndex::updateTermEntry(GPID page_gpid,
                                      uint16_t entry_index,
                                      const TermDictionaryEntry& entry,
                                      ErrorContext* ctx) const
{
    BufferPool* buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint8_t* page_data = nullptr;
    Status status = buffer_pool->pinPageGlobal(page_gpid, reinterpret_cast<void**>(&page_data), ctx);
    if (status != Status::OK)
    {
        return status;
    }

    auto* page = reinterpret_cast<SBTermDictionaryPage*>(page_data);
    if (entry_index >= page->dict_num_entries)
    {
        buffer_pool->unpinPageGlobal(page_gpid, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Dictionary entry index out of range");
        return Status::INVALID_ARGUMENT;
    }

    auto* entries = reinterpret_cast<TermDictionaryEntry*>(page->dict_entries);
    entries[entry_index] = entry;
    buffer_pool->unpinPageGlobal(page_gpid, true, ctx);
    return Status::OK;
}

Status InvertedIndex::create(Database* db,
                             const ID& index_uuid,
                             const ID& table_uuid,
                             const ID& column_uuid,
                             GPID meta_gpid,
                             const InvertedIndexConfig& config,
                             ErrorContext* ctx)
{
    if (!db || meta_gpid == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Invalid arguments to InvertedIndex::create");
        return Status::INVALID_ARGUMENT;
    }

    PageManager* page_mgr = db->page_manager();
    BufferPool* buffer_pool = db->buffer_pool();
    if (!page_mgr || !buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "InvertedIndex::create missing page manager or buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t page_size = db->page_size();
    uint16_t tablespace_id = getTablespaceID(meta_gpid);

    uint8_t* meta_data = nullptr;
    Status status = buffer_pool->pinPageGlobal(meta_gpid, reinterpret_cast<void**>(&meta_data), ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::memset(meta_data, 0, page_size);
    auto* meta = reinterpret_cast<SBInvertedIndexMetaPage*>(meta_data);
    meta->ii_header.magic = K_MAGIC_SBRD;
    meta->ii_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
    meta->ii_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_INVERTED_META);
    meta->ii_header.page_size = page_size;
    meta->ii_header.page_id = static_cast<uint32_t>(getPageNumber(meta_gpid));
    meta->ii_header.generation = 1;
    meta->ii_header.checksum = 0;
    meta->ii_header.flags = 0;
    meta->ii_header.lsn = 0;
    pageSetLower(meta->ii_header, SB_INVERTED_META_PAGE_HEADER_SIZE);
    pageSetUpper(meta->ii_header, page_size);
    pageSetSpecial(meta->ii_header, page_size);

    std::memcpy(meta->ii_index_uuid, index_uuid.bytes.data(), sizeof(meta->ii_index_uuid));
    std::memcpy(meta->ii_table_uuid, table_uuid.bytes.data(), sizeof(meta->ii_table_uuid));
    std::memcpy(meta->ii_column_uuid, column_uuid.bytes.data(), sizeof(meta->ii_column_uuid));
    meta->ii_language = config.language;
    meta->ii_num_segments = 1;
    meta->ii_active_segment = 0;
    meta->ii_total_documents = 0;
    meta->ii_total_terms = 0;
    meta->ii_total_tokens = 0;
    meta->ii_avg_doc_length = 0;
    meta->ii_features = config.features;
    meta->ii_compression_type = config.compression_type;
    uint16_t min_len = config.min_term_length ? config.min_term_length : 2;
    uint16_t max_len = config.max_term_length ? config.max_term_length : 40;
    if (min_len > max_len)
    {
        buffer_pool->unpinPageGlobal(meta_gpid, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "InvertedIndex min_term_length exceeds max_term_length");
        return Status::INVALID_ARGUMENT;
    }
    writeUint16LE(meta->ii_reserved1, min_len);
    writeUint16LE(meta->ii_reserved1 + 2, max_len);

    GPID seg_meta_gpid = 0;
    status = page_mgr->allocatePageInTablespace(tablespace_id, &seg_meta_gpid, ctx);
    if (status != Status::OK)
    {
        buffer_pool->unpinPageGlobal(meta_gpid, false, ctx);
        return status;
    }

    meta->ii_segment_pages[0] = seg_meta_gpid;

    buffer_pool->unpinPageGlobal(meta_gpid, true, ctx);

    uint8_t* seg_data = nullptr;
    status = buffer_pool->pinPageGlobal(seg_meta_gpid, reinterpret_cast<void**>(&seg_data), ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::memset(seg_data, 0, page_size);
    auto* seg = reinterpret_cast<SBInvertedIndexSegmentMeta*>(seg_data);
    seg->seg_header.magic = K_MAGIC_SBRD;
    seg->seg_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
    seg->seg_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_INVERTED_DICT);
    seg->seg_header.page_size = page_size;
    seg->seg_header.page_id = static_cast<uint32_t>(getPageNumber(seg_meta_gpid));
    seg->seg_header.generation = 1;
    seg->seg_header.checksum = 0;
    seg->seg_header.flags = 0;
    seg->seg_header.lsn = 0;
    pageSetLower(seg->seg_header, SB_INVERTED_SEGMENT_META_HEADER_SIZE);
    pageSetUpper(seg->seg_header, page_size);
    pageSetSpecial(seg->seg_header, page_size);
    seg->seg_id = 0;
    seg->seg_num_documents = 0;
    seg->seg_num_terms = 0;
    seg->seg_num_tokens = 0;
    seg->seg_avg_doc_length = 0;
    seg->seg_created_at = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    seg->seg_merged_at = 0;
    seg->seg_flags = 0x01;
    seg->seg_dict_first_page = 0;
    seg->seg_dict_num_pages = 0;
    seg->seg_posting_first_page = 0;
    seg->seg_posting_num_pages = 0;
    seg->seg_docstats_page = 0;
    seg->seg_delete_bitmap_page = 0;
    seg->seg_total_posting_bytes = 0;

    buffer_pool->unpinPageGlobal(seg_meta_gpid, true, ctx);

    return Status::OK;
}

std::unique_ptr<InvertedIndex> InvertedIndex::open(Database* db,
                                                   const ID& index_uuid,
                                                   const ID& table_uuid,
                                                   const ID& column_uuid,
                                                   GPID meta_gpid,
                                                   ErrorContext* ctx)
{
    if (!db || meta_gpid == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Invalid arguments to InvertedIndex::open");
        return nullptr;
    }

    BufferPool* buffer_pool = db->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "InvertedIndex::open missing buffer pool");
        return nullptr;
    }

    uint8_t* meta_data = nullptr;
    Status status = buffer_pool->pinPageGlobal(meta_gpid, reinterpret_cast<void**>(&meta_data), ctx);
    if (status != Status::OK)
    {
        return nullptr;
    }

    auto* meta = reinterpret_cast<SBInvertedIndexMetaPage*>(meta_data);
    if (meta->ii_header.page_type != static_cast<uint16_t>(PageType::PAGE_TYPE_INVERTED_META))
    {
        buffer_pool->unpinPageGlobal(meta_gpid, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid inverted index meta page");
        return nullptr;
    }

    if (std::memcmp(meta->ii_index_uuid, index_uuid.bytes.data(), sizeof(meta->ii_index_uuid)) != 0)
    {
        buffer_pool->unpinPageGlobal(meta_gpid, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Inverted index UUID mismatch");
        return nullptr;
    }

    InvertedIndexConfig config;
    config.language = meta->ii_language;
    config.features = meta->ii_features;
    config.compression_type = meta->ii_compression_type;
    uint16_t min_len = readUint16LE(meta->ii_reserved1);
    uint16_t max_len = readUint16LE(meta->ii_reserved1 + 2);
    if (min_len == 0)
    {
        min_len = 2;
    }
    if (max_len == 0)
    {
        max_len = 40;
    }
    if (min_len > max_len)
    {
        min_len = 2;
        max_len = 40;
    }
    config.min_term_length = min_len;
    config.max_term_length = max_len;

    uint32_t active_segment = meta->ii_active_segment;
    buffer_pool->unpinPageGlobal(meta_gpid, false, ctx);

    auto index = std::unique_ptr<InvertedIndex>(
        new InvertedIndex(db, index_uuid, table_uuid, column_uuid, meta_gpid, config));
    Status load_status = index->loadDocStatsMap(active_segment, true, ctx);
    if (load_status != Status::OK)
    {
        return nullptr;
    }
    return index;
}

Status InvertedIndex::insert(const void* document_data,
                             size_t document_len,
                             const TID& tid,
                             ErrorContext* ctx)
{
    if (!document_data || document_len == 0)
    {
        return Status::OK;
    }

    std::string_view text(reinterpret_cast<const char*>(document_data), document_len);
    bool filter_stop = (config_.features & II_FEATURE_STOP_WORDS) != 0;
    bool store_offsets = (config_.features & II_FEATURE_OFFSETS) != 0;
    bool store_payloads = (config_.features & II_FEATURE_PAYLOADS) != 0;
    bool store_positions = (config_.features & II_FEATURE_POSITIONS) != 0 || store_offsets || store_payloads;
    std::unordered_map<std::string, uint32_t> term_counts;
    std::unordered_map<std::string, std::vector<uint32_t>> term_positions;
    std::unordered_map<std::string, std::vector<std::pair<uint32_t, uint32_t>>> term_offsets;
    std::unordered_map<std::string, std::vector<std::vector<uint8_t>>> term_payloads;
    if (store_positions)
    {
        auto tokens = tokenizeAsciiWithOffsets(text, filter_stop,
                                               config_.min_term_length,
                                               config_.max_term_length,
                                               (config_.features & II_FEATURE_STEMMING) != 0,
                                               config_.language);
        if (tokens.empty())
        {
            return Status::OK;
        }
        for (const auto& token : tokens)
        {
            term_positions[token.term].push_back(token.position);
            if (store_offsets)
            {
                term_offsets[token.term].push_back({token.start_offset, token.end_offset});
            }
            if (store_payloads)
            {
                term_payloads[token.term].push_back({});
            }
        }
        for (const auto& [term, positions] : term_positions)
        {
            term_counts[term] = static_cast<uint32_t>(positions.size());
        }
    }
    else
    {
        auto tokens = tokenizeAscii(text, filter_stop,
                                    config_.min_term_length,
                                    config_.max_term_length,
                                    (config_.features & II_FEATURE_STEMMING) != 0,
                                    config_.language);
        if (tokens.empty())
        {
            return Status::OK;
        }
        for (const auto& token : tokens)
        {
            ++term_counts[token];
        }
    }
    uint32_t token_count = 0;
    for (const auto& [term, count] : term_counts)
    {
        (void)term;
        token_count += count;
    }

    SBInvertedIndexMetaPage meta{};
    Status status = loadMeta(&meta, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint32_t segment_id = meta.ii_active_segment;
    SBInvertedIndexSegmentMeta seg{};
    GPID seg_gpid = 0;
    status = loadSegmentMeta(segment_id, &seg_gpid, &seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint64_t new_terms = 0;
    for (const auto& [term, count] : term_counts)
    {
        TermDictionaryEntry entry{};
        GPID page_gpid = 0;
        uint16_t entry_index = 0;
        Status find_status = findTerm(segment_id, term, &entry, &page_gpid, &entry_index, ctx);

        bool tid_exists = false;
        std::vector<TID> tids;
        std::vector<PostingWithPositions> postings;
        if (find_status == Status::OK)
        {
            if (store_positions)
            {
                status = readPostingListWithPositions(segment_id, entry.posting_offset,
                                                     entry.posting_length, &postings, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                for (const auto& item : postings)
                {
                    if (item.tid == tid)
                    {
                        tid_exists = true;
                        break;
                    }
                }
            }
            else
            {
                status = readPostingList(segment_id, entry.posting_offset, entry.posting_length, &tids, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                tid_exists = std::find(tids.begin(), tids.end(), tid) != tids.end();
            }
        }
        else if (find_status != Status::NOT_FOUND)
        {
            return find_status;
        }

        if (!tid_exists)
        {
            if (store_positions)
            {
                PostingWithPositions new_entry;
                new_entry.tid = tid;
                new_entry.positions = term_positions[term];
                if (store_offsets)
                {
                    new_entry.offsets = term_offsets[term];
                }
                if (store_payloads)
                {
                    new_entry.payloads = term_payloads[term];
                }
                postings.push_back(std::move(new_entry));
                std::sort(postings.begin(), postings.end(),
                          [](const PostingWithPositions& a, const PostingWithPositions& b) {
                              return a.tid < b.tid;
                          });
            }
            else
            {
                tids.push_back(tid);
                sortUnique(tids);
            }
            if (find_status == Status::OK)
            {
                entry.doc_frequency += 1;
            }
            else
            {
                entry.doc_frequency = 1;
            }
        }

        if (find_status == Status::NOT_FOUND)
        {
            std::memset(&entry, 0, sizeof(entry));
            size_t copy_len = term.size();
            if (copy_len >= sizeof(entry.term))
            {
                copy_len = sizeof(entry.term) - 1;
            }
            std::memcpy(entry.term, term.data(), copy_len);
            entry.term[copy_len] = '\0';
            entry.term_hash = hashTerm(term);
            entry.doc_frequency = 1;
            entry.total_frequency = count;
            entry.reserved = config_.compression_type;

            uint64_t new_offset = 0;
            uint32_t new_length = 0;
            if (store_positions)
            {
                status = writePostingListWithPositions(segment_id, postings, &new_offset, &new_length, ctx);
            }
            else
            {
                status = writePostingList(segment_id, tids, &new_offset, &new_length, ctx);
            }
            if (status != Status::OK)
            {
                return status;
            }
            entry.posting_offset = new_offset;
            entry.posting_length = new_length;

            status = insertTerm(segment_id, entry, &page_gpid, &entry_index, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            new_terms += 1;
        }
        else
        {
            if (!tid_exists)
            {
                entry.total_frequency += count;
            }
            uint64_t new_offset = 0;
            uint32_t new_length = 0;
            if (store_positions)
            {
                status = writePostingListWithPositions(segment_id, postings, &new_offset, &new_length, ctx);
            }
            else
            {
                status = writePostingList(segment_id, tids, &new_offset, &new_length, ctx);
            }
            if (status != Status::OK)
            {
                return status;
            }
            entry.posting_offset = new_offset;
            entry.posting_length = new_length;
            status = updateTermEntry(page_gpid, entry_index, entry, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }
    }

    SBInvertedIndexSegmentMeta seg_latest{};
    status = loadSegmentMeta(segment_id, &seg_gpid, &seg_latest, ctx);
    if (status != Status::OK)
    {
        return status;
    }
    seg_latest.seg_num_documents += 1;
    seg_latest.seg_num_tokens += token_count;
    seg_latest.seg_num_terms += new_terms;
    if (seg_latest.seg_num_documents > 0)
    {
        seg_latest.seg_avg_doc_length =
            static_cast<uint32_t>(seg_latest.seg_num_tokens / seg_latest.seg_num_documents);
    }
    status = updateSegmentMeta(seg_gpid, seg_latest, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    meta.ii_total_documents += 1;
    meta.ii_total_tokens += token_count;
    meta.ii_total_terms += new_terms;
    if (meta.ii_total_documents > 0)
    {
        meta.ii_avg_doc_length =
            static_cast<uint32_t>(meta.ii_total_tokens / meta.ii_total_documents);
    }
    status = updateMeta(meta, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint32_t unique_terms = static_cast<uint32_t>(term_counts.size());
    status = updateDocStats(segment_id, tid, token_count, unique_terms, ctx);
    if (status != Status::OK)
    {
        return status;
    }
    doc_lengths_[tid] = token_count;

    status = maybeRotateSegment(&meta, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    status = loadMeta(&meta, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    status = maybeMergeSegments(&meta, ctx);
    if (status != Status::OK)
    {
        return status;
    }
    return Status::OK;
}

Status InvertedIndex::remove(const void* document_data,
                             size_t document_len,
                             const TID& tid,
                             uint64_t current_xid,
                             ErrorContext* ctx)
{
    (void)current_xid;
    if (!document_data || document_len == 0)
    {
        return Status::OK;
    }

    std::string_view text(reinterpret_cast<const char*>(document_data), document_len);
    bool filter_stop = (config_.features & II_FEATURE_STOP_WORDS) != 0;
    bool store_offsets = (config_.features & II_FEATURE_OFFSETS) != 0;
    bool store_payloads = (config_.features & II_FEATURE_PAYLOADS) != 0;
    bool store_positions = (config_.features & II_FEATURE_POSITIONS) != 0 || store_offsets || store_payloads;
    std::unordered_map<std::string, uint32_t> term_counts;
    std::unordered_map<std::string, std::vector<uint32_t>> term_positions;
    std::unordered_map<std::string, std::vector<std::pair<uint32_t, uint32_t>>> term_offsets;
    std::unordered_map<std::string, std::vector<std::vector<uint8_t>>> term_payloads;
    if (store_positions)
    {
        auto tokens = tokenizeAsciiWithOffsets(text, filter_stop,
                                               config_.min_term_length,
                                               config_.max_term_length,
                                               (config_.features & II_FEATURE_STEMMING) != 0,
                                               config_.language);
        if (tokens.empty())
        {
            return Status::OK;
        }
        for (const auto& token : tokens)
        {
            term_positions[token.term].push_back(token.position);
            if (store_offsets)
            {
                term_offsets[token.term].push_back({token.start_offset, token.end_offset});
            }
            if (store_payloads)
            {
                term_payloads[token.term].push_back({});
            }
        }
        for (const auto& [term, positions] : term_positions)
        {
            term_counts[term] = static_cast<uint32_t>(positions.size());
        }
    }
    else
    {
        auto tokens = tokenizeAscii(text, filter_stop,
                                    config_.min_term_length,
                                    config_.max_term_length,
                                    (config_.features & II_FEATURE_STEMMING) != 0,
                                    config_.language);
        if (tokens.empty())
        {
            return Status::OK;
        }
        for (const auto& token : tokens)
        {
            ++term_counts[token];
        }
    }

    SBInvertedIndexMetaPage meta{};
    Status status = loadMeta(&meta, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint32_t segment_id = meta.ii_active_segment;
    SBInvertedIndexSegmentMeta seg{};
    GPID seg_gpid = 0;
    status = loadSegmentMeta(segment_id, &seg_gpid, &seg, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    for (const auto& [term, count] : term_counts)
    {
        TermDictionaryEntry entry{};
        GPID page_gpid = 0;
        uint16_t entry_index = 0;
        Status find_status = findTerm(segment_id, term, &entry, &page_gpid, &entry_index, ctx);
        if (find_status != Status::OK)
        {
            continue;
        }

        std::vector<TID> tids;
        std::vector<PostingWithPositions> postings;
        bool tid_found = false;
        if (store_positions)
        {
            status = readPostingListWithPositions(segment_id, entry.posting_offset,
                                                 entry.posting_length, &postings, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            auto it = std::find_if(postings.begin(), postings.end(),
                                   [&](const PostingWithPositions& item) {
                                       return item.tid == tid;
                                   });
            if (it == postings.end())
            {
                continue;
            }
            postings.erase(it);
            tid_found = true;
        }
        else
        {
            status = readPostingList(segment_id, entry.posting_offset, entry.posting_length, &tids, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            auto it = std::remove(tids.begin(), tids.end(), tid);
            if (it == tids.end())
            {
                continue;
            }
            tids.erase(it, tids.end());
            sortUnique(tids);
            tid_found = true;
        }

        if (!tid_found)
        {
            continue;
        }

        if (entry.doc_frequency > 0)
        {
            entry.doc_frequency -= 1;
        }
        if (entry.total_frequency >= count)
        {
            entry.total_frequency -= count;
        }
        if ((!store_positions && tids.empty()) ||
            (store_positions && postings.empty()))
        {
            entry.posting_offset = 0;
            entry.posting_length = 0;
        }
        else
        {
            uint64_t new_offset = 0;
            uint32_t new_length = 0;
            if (store_positions)
            {
                status = writePostingListWithPositions(segment_id, postings, &new_offset, &new_length, ctx);
            }
            else
            {
                status = writePostingList(segment_id, tids, &new_offset, &new_length, ctx);
            }
            if (status != Status::OK)
            {
                return status;
            }
            entry.posting_offset = new_offset;
            entry.posting_length = new_length;
        }

        status = updateTermEntry(page_gpid, entry_index, entry, ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    auto doc_it = doc_lengths_.find(tid);
    if (doc_it != doc_lengths_.end())
    {
        uint32_t doc_len = doc_it->second;
        SBInvertedIndexSegmentMeta seg_latest{};
        status = loadSegmentMeta(segment_id, &seg_gpid, &seg_latest, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        if (seg_latest.seg_num_documents > 0)
        {
            seg_latest.seg_num_documents -= 1;
        }
        if (seg_latest.seg_num_tokens >= doc_len)
        {
            seg_latest.seg_num_tokens -= doc_len;
        }
        if (seg_latest.seg_num_documents > 0)
        {
            seg_latest.seg_avg_doc_length =
                static_cast<uint32_t>(seg_latest.seg_num_tokens / seg_latest.seg_num_documents);
        }
        else
        {
            seg_latest.seg_avg_doc_length = 0;
        }
        status = updateSegmentMeta(seg_gpid, seg_latest, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (meta.ii_total_documents > 0)
        {
            meta.ii_total_documents -= 1;
        }
        if (meta.ii_total_tokens >= doc_len)
        {
            meta.ii_total_tokens -= doc_len;
        }
        if (meta.ii_total_documents > 0)
        {
            meta.ii_avg_doc_length =
                static_cast<uint32_t>(meta.ii_total_tokens / meta.ii_total_documents);
        }
        else
        {
            meta.ii_avg_doc_length = 0;
        }
        status = updateMeta(meta, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        status = updateDocStats(segment_id, tid, 0, 0, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        doc_lengths_.erase(doc_it);
    }

    return Status::OK;
}

Status InvertedIndex::search(const std::string& query,
                             uint64_t current_xid,
                             std::vector<TID>* results,
                             ErrorContext* ctx)
{
    (void)current_xid;
    if (!results)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Results vector cannot be null");
        return Status::INVALID_ARGUMENT;
    }

    results->clear();
    SBInvertedIndexMetaPage meta{};
    Status status = loadMeta(&meta, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::vector<uint32_t> segments;
    segments.reserve(meta.ii_num_segments);
    for (uint32_t i = 0; i < meta.ii_num_segments; ++i)
    {
        SBInvertedIndexSegmentMeta seg{};
        GPID seg_gpid = 0;
        status = loadSegmentMeta(i, &seg_gpid, &seg, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        if ((seg.seg_flags & SEG_FLAG_MERGED) != 0)
        {
            continue;
        }
        segments.push_back(i);
    }

    if (doc_lengths_.empty())
    {
        bool clear_existing = true;
        for (uint32_t segment_id : segments)
        {
            status = loadDocStatsMap(segment_id, clear_existing, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            clear_existing = false;
        }
    }

    std::vector<TID> universe;
    universe.reserve(doc_lengths_.size());
    for (const auto& [tid, len] : doc_lengths_)
    {
        (void)len;
        universe.push_back(tid);
    }
    sortUnique(universe);

    std::unordered_map<std::string, uint64_t> df_totals;
    bool store_positions = (config_.features & II_FEATURE_POSITIONS) != 0 ||
        (config_.features & (II_FEATURE_OFFSETS | II_FEATURE_PAYLOADS)) != 0;

    std::optional<TSQuery> tsquery = TSQuery::fromString(query);
    if (!tsquery || !tsquery->isValid())
    {
        bool filter_stop = (config_.features & II_FEATURE_STOP_WORDS) != 0;
        auto terms = tokenizeAscii(query, filter_stop,
                                   config_.min_term_length,
                                   config_.max_term_length,
                                   (config_.features & II_FEATURE_STEMMING) != 0,
                                   config_.language);
        if (terms.empty())
        {
            return Status::OK;
        }
        std::string rebuilt;
        for (size_t i = 0; i < terms.size(); ++i)
        {
            if (i > 0)
            {
                rebuilt.append(" & ");
            }
            rebuilt.append(terms[i]);
        }
        tsquery = TSQuery::fromString(rebuilt);
        if (!tsquery || !tsquery->isValid())
        {
            return Status::OK;
        }
    }

    std::vector<TID> matches;
    for (uint32_t segment_id : segments)
    {
        std::unordered_map<std::string, std::vector<TID>> postings_cache;
        std::unordered_map<std::string, std::vector<PostingWithPositions>> postings_positions_cache;
        std::unordered_map<std::string, uint32_t> df_cache;

        auto fetch_postings = [&](const std::string& term) -> std::vector<TID> {
            auto it = postings_cache.find(term);
            if (it != postings_cache.end())
            {
                return it->second;
            }

            TermDictionaryEntry entry{};
            GPID page_gpid = 0;
            uint16_t entry_index = 0;
            Status find_status = findTerm(segment_id, term, &entry, &page_gpid, &entry_index, ctx);
            if (find_status != Status::OK)
            {
                postings_cache[term] = {};
                df_cache[term] = 0;
                return {};
            }

            std::vector<TID> tids;
            if (store_positions)
            {
                std::vector<PostingWithPositions> postings;
                Status list_status = readPostingListWithPositions(segment_id, entry.posting_offset,
                                                                  entry.posting_length, &postings, ctx);
                if (list_status != Status::OK)
                {
                    postings_cache[term] = {};
                    df_cache[term] = 0;
                    return {};
                }
                tids.reserve(postings.size());
                for (const auto& item : postings)
                {
                    tids.push_back(item.tid);
                }
                postings_positions_cache[term] = std::move(postings);
            }
            else
            {
                Status list_status = readPostingList(segment_id, entry.posting_offset, entry.posting_length, &tids, ctx);
                if (list_status != Status::OK)
                {
                    postings_cache[term] = {};
                    df_cache[term] = 0;
                    return {};
                }
            }
            sortUnique(tids);
            postings_cache[term] = tids;
            df_cache[term] = entry.doc_frequency;
            df_totals[term] += entry.doc_frequency;
            return tids;
        };

        auto fetch_postings_positions = [&](const std::string& term) -> std::vector<PostingWithPositions> {
            auto it = postings_positions_cache.find(term);
            if (it != postings_positions_cache.end())
            {
                return it->second;
            }

            TermDictionaryEntry entry{};
            GPID page_gpid = 0;
            uint16_t entry_index = 0;
            Status find_status = findTerm(segment_id, term, &entry, &page_gpid, &entry_index, ctx);
            if (find_status != Status::OK)
            {
                postings_positions_cache[term] = {};
                df_cache[term] = 0;
                return {};
            }

            std::vector<PostingWithPositions> postings;
            Status list_status = readPostingListWithPositions(segment_id, entry.posting_offset,
                                                              entry.posting_length, &postings, ctx);
            if (list_status != Status::OK)
            {
                postings_positions_cache[term] = {};
                df_cache[term] = 0;
                return {};
            }
            std::vector<TID> tids;
            tids.reserve(postings.size());
            for (const auto& item : postings)
            {
                tids.push_back(item.tid);
            }
            sortUnique(tids);
            postings_cache[term] = std::move(tids);
            postings_positions_cache[term] = postings;
            df_cache[term] = entry.doc_frequency;
            df_totals[term] += entry.doc_frequency;
            return postings;
        };

        std::function<std::vector<TID>(const TSQueryNode*)> eval_node;
        eval_node = [&](const TSQueryNode* node) -> std::vector<TID> {
            if (!node)
            {
                return {};
            }
            switch (node->type())
            {
                case TSQueryNode::Type::LEXEME:
                {
                    std::string term = normalizeTerm(node->term(), config_);
                    if (term.empty())
                    {
                        return {};
                    }
                    return fetch_postings(term);
                }
                case TSQueryNode::Type::AND:
                {
                    auto left = eval_node(node->left());
                    auto right = eval_node(node->right());
                    return intersectTids(left, right);
                }
                case TSQueryNode::Type::OR:
                {
                    auto left = eval_node(node->left());
                    auto right = eval_node(node->right());
                    return unionTids(left, right);
                }
                case TSQueryNode::Type::PHRASE:
                {
                    if (!store_positions)
                    {
                        auto left = eval_node(node->left());
                        auto right = eval_node(node->right());
                        return intersectTids(left, right);
                    }
                    if (!node->left() || !node->right())
                    {
                        return {};
                    }
                    if (node->left()->type() != TSQueryNode::Type::LEXEME ||
                        node->right()->type() != TSQueryNode::Type::LEXEME)
                    {
                        return {};
                    }
                    std::string left_term = normalizeTerm(node->left()->term(), config_);
                    std::string right_term = normalizeTerm(node->right()->term(), config_);
                    if (left_term.empty() || right_term.empty())
                    {
                        return {};
                    }
                    auto left_postings = fetch_postings_positions(left_term);
                    auto right_postings = fetch_postings_positions(right_term);
                    if (left_postings.empty() || right_postings.empty())
                    {
                        return {};
                    }
                    std::map<TID, const std::vector<uint32_t>*> right_map;
                    for (const auto& entry : right_postings)
                    {
                        right_map[entry.tid] = &entry.positions;
                    }
                    std::vector<TID> result;
                    result.reserve(left_postings.size());
                    uint16_t distance = node->distance();
                    for (const auto& left_entry : left_postings)
                    {
                        auto it = right_map.find(left_entry.tid);
                        if (it == right_map.end())
                        {
                            continue;
                        }
                        const auto& left_positions = left_entry.positions;
                        const auto& right_positions = *it->second;
                        bool matched = false;
                        for (uint32_t pos1 : left_positions)
                        {
                            for (uint32_t pos2 : right_positions)
                            {
                                uint32_t diff = pos1 > pos2 ? pos1 - pos2 : pos2 - pos1;
                                if (diff > 0 && diff <= distance)
                                {
                                    matched = true;
                                    break;
                                }
                            }
                            if (matched)
                            {
                                break;
                            }
                        }
                        if (matched)
                        {
                            result.push_back(left_entry.tid);
                        }
                    }
                    return result;
                }
                case TSQueryNode::Type::NOT:
                {
                    auto child = eval_node(node->left());
                    return subtractTids(universe, child);
                }
                default:
                    return {};
            }
        };

        auto segment_matches = eval_node(tsquery->root());
        if (!segment_matches.empty())
        {
            matches.insert(matches.end(), segment_matches.begin(), segment_matches.end());
        }
    }
    if (matches.empty())
    {
        return Status::OK;
    }
    sortUnique(matches);

    std::vector<std::string> scoring_terms;
    {
        std::unordered_map<std::string, bool> seen;
        std::function<void(const TSQueryNode*)> collect_terms;
        collect_terms = [&](const TSQueryNode* node) {
            if (!node)
            {
                return;
            }
            if (node->type() == TSQueryNode::Type::LEXEME)
            {
                std::string term = normalizeTerm(node->term(), config_);
                if (term.empty())
                {
                    return;
                }
                if (!seen[term])
                {
                    seen[term] = true;
                    scoring_terms.push_back(term);
                }
                return;
            }
            collect_terms(node->left());
            collect_terms(node->right());
        };
        collect_terms(tsquery->root());
    }

    double N = meta.ii_total_documents > 0 ? static_cast<double>(meta.ii_total_documents) : 1.0;
    double avgdl = meta.ii_avg_doc_length > 0 ? static_cast<double>(meta.ii_avg_doc_length) : 1.0;
    constexpr double k1 = 1.2;
    constexpr double b = 0.75;

    struct ScoredResult
    {
        TID tid;
        double score;
    };
    std::vector<ScoredResult> scored;
    scored.reserve(matches.size());

    for (const auto& tid : matches)
    {
        double dl = avgdl;
        auto it = doc_lengths_.find(tid);
        if (it != doc_lengths_.end())
        {
            dl = static_cast<double>(it->second);
        }

        double score = 0.0;
        for (const auto& term : scoring_terms)
        {
            double df = 1.0;
            auto df_it = df_totals.find(term);
            if (df_it != df_totals.end() && df_it->second > 0)
            {
                df = static_cast<double>(df_it->second);
            }
            double idf = std::log((N - df + 0.5) / (df + 0.5) + 1.0);
            double tf = 1.0;
            double denom = tf + k1 * (1.0 - b + b * (dl / avgdl));
            score += idf * ((k1 + 1.0) * tf / denom);
        }
        scored.push_back({tid, score});
    }

    std::sort(scored.begin(), scored.end(),
              [](const ScoredResult& a, const ScoredResult& b) {
                  if (a.score != b.score)
                  {
                      return a.score > b.score;
                  }
                  return a.tid < b.tid;
              });

    results->reserve(scored.size());
    for (const auto& item : scored)
    {
        results->push_back(item.tid);
    }

    return Status::OK;
}

Status InvertedIndex::removeDeadEntries(const std::vector<TID>& dead_tids,
                                        uint64_t* entries_removed_out,
                                        uint64_t* pages_modified_out,
                                        ErrorContext* ctx)
{
    if (entries_removed_out)
    {
        *entries_removed_out = 0;
    }
    if (pages_modified_out)
    {
        *pages_modified_out = 0;
    }
    if (dead_tids.empty())
    {
        return Status::OK;
    }

    struct TIDHash
    {
        size_t operator()(const TID& tid) const
        {
            uint64_t h1 = std::hash<uint64_t>{}(tid.gpid);
            uint64_t h2 = std::hash<uint16_t>{}(tid.slot);
            return static_cast<size_t>(h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2)));
        }
    };

    std::unordered_set<TID, TIDHash> dead_set;
    dead_set.reserve(dead_tids.size());
    for (const auto& tid : dead_tids)
    {
        dead_set.insert(tid);
    }

    SBInvertedIndexMetaPage meta{};
    Status status = loadMeta(&meta, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::vector<uint32_t> segments;
    segments.reserve(meta.ii_num_segments);
    for (uint32_t i = 0; i < meta.ii_num_segments; ++i)
    {
        SBInvertedIndexSegmentMeta seg{};
        GPID seg_gpid = 0;
        status = loadSegmentMeta(i, &seg_gpid, &seg, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        if ((seg.seg_flags & SEG_FLAG_MERGED) != 0)
        {
            continue;
        }
        segments.push_back(i);
    }

    if (doc_lengths_.empty())
    {
        bool clear_existing = true;
        for (uint32_t segment_id : segments)
        {
            status = loadDocStatsMap(segment_id, clear_existing, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            clear_existing = false;
        }
    }

    uint64_t total_removed = 0;
    uint64_t pages_modified = 0;
    uint64_t removed_docs = 0;
    uint64_t removed_tokens = 0;

    BufferPool* buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    bool store_positions = (config_.features & II_FEATURE_POSITIONS) != 0;

    for (uint32_t segment_id : segments)
    {
        SBInvertedIndexSegmentMeta seg{};
        GPID seg_gpid = 0;
        status = loadSegmentMeta(segment_id, &seg_gpid, &seg, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        if (seg.seg_dict_first_page == 0)
        {
            continue;
        }

        std::unordered_set<TID, TIDHash> segment_removed;

        GPID current = seg.seg_dict_first_page;
        while (current != 0)
        {
            uint8_t* page_data = nullptr;
            status = buffer_pool->pinPageGlobal(current, reinterpret_cast<void**>(&page_data), ctx);
            if (status != Status::OK)
            {
                return status;
            }
            auto* page = reinterpret_cast<SBTermDictionaryPage*>(page_data);
            auto* entries = reinterpret_cast<TermDictionaryEntry*>(page->dict_entries);
            uint16_t count = page->dict_num_entries;
            bool page_dirty = false;

            for (uint16_t i = 0; i < count; ++i)
            {
                TermDictionaryEntry entry = entries[i];
                if (entry.posting_length == 0 || entry.doc_frequency == 0)
                {
                    continue;
                }

                uint64_t removed_here = 0;
                uint64_t new_total_freq = 0;
                uint64_t new_offset = 0;
                uint32_t new_length = 0;

                if (store_positions)
                {
                    std::vector<PostingWithPositions> postings;
                    status = readPostingListWithPositions(segment_id, entry.posting_offset,
                                                          entry.posting_length, &postings, ctx);
                    if (status != Status::OK)
                    {
                        buffer_pool->unpinPageGlobal(current, false, ctx);
                        return status;
                    }
                    std::vector<PostingWithPositions> filtered;
                    filtered.reserve(postings.size());
                    for (const auto& post : postings)
                    {
                        if (dead_set.find(post.tid) != dead_set.end())
                        {
                            removed_here += 1;
                            segment_removed.insert(post.tid);
                            continue;
                        }
                        new_total_freq += post.positions.size();
                        filtered.push_back(post);
                    }

                    if (removed_here > 0)
                    {
                        if (!filtered.empty())
                        {
                            status = writePostingListWithPositions(segment_id, filtered,
                                                                  &new_offset, &new_length, ctx);
                            if (status != Status::OK)
                            {
                                buffer_pool->unpinPageGlobal(current, false, ctx);
                                return status;
                            }
                            entry.posting_offset = new_offset;
                            entry.posting_length = new_length;
                            entry.doc_frequency = static_cast<uint32_t>(filtered.size());
                            entry.total_frequency = new_total_freq;
                        }
                        else
                        {
                            entry.doc_frequency = 0;
                            entry.total_frequency = 0;
                            entry.posting_offset = 0;
                            entry.posting_length = 0;
                        }
                        entries[i] = entry;
                        page_dirty = true;
                    }
                }
                else
                {
                    std::vector<TID> tids;
                    status = readPostingList(segment_id, entry.posting_offset,
                                             entry.posting_length, &tids, ctx);
                    if (status != Status::OK)
                    {
                        buffer_pool->unpinPageGlobal(current, false, ctx);
                        return status;
                    }
                    std::vector<TID> filtered;
                    filtered.reserve(tids.size());
                    for (const auto& tid : tids)
                    {
                        if (dead_set.find(tid) != dead_set.end())
                        {
                            removed_here += 1;
                            segment_removed.insert(tid);
                            continue;
                        }
                        filtered.push_back(tid);
                    }

                    if (removed_here > 0)
                    {
                        if (!filtered.empty())
                        {
                            status = writePostingList(segment_id, filtered, &new_offset, &new_length, ctx);
                            if (status != Status::OK)
                            {
                                buffer_pool->unpinPageGlobal(current, false, ctx);
                                return status;
                            }
                            entry.posting_offset = new_offset;
                            entry.posting_length = new_length;
                            entry.doc_frequency = static_cast<uint32_t>(filtered.size());
                            if (entry.total_frequency >= removed_here)
                            {
                                entry.total_frequency -= removed_here;
                            }
                            else
                            {
                                entry.total_frequency = 0;
                            }
                        }
                        else
                        {
                            entry.doc_frequency = 0;
                            entry.total_frequency = 0;
                            entry.posting_offset = 0;
                            entry.posting_length = 0;
                        }
                        entries[i] = entry;
                        page_dirty = true;
                    }
                }

                if (removed_here > 0)
                {
                    total_removed += removed_here;
                    pages_modified += 1;
                }
            }

            GPID next = page->dict_next_page;
            buffer_pool->unpinPageGlobal(current, page_dirty, ctx);
            current = next;
        }

        for (const auto& tid : segment_removed)
        {
            status = updateDocStats(segment_id, tid, 0, 0, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }
    }

    for (const auto& tid : dead_set)
    {
        auto it = doc_lengths_.find(tid);
        if (it != doc_lengths_.end())
        {
            removed_docs += 1;
            removed_tokens += it->second;
            doc_lengths_.erase(it);
        }
    }

    if (removed_docs > 0)
    {
        if (meta.ii_total_documents >= removed_docs)
        {
            meta.ii_total_documents -= removed_docs;
        }
        else
        {
            meta.ii_total_documents = 0;
        }

        if (meta.ii_total_tokens >= removed_tokens)
        {
            meta.ii_total_tokens -= removed_tokens;
        }
        else
        {
            meta.ii_total_tokens = 0;
        }

        if (meta.ii_total_documents > 0)
        {
            meta.ii_avg_doc_length =
                static_cast<uint32_t>(meta.ii_total_tokens / meta.ii_total_documents);
        }
        else
        {
            meta.ii_avg_doc_length = 0;
        }

        status = updateMeta(meta, ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    if (entries_removed_out)
    {
        *entries_removed_out = total_removed;
    }
    if (pages_modified_out)
    {
        *pages_modified_out = pages_modified;
    }

    return Status::OK;
}

} // namespace scratchbird::core
