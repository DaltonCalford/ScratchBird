/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/gpid.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <map>

namespace scratchbird::core
{

class Database;

constexpr uint8_t II_FEATURE_POSITIONS = 0x01;
constexpr uint8_t II_FEATURE_OFFSETS = 0x02;
constexpr uint8_t II_FEATURE_PAYLOADS = 0x04;
constexpr uint8_t II_FEATURE_STEMMING = 0x08;
constexpr uint8_t II_FEATURE_STOP_WORDS = 0x10;

constexpr uint8_t II_COMPRESSION_NONE = 0;
constexpr uint8_t II_COMPRESSION_VBYTE = 1;
constexpr uint8_t II_COMPRESSION_PFORDELTA = 2;

constexpr uint8_t SEG_FLAG_ACTIVE = 0x01;
constexpr uint8_t SEG_FLAG_MERGED = 0x02;
constexpr uint8_t SEG_FLAG_COMPACTING = 0x04;

struct InvertedIndexConfig
{
    uint16_t language = 0;
    uint8_t features = 0;
    uint8_t compression_type = II_COMPRESSION_VBYTE;
    uint16_t min_term_length = 2;
    uint16_t max_term_length = 40;
};

#pragma pack(push, 1)

struct SBInvertedIndexMetaPage
{
    PageHeader ii_header;
    uint8_t ii_index_uuid[16];
    uint8_t ii_table_uuid[16];
    uint8_t ii_column_uuid[16];
    uint16_t ii_language;
    uint32_t ii_num_segments;
    uint32_t ii_active_segment;
    uint64_t ii_total_documents;
    uint64_t ii_total_terms;
    uint64_t ii_total_tokens;
    uint32_t ii_avg_doc_length;
    uint8_t ii_features;
    uint8_t ii_compression_type;
    uint8_t ii_reserved1[30];
    uint64_t ii_segment_pages[256];
    uint64_t ii_total_queries;
    uint64_t ii_avg_query_time_us;
    uint64_t ii_last_merge_time;
    uint64_t ii_reserved2;
    uint8_t ii_padding[];
} __attribute__((packed));

struct SBInvertedIndexSegmentMeta
{
    PageHeader seg_header;
    uint32_t seg_id;
    uint64_t seg_num_documents;
    uint64_t seg_num_terms;
    uint64_t seg_num_tokens;
    uint32_t seg_avg_doc_length;
    uint64_t seg_created_at;
    uint64_t seg_merged_at;
    uint8_t seg_flags;
    uint8_t seg_reserved1[23];
    uint64_t seg_dict_first_page;
    uint64_t seg_dict_num_pages;
    uint64_t seg_posting_first_page;
    uint64_t seg_posting_num_pages;
    uint64_t seg_docstats_page;
    uint64_t seg_delete_bitmap_page;
    uint64_t seg_total_posting_bytes;
    uint64_t seg_reserved2;
    uint8_t seg_padding[];
} __attribute__((packed));

struct TermDictionaryEntry
{
    char term[64];
    uint32_t term_hash;
    uint32_t doc_frequency;
    uint64_t total_frequency;
    uint64_t posting_offset;
    uint32_t posting_length;
    uint32_t reserved;
    uint64_t reserved2[4];
} __attribute__((packed));

static_assert(sizeof(TermDictionaryEntry) == 128, "TermDictionaryEntry must be 128 bytes");

struct SBTermDictionaryPage
{
    PageHeader dict_header;
    uint64_t dict_next_page;
    uint16_t dict_num_entries;
    uint16_t dict_reserved;
    uint64_t dict_first_term_hash;
    uint8_t dict_entries[];
} __attribute__((packed));

struct SBPostingListPage
{
    PageHeader post_header;
    uint64_t post_next_page;
    uint32_t post_data_length;
    uint8_t post_compression_type;
    uint8_t post_reserved[7];
    uint8_t post_data[];
} __attribute__((packed));

struct DocumentStats
{
    uint32_t doc_id;
    uint32_t doc_length;
    uint32_t num_unique_terms;
    uint32_t reserved;
} __attribute__((packed));

static_assert(sizeof(DocumentStats) == 16, "DocumentStats must be 16 bytes");

struct InvertedDocStatsEntry
{
    GPID gpid;
    uint16_t slot;
    uint16_t reserved;
    uint32_t doc_length;
    uint32_t num_unique_terms;
} __attribute__((packed));

static_assert(sizeof(InvertedDocStatsEntry) == 20, "InvertedDocStatsEntry must be 20 bytes");

struct SBDocumentStatsPage
{
    PageHeader docstats_header;
    uint64_t docstats_next_page;
    uint32_t docstats_num_entries;
    uint64_t docstats_reserved;
    uint8_t docstats_data[];
} __attribute__((packed));

#pragma pack(pop)

inline uint32_t maxTermsPerPage(uint32_t page_size)
{
    constexpr uint32_t DICT_PAGE_HEADER_SIZE = sizeof(PageHeader) + 8 + 2 + 2 + 8;
    constexpr uint32_t TERM_ENTRY_SIZE = sizeof(TermDictionaryEntry);
    if (page_size <= DICT_PAGE_HEADER_SIZE)
    {
        return 0;
    }
    uint32_t available_bytes = page_size - DICT_PAGE_HEADER_SIZE;
    return available_bytes / TERM_ENTRY_SIZE;
}

inline uint32_t maxDocStatsPerPage(uint32_t page_size)
{
    constexpr uint32_t STATS_PAGE_HEADER_SIZE = sizeof(PageHeader) + 8 + 4 + 8;
    constexpr uint32_t DOC_STATS_SIZE = sizeof(InvertedDocStatsEntry);
    if (page_size <= STATS_PAGE_HEADER_SIZE)
    {
        return 0;
    }
    uint32_t available_bytes = page_size - STATS_PAGE_HEADER_SIZE;
    return available_bytes / DOC_STATS_SIZE;
}

class InvertedIndex : public IndexGCInterface
{
public:
    InvertedIndex(Database* db,
                  const ID& index_uuid,
                  const ID& table_uuid,
                  const ID& column_uuid,
                  GPID meta_gpid,
                  InvertedIndexConfig config);

