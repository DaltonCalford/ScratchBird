#pragma once

namespace scratchbird::core
{
    enum class OversizedValueFamily
    {
        heap_toast,
        hash_overflow
    };

    enum class OversizedValueActionKind
    {
        retain_existing_payload,
        defer_old_payload_cleanup,
        cleanup_old_payload_now,
        allocate_overflow_page,
        compact_overflow_chain,
        unlink_empty_overflow_page,
        reject_unclassified_path
    };

    struct OversizedValueLifecycleInput
    {
        OversizedValueFamily family = OversizedValueFamily::heap_toast;
        bool savepoint_visible = false;
        bool old_payload_still_referenced = false;
        bool overflow_allocation_requested = false;
        bool overflow_page_empty = false;
        bool compaction_required = false;
        bool rewrite_publication_complete = false;
    };

    struct OversizedValueLifecycleDecision
    {
        OversizedValueActionKind action = OversizedValueActionKind::reject_unclassified_path;
        bool allow_space_reuse = false;
        const char *reason_code = "OVERSIZED_REJECT_UNCLASSIFIED_PATH";
    };

    [[nodiscard]] auto classifyOversizedValueLifecycle(
        const OversizedValueLifecycleInput &input) -> OversizedValueLifecycleDecision;
}
