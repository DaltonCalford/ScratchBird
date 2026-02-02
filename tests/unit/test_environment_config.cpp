/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
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
