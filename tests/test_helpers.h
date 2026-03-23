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
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <vector>

#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_opcode_registry.h"
#include "scratchbird/sblr/v3_payloads.h"

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

    inline std::vector<uint8_t> minimalCompiledStoredCodeBytecode(
        const std::string& module_name = "test_stored_code",
        uint16_t dialect_id = 0)
    {
        scratchbird::sblr::v3::Container container;
        container.header.version_major = 3;
        container.header.version_minor = 0;
        container.header.version_patch = 0;
        container.header.flags = 0;
        container.metadata.module_name = module_name;
        container.metadata.module_version = "v3";
        container.metadata.dialect_id = dialect_id;
        container.metadata.target_platform = 0;

        scratchbird::sblr::v3::Buffer bytecode_stream;
        scratchbird::sblr::v3::DecodeError derr;

        scratchbird::sblr::v3::Instruction version_inst;
        version_inst.opcode = static_cast<uint16_t>(scratchbird::sblr::v3::Opcode::SBLR3_VERSION);
        version_inst.flags = 0;
        scratchbird::sblr::v3::Value::Bytes version_payload(6, 0);
        version_payload[0] = 3;
        version_inst.payload = scratchbird::sblr::v3::Value(std::move(version_payload));
        if (!scratchbird::sblr::v3::encodeInstructionWithSchema(version_inst, bytecode_stream, derr))
        {
            throw std::runtime_error("failed to encode minimal SBLR3_VERSION: " + derr.message);
        }

        scratchbird::sblr::v3::Instruction end_inst;
        end_inst.opcode = static_cast<uint16_t>(scratchbird::sblr::v3::Opcode::SBLR3_END);
        end_inst.flags = 0;
        end_inst.payload = scratchbird::sblr::v3::Value(scratchbird::sblr::v3::Value::Bytes{});
        if (!scratchbird::sblr::v3::encodeInstructionWithSchema(end_inst, bytecode_stream, derr))
        {
            throw std::runtime_error("failed to encode minimal SBLR3_END: " + derr.message);
        }

        container.bytecode_stream = std::move(bytecode_stream);

        std::vector<uint8_t> encoded;
        std::string encode_error;
        if (!scratchbird::sblr::v3::encodeContainer(container, encoded, encode_error))
        {
            throw std::runtime_error("failed to encode minimal stored-code container: " + encode_error);
        }
        return encoded;
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
