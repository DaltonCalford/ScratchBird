#include "scratchbird/core/clog.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/error_context.h"
#include <cstring>

namespace scratchbird::core
{

    // ============================================================================
    // CLOG TRANSACTION STATE SIZE CONSTRAINT
    // ============================================================================
    // The CLOG uses 2 bits per transaction to store status, which allows exactly
    // 4 possible values (2^2 = 4). The ClogStatus enum MUST NOT exceed 4 values.
    //
    // These static assertions enforce this constraint at compile time to prevent
    // forward compatibility issues if someone attempts to add a 5th state.
    //
    // If you need more than 4 transaction states, you MUST:
    // 1. Change BITS_PER_XID from 2 to 3 (allows 8 states)
    // 2. Update setStatusBits() and getStatusBits() to use 3 bits
    // 3. Update getXidsPerPage() calculation to use 3 bits instead of 2
    // 4. Implement database version migration for existing CLOG pages
    // ============================================================================

    // Verify ClogStatus has exactly 4 values
    static_assert(static_cast<uint8_t>(ClogStatus::IN_PROGRESS) == 0,
                  "ClogStatus::IN_PROGRESS must be 0 to fit in 2-bit storage");
    static_assert(static_cast<uint8_t>(ClogStatus::COMMITTED) == 1,
                  "ClogStatus::COMMITTED must be 1 to fit in 2-bit storage");
    static_assert(static_cast<uint8_t>(ClogStatus::ABORTED) == 2,
                  "ClogStatus::ABORTED must be 2 to fit in 2-bit storage");
    static_assert(static_cast<uint8_t>(ClogStatus::SUB_COMMITTED) == 3,
                  "ClogStatus::SUB_COMMITTED must be 3 to fit in 2-bit storage");

    // Ensure no enum value exceeds 3 (maximum value for 2 bits)
    static_assert(static_cast<uint8_t>(ClogStatus::IN_PROGRESS) <= 3,
                  "ClogStatus values must fit in 2 bits (0-3)");
    static_assert(static_cast<uint8_t>(ClogStatus::COMMITTED) <= 3,
                  "ClogStatus values must fit in 2 bits (0-3)");
    static_assert(static_cast<uint8_t>(ClogStatus::ABORTED) <= 3,
                  "ClogStatus values must fit in 2 bits (0-3)");
    static_assert(static_cast<uint8_t>(ClogStatus::SUB_COMMITTED) <= 3,
                  "ClogStatus values must fit in 2 bits (0-3)");

    // ============================================================================

    Clog::Clog(Database *db) : db_(db), buffer_pool_(db->buffer_pool()), clog_root_page_(0) {}

    Clog::~Clog() = default;

