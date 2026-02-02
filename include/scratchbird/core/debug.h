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

#include <iostream>
#include <sstream>
#include <string>

namespace scratchbird::core
{

// Debug logging configuration
#ifndef SCRATCHBIRD_DEBUG
#ifdef DEBUG
#define SCRATCHBIRD_DEBUG 1
#else
#define SCRATCHBIRD_DEBUG 0
#endif
#endif

// Debug log macro - only compiles in debug builds
#if SCRATCHBIRD_DEBUG
#define DEBUG_LOG(component, message)                                                       \
    do                                                                                      \
    {                                                                                       \
        std::ostringstream _debug_oss;                                                      \
        _debug_oss << "[DEBUG][" << component << "] " << __FILE__ << ":" << __LINE__ << " " \
                   << __func__ << "() - " << message;                                       \
        std::cerr << _debug_oss.str() << std::endl;                                         \
    } while (0)
#else
#define DEBUG_LOG(component, message) ((void)0)
#endif

// Component-specific debug macros
#define DEBUG_LOG_PM(message) DEBUG_LOG("PageManager", message)
#define DEBUG_LOG_BP(message) DEBUG_LOG("BufferPool", message)
#define DEBUG_LOG_DB(message) DEBUG_LOG("Database", message)
#define DEBUG_LOG_INDEX(message) DEBUG_LOG("Index", message)  // Task 17 MGA Phase 2.1

} // namespace scratchbird::core