    static Status create(Database* db,
                         const ID& index_uuid,
                         const ID& table_uuid,
                         const ID& column_uuid,
                         GPID meta_gpid,
                         const InvertedIndexConfig& config,
                         ErrorContext* ctx = nullptr);

    static std::unique_ptr<InvertedIndex> open(Database* db,
                                               const ID& index_uuid,
                                               const ID& table_uuid,
                                               const ID& column_uuid,
                                               GPID meta_gpid,
                                               ErrorContext* ctx = nullptr);

    Status insert(const void* document_data,
                  size_t document_len,
                  const TID& tid,
                  ErrorContext* ctx = nullptr);

    Status remove(const void* document_data,
                  size_t document_len,
                  const TID& tid,
                  uint64_t current_xid,
                  ErrorContext* ctx = nullptr);

    Status search(const std::string& query,
                  uint64_t current_xid,
                  std::vector<TID>* results,
                  ErrorContext* ctx = nullptr);

    Status removeDeadEntries(const std::vector<TID>& dead_tids,
                             uint64_t* entries_removed_out = nullptr,
                             uint64_t* pages_modified_out = nullptr,
                             ErrorContext* ctx = nullptr) override;

    const char* indexTypeName() const override { return "Inverted"; }

private:
    struct PostingWithPositions
    {
        TID tid;
        std::vector<uint32_t> positions;
        std::vector<std::pair<uint32_t, uint32_t>> offsets;
        std::vector<std::vector<uint8_t>> payloads;
    };

    uint32_t hashTerm(std::string_view term) const;
    Status loadMeta(SBInvertedIndexMetaPage* meta_out, ErrorContext* ctx) const;
    Status loadSegmentMeta(uint32_t segment_id,
                           GPID* seg_gpid_out,
                           SBInvertedIndexSegmentMeta* seg_out,
                           ErrorContext* ctx) const;
    Status updateSegmentMeta(GPID seg_gpid,
                             const SBInvertedIndexSegmentMeta& seg,
                             ErrorContext* ctx) const;
    Status findTerm(uint32_t segment_id,
                    std::string_view term,
                    TermDictionaryEntry* entry_out,
                    GPID* page_gpid_out,
                    uint16_t* entry_index_out,
                    ErrorContext* ctx) const;
    Status insertTerm(uint32_t segment_id,
                      const TermDictionaryEntry& entry,
                      GPID* page_gpid_out,
                      uint16_t* entry_index_out,
                      ErrorContext* ctx);
    Status writePostingList(uint32_t segment_id,
                            const std::vector<TID>& tids,
                            uint64_t* posting_offset_out,
                            uint32_t* posting_length_out,
                            ErrorContext* ctx);
    Status writePostingListWithPositions(uint32_t segment_id,
                                         const std::vector<PostingWithPositions>& postings,
                                         uint64_t* posting_offset_out,
                                         uint32_t* posting_length_out,
                                         ErrorContext* ctx);
    Status readPostingList(uint32_t segment_id,
                           uint64_t posting_offset,
                           uint32_t posting_length,
                           std::vector<TID>* tids_out,
                           ErrorContext* ctx) const;
    Status readPostingListWithPositions(uint32_t segment_id,
                                        uint64_t posting_offset,
                                        uint32_t posting_length,
                                        std::vector<PostingWithPositions>* postings_out,
                                        ErrorContext* ctx) const;
    Status updateTermEntry(GPID page_gpid,
                           uint16_t entry_index,
                           const TermDictionaryEntry& entry,
                           ErrorContext* ctx) const;
    Status updateMeta(const SBInvertedIndexMetaPage& meta, ErrorContext* ctx) const;
    Status appendDocStats(uint32_t segment_id,
                          const TID& tid,
                          uint32_t doc_length,
                          uint32_t unique_terms,
                          ErrorContext* ctx);
    Status updateDocStats(uint32_t segment_id,
                          const TID& tid,
                          uint32_t doc_length,
                          uint32_t unique_terms,
                          ErrorContext* ctx);
    Status loadDocStatsMap(uint32_t segment_id, bool clear_existing, ErrorContext* ctx);
    Status loadAllTerms(uint32_t segment_id,
                        std::vector<std::pair<std::string, TermDictionaryEntry>>* terms_out,
                        ErrorContext* ctx) const;
    Status createSegment(uint32_t* segment_id_out, GPID* seg_gpid_out, ErrorContext* ctx);
    Status maybeRotateSegment(SBInvertedIndexMetaPage* meta, ErrorContext* ctx);
    Status maybeMergeSegments(SBInvertedIndexMetaPage* meta, ErrorContext* ctx);
    Status mergeSegments(const std::vector<uint32_t>& segment_ids, ErrorContext* ctx);

    Database* db_ = nullptr;
    ID index_uuid_{};
    ID table_uuid_{};
    ID column_uuid_{};
    GPID meta_gpid_ = 0;
    uint16_t tablespace_id_ = 0;
    InvertedIndexConfig config_{};
    std::map<TID, uint32_t> doc_lengths_;
};

} // namespace scratchbird::core