    auto Clog::initialize(ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Allocate first CLOG page
        Status status = db_->page_manager()->allocatePage(clog_root_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Initialize the first CLOG page
        status = allocateClogPage(clog_root_page_, 0, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        return Status::OK;
    }

    auto Clog::setStatus(uint64_t xid, ClogStatus status, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Calculate which page and offset
        uint32_t page_id = getPageForXid(xid);
        uint32_t offset = getOffsetInPage(xid);

        // Pin the CLOG page
        void *page_buffer;
        Status pin_status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
        if (pin_status == Status::IO_ERROR)
        {
            // Page doesn't exist, need to extend CLOG
            Status extend_status = extendClog(xid, ctx);
            if (extend_status != Status::OK)
            {
                return extend_status;
            }
            // Try again
            pin_status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
        }

        if (pin_status != Status::OK)
        {
            return pin_status;
        }

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        auto *header = reinterpret_cast<ClogPageHeader *>(page_data);

        // Status data starts after header
        uint8_t *status_data = page_data + sizeof(ClogPageHeader);

        // Set the 2-bit status
        setStatusBits(status_data, offset, status);

        // Unpin with dirty flag
        buffer_pool_->unpinPage(page_id, true, ctx);

        return Status::OK;
    }

    auto Clog::getStatus(uint64_t xid, ClogStatus *status_out, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Calculate which page and offset
        uint32_t page_id = getPageForXid(xid);
        uint32_t offset = getOffsetInPage(xid);

        // Pin the CLOG page
        void *page_buffer;
        Status status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            // Page doesn't exist - XID not in CLOG yet
            if (status_out != nullptr)
            {
                *status_out = ClogStatus::IN_PROGRESS;
            }
            return Status::NOT_FOUND;
        }

        auto *page_data = static_cast<uint8_t *>(page_buffer);

        // Status data starts after header
        const uint8_t *status_data = page_data + sizeof(ClogPageHeader);

        // Get the 2-bit status
        ClogStatus clog_status = getStatusBits(status_data, offset);

        if (status_out != nullptr)
        {
            *status_out = clog_status;
        }

        // Unpin page
        buffer_pool_->unpinPage(page_id, false, ctx);

        return Status::OK;
    }

    auto Clog::extendClog(uint64_t xid, ErrorContext *ctx) -> Status
    {
        // Calculate how many pages we need
        uint32_t required_page = getPageForXid(xid);
        uint32_t current_last_page = clog_root_page_;

        // Find the current last CLOG page
        void *page_buffer;
        Status status = buffer_pool_->pinPage(current_last_page, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        auto *header = reinterpret_cast<ClogPageHeader *>(page_data);

        // Follow the chain to find the last page
        while (header->next_clog_page != 0)
        {
            uint32_t next_page = header->next_clog_page;
            buffer_pool_->unpinPage(current_last_page, false, ctx);
            current_last_page = next_page;

            status = buffer_pool_->pinPage(current_last_page, &page_buffer, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            page_data = static_cast<uint8_t *>(page_buffer);
            header = reinterpret_cast<ClogPageHeader *>(page_data);
        }

        // Now current_last_page is the last page in the chain
        // We need to add pages until we reach required_page
        uint32_t next_page_num = current_last_page + 1;

        while (next_page_num <= required_page)
        {
            // Allocate new page
            uint32_t new_page_id;
            status = db_->page_manager()->allocatePage(new_page_id, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(current_last_page, false, ctx);
                return status;
            }

            // Initialize the new CLOG page
            uint64_t base_xid =
                static_cast<uint64_t>(next_page_num - clog_root_page_) * getXidsPerPage();
            status = allocateClogPage(new_page_id, base_xid, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(current_last_page, false, ctx);
                return status;
            }

            // Link previous page to this new page
            header->next_clog_page = new_page_id;
            buffer_pool_->unpinPage(current_last_page, true, ctx);

            // Move to the new page
            current_last_page = new_page_id;
            status = buffer_pool_->pinPage(current_last_page, &page_buffer, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            page_data = static_cast<uint8_t *>(page_buffer);
            header = reinterpret_cast<ClogPageHeader *>(page_data);

            next_page_num++;
        }

        buffer_pool_->unpinPage(current_last_page, false, ctx);
        return Status::OK;
    }

    auto Clog::allocateClogPage(uint32_t page_id, uint64_t base_xid, ErrorContext *ctx) -> Status
    {
        // Pin the new page
        void *page_buffer;
        Status status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page_data = static_cast<uint8_t *>(page_buffer);

        // Initialize page to zeros (all statuses = IN_PROGRESS)
        memset(page_data, 0, db_->page_size());

        // Set up page header
        auto *header = reinterpret_cast<ClogPageHeader *>(page_data);
        header->page_header.magic = K_MAGIC_SBRD;
        header->page_header.version = 1;
        header->page_header.page_type = PAGE_TYPE_CLOG;
        header->page_header.page_size = db_->page_size();
        header->page_header.page_id = page_id;
        header->page_header.lsn = 0;
        header->page_header.flags = 0;
        memcpy(header->page_header.database_uuid, db_->uuid().bytes.data(), 16);
        header->page_header.generation = 1;
        header->page_header.free_space = 0;
        header->page_header.item_count = 0;
        header->page_header.free_offset = sizeof(ClogPageHeader);
        header->page_header.special_size = 0;

        header->base_xid = base_xid;
        header->next_clog_page = 0;
        header->reserved = 0;

        // Calculate checksum
        header->page_header.checksum = calculatePageChecksum(page_data, db_->page_size());

        // Unpin with dirty flag
        buffer_pool_->unpinPage(page_id, true, ctx);

        return Status::OK;
    }

    void Clog::setStatusBits(uint8_t *data, uint32_t offset, ClogStatus status)
    {
        // Each byte holds 4 transaction statuses (2 bits each)
        // offset is the transaction number within the page (0-65535)

        uint32_t byte_offset = offset / 4;      // Which byte
        uint32_t bit_offset = (offset % 4) * 2; // Which 2-bit slot (0, 2, 4, or 6)

        // Clear the 2 bits
        data[byte_offset] &= ~(0x3 << bit_offset);

        // Set the 2 bits
        data[byte_offset] |= (static_cast<uint8_t>(status) << bit_offset);
    }

    auto Clog::getStatusBits(const uint8_t *data, uint32_t offset) const -> ClogStatus
    {
        // Each byte holds 4 transaction statuses (2 bits each)
        uint32_t byte_offset = offset / 4;
        uint32_t bit_offset = (offset % 4) * 2;

        // Extract the 2 bits
        uint8_t bits = (data[byte_offset] >> bit_offset) & 0x3;

        return static_cast<ClogStatus>(bits);
    }

    void Clog::getStatistics(ClogStats *stats_out) const
    {
        if (stats_out == nullptr)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // Count CLOG pages
        uint32_t num_pages = 0;
        uint32_t current_page = clog_root_page_;

        while (current_page != 0)
        {
            num_pages++;

            // Try to get next page
            void *page_buffer;
            Status status = buffer_pool_->pinPage(current_page, &page_buffer, nullptr);
            if (status != Status::OK)
            {
                break;
            }

            auto *page_data = static_cast<uint8_t *>(page_buffer);
            auto *header = reinterpret_cast<ClogPageHeader *>(page_data);
            uint32_t next_page = header->next_clog_page;

            buffer_pool_->unpinPage(current_page, false, nullptr);

            if (next_page == 0)
            {
                break;
            }
            current_page = next_page;
        }

        stats_out->num_pages = num_pages;
        stats_out->total_transactions = static_cast<uint64_t>(num_pages) * getXidsPerPage();
        stats_out->space_used_bytes = static_cast<uint64_t>(num_pages) * db_->page_size();

        // Calculate space saved vs TIP (20 bytes per transaction)
        uint64_t tip_space = stats_out->total_transactions * 20;
        stats_out->space_saved_bytes = (tip_space > stats_out->space_used_bytes)
                                           ? (tip_space - stats_out->space_used_bytes)
                                           : 0;
    }

} // namespace scratchbird::core
