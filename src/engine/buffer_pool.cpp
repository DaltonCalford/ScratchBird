#include "scratchbird/engine/buffer_pool.h"

#include <algorithm>
#include <cstring>

namespace scratchbird::engine
{

    // BufferHandle implementation
    BufferFrame* BufferHandle::frame()
    {
        if (!valid())
            return nullptr;
        return pool_->frames_[index_].get();
    }

    const BufferFrame* BufferHandle::frame() const
    {
        if (!valid())
            return nullptr;
        return pool_->frames_[index_].get();
    }

    void BufferHandle::mark_dirty()
    {
        if (valid()) {
            pool_->mark_dirty(index_);
        }
    }

    void BufferHandle::release()
    {
        if (valid()) {
            pool_->dec_ref(index_);
            pool_ = nullptr;
            index_ = -1;
        }
    }

} // namespace scratchbird::engine
