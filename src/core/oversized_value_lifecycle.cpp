#include "scratchbird/core/oversized_value_lifecycle.h"

namespace scratchbird::core
{
    auto classifyOversizedValueLifecycle(
        const OversizedValueLifecycleInput &input) -> OversizedValueLifecycleDecision
    {
        switch (input.family)
        {
            case OversizedValueFamily::heap_toast:
                if (input.old_payload_still_referenced || input.savepoint_visible)
                {
                    return {OversizedValueActionKind::defer_old_payload_cleanup,
                            false,
                            "OVERSIZED_DEFER_OLD_PAYLOAD_CLEANUP"};
                }

                if (input.rewrite_publication_complete)
                {
                    return {OversizedValueActionKind::cleanup_old_payload_now,
                            true,
                            "OVERSIZED_CLEANUP_OLD_PAYLOAD_NOW"};
                }

                return {OversizedValueActionKind::retain_existing_payload,
                        false,
                        "OVERSIZED_RETAIN_EXISTING_PAYLOAD"};

            case OversizedValueFamily::hash_overflow:
                if (input.overflow_allocation_requested)
                {
                    return {OversizedValueActionKind::allocate_overflow_page,
                            false,
                            "OVERSIZED_ALLOCATE_HASH_OVERFLOW"};
                }

                if (input.compaction_required)
                {
                    return {OversizedValueActionKind::compact_overflow_chain,
                            false,
                            "OVERSIZED_COMPACT_HASH_OVERFLOW_CHAIN"};
                }

                if (input.overflow_page_empty && input.rewrite_publication_complete)
                {
                    return {OversizedValueActionKind::unlink_empty_overflow_page,
                            true,
                            "OVERSIZED_UNLINK_EMPTY_HASH_OVERFLOW"};
                }

                return {OversizedValueActionKind::retain_existing_payload,
                        false,
                        "OVERSIZED_RETAIN_EXISTING_PAYLOAD"};
        }

        return {OversizedValueActionKind::reject_unclassified_path,
                false,
                "OVERSIZED_REJECT_UNCLASSIFIED_PATH"};
    }
}
