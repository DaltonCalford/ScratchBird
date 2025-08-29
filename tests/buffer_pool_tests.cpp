#include "scratchbird/engine/buffer_pool.h"
#include "scratchbird/engine/background_writer.h"

#include <gtest/gtest.h>

using namespace scratchbird::engine;

int main()
{
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}

TEST(BufferPool, BasicPinUnpin)
{
    BufferPool pool(8, 4096);
    BufferTag t{1, 42};

    auto h1 = pool.get(t);
    ASSERT_TRUE(h1.valid());
    EXPECT_EQ(h1.frame()->refcount.load(), 1);
    h1.frame()->data[0] = 0xAB;
    h1.mark_dirty();

    // Second pin increments refcount
    auto h2 = pool.get(t);
    ASSERT_TRUE(h2.valid());
    EXPECT_EQ(h2.frame()->refcount.load(), 2);

    // Release both
    h2 = BufferHandle{};
    h1 = BufferHandle{};
}

TEST(BufferPool, EvictionClockSweep)
{
    BufferPool pool(2, 1024);
    BufferTag a{1, 1}, b{1, 2}, c{1, 3};
    auto ha = pool.get(a);
    auto hb = pool.get(b);
    // Unpin a
    ha = BufferHandle{};
    // Access b to set clock bit
    hb.frame();
    // Insert c forces eviction (of a most likely)
    auto hc = pool.get(c);
    ASSERT_TRUE(hc.valid());
}

TEST(BackgroundWriter, FlushBatch)
{
    BufferPool pool(4, 2048);
    std::vector<BufferTag> flushed;
    pool.set_flush_callback([&](const BufferFrame& f) { flushed.push_back(f.tag); });

    // Dirty two frames
    for (int i = 0; i < 2; ++i) {
        BufferTag t{2, static_cast<std::uint64_t>(i)};
        auto h = pool.get(t);
        h.mark_dirty();
    }

    BackgroundWriter bw(pool, std::chrono::milliseconds(10), 4);
    bw.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    bw.stop();

    ASSERT_GE(flushed.size(), 2u);
}

