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

#include <atomic>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <vector>

#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_opcode_registry.h"

namespace scratchbird::testing
{
    inline std::filesystem::path resolveTestTempRoot()
    {
        // Explicit override for CI or constrained environments.
        if (const char* env = std::getenv("SCRATCHBIRD_TEST_TMPDIR"))
        {
            std::filesystem::path from_env(env);
            if (!from_env.empty())
            {
                std::error_code ec;
                std::filesystem::create_directories(from_env, ec);
                if (!ec)
                {
                    return from_env;
                }
            }
        }

        // Prefer a project-local temp root to avoid exhausting tmpfs-backed /tmp.
        std::error_code ec;
        std::filesystem::path cwd = std::filesystem::current_path(ec);
        if (!ec)
        {
            std::filesystem::path local_root = cwd / ".scratchbird-test-tmp";
            std::filesystem::create_directories(local_root, ec);
            if (!ec)
            {
                return local_root;
            }
        }

        // Fall back to system temp directory if project-local allocation fails.
        ec.clear();
        std::filesystem::path temp_root =
            std::filesystem::temp_directory_path(ec) / "scratchbird-tests";
        if (!ec)
        {
            std::filesystem::create_directories(temp_root, ec);
            if (!ec)
            {
                return temp_root;
            }
        }

        // Last resort.
        return std::filesystem::temp_directory_path();
    }

    inline const std::filesystem::path& testTempRoot()
    {
        static const std::filesystem::path root = resolveTestTempRoot();
        return root;
    }

    inline std::string uniqueTestDbPath(const std::string& base,
                                        const std::string& extension = ".db")
    {
        static std::atomic<uint64_t> counter{0};
        const auto suffix = std::to_string(::getpid()) + "_" + std::to_string(counter.fetch_add(1));
        std::string filename = base + "_" + suffix + extension;
        return (testTempRoot() / filename).string();
    }

    inline std::string uniqueTestShortPath(const std::string& base,
                                           const std::string& extension = "")
    {
        static std::atomic<uint64_t> counter{0};
        const auto suffix = std::to_string(::getpid()) + "_" + std::to_string(counter.fetch_add(1));
        std::string filename = base + "_" + suffix + extension;
        std::filesystem::path root = "/tmp";
        if (!std::filesystem::exists(root))
        {
            root = std::filesystem::temp_directory_path();
        }
        return (root / filename).string();
    }

    inline std::string uniqueTestSocketPath(const std::string& base)
    {
        return uniqueTestShortPath(base, ".sock");
    }

    inline bool networkTestsEnabled()
    {
        const char* env = std::getenv("SCRATCHBIRD_TEST_NETWORK");
        if (!env)
        {
            return false;
        }
        std::string value(env);
        for (auto& ch : value)
        {
            ch = static_cast<char>(std::tolower(ch));
        }
        return value == "1" || value == "true" || value == "yes" || value == "on";
    }

    inline std::filesystem::path findProjectRoot()
    {
        if (const char* env = std::getenv("SCRATCHBIRD_PROJECT_ROOT"))
        {
            std::filesystem::path from_env(env);
            if (!from_env.empty())
            {
                return from_env;
            }
        }

        std::filesystem::path probe = std::filesystem::current_path();
        for (int depth = 0; depth < 8; ++depth)
        {
            auto charsets = probe / "resources" / "charsets" / "charsets.json";
            auto collations = probe / "resources" / "collations" / "collations.json";
            if (std::filesystem::exists(charsets) || std::filesystem::exists(collations))
            {
                return probe;
            }
            if (!probe.has_parent_path())
            {
                break;
            }
            probe = probe.parent_path();
        }

        return {};
    }

    class TestDatabaseFile
    {
    public:
        explicit TestDatabaseFile(const std::string& name, const std::string& extension = ".sbdb")
        {
            // Use atomic counter + PID + thread ID for true uniqueness in parallel test runs
            static std::atomic<uint64_t> counter{0};
            auto tid = std::this_thread::get_id();
            std::ostringstream oss;
            oss << name << "_" << ::getpid() << "_" << tid << "_" << counter.fetch_add(1);
            std::string filename = oss.str() + extension;
            path_ = (testTempRoot() / filename).string();
            std::filesystem::remove_all(path_);
        }

        ~TestDatabaseFile()
        {
            if (!path_.empty())
            {
                std::filesystem::remove_all(path_);
            }
        }

        const std::string& path() const
        {
            return path_;
        }

        const char* c_str() const
        {
            return path_.c_str();
        }

    private:
        std::string path_;
    };

    inline bool decodeV3Container(const std::vector<uint8_t>& bytes,
                                  scratchbird::sblr::v3::Container& out,
                                  std::string* err_out = nullptr)
    {
        std::string err;
        if (!scratchbird::sblr::v3::decodeContainer(bytes.data(), bytes.size(), out, err))
        {
            if (err_out) *err_out = err;
            return false;
        }
        return true;
    }

    inline bool v3BytecodeContainsOpcode(const std::vector<uint8_t>& bytes,
                                         scratchbird::sblr::v3::Opcode opcode,
                                         std::string* err_out = nullptr)
    {
        scratchbird::sblr::v3::Container container;
        std::string err;
        if (!decodeV3Container(bytes, container, &err))
        {
            if (err_out) *err_out = err;
            return false;
        }

        const auto& stream = container.bytecode_stream;
        size_t offset = 0;
        scratchbird::sblr::v3::DecodeError derr;
        while (offset < stream.size())
        {
            scratchbird::sblr::v3::Instruction inst;
            if (!scratchbird::sblr::v3::decodeInstruction(stream.data(),
                                                          stream.size(),
                                                          offset,
                                                          inst,
                                                          derr))
            {
                if (err_out) *err_out = derr.message;
                return false;
            }
            if (inst.opcode == static_cast<uint16_t>(opcode))
            {
                return true;
            }
        }
        return false;
    }
} // namespace scratchbird::testing
