#include <gtest/gtest.h>
#include <cstdlib>

namespace {

class ScratchbirdTestEnvironment : public ::testing::Environment
{
public:
    void SetUp() override
    {
        // Avoid background monitor noise/hangs in unit tests unless explicitly enabled.
        const char* existing = std::getenv("SCRATCHBIRD_LONG_TRANSACTIONS_ENABLED");
        if (existing)
        {
            return;
        }
#if defined(_WIN32)
        _putenv_s("SCRATCHBIRD_LONG_TRANSACTIONS_ENABLED", "0");
#else
        setenv("SCRATCHBIRD_LONG_TRANSACTIONS_ENABLED", "0", 1);
#endif
    }
};

const ::testing::Environment* const kScratchbirdTestEnvironment =
    ::testing::AddGlobalTestEnvironment(new ScratchbirdTestEnvironment());

} // namespace
