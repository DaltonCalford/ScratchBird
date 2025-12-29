#include <cstdlib>

namespace {

void setEnvVar(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

struct TestEnvironmentConfig {
    TestEnvironmentConfig() {
        // Disable background monitoring to keep unit tests deterministic.
        setEnvVar("SCRATCHBIRD_LONG_TRANSACTIONS_ENABLED", "0");
    }
};

const TestEnvironmentConfig kTestEnvironmentConfig;

} // namespace
