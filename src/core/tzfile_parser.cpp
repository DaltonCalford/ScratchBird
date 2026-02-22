/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/tzfile_parser.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <filesystem>

namespace scratchbird::core
{

    // ===== Binary Reading Utilities =====

    auto TZFileParser::read8(FILE *fp) -> uint8_t
    {
        uint8_t val = 0;
        if (fread(&val, 1, 1, fp) != 1)
        {
            return 0;
        }
        return val;
    }

    auto TZFileParser::read32(FILE *fp) -> uint32_t
    {
        uint8_t bytes[4];
        if (fread(bytes, 1, 4, fp) != 4)
        {
            return 0;
        }
        // TZif uses big-endian (network byte order)
        return (static_cast<uint32_t>(bytes[0]) << 24) |
               (static_cast<uint32_t>(bytes[1]) << 16) |
               (static_cast<uint32_t>(bytes[2]) << 8) |
               (static_cast<uint32_t>(bytes[3]));
    }

    auto TZFileParser::read64(FILE *fp) -> uint64_t
    {
        uint8_t bytes[8];
        if (fread(bytes, 1, 8, fp) != 8)
        {
            return 0;
        }
        // TZif uses big-endian (network byte order)
        return (static_cast<uint64_t>(bytes[0]) << 56) |
               (static_cast<uint64_t>(bytes[1]) << 48) |
               (static_cast<uint64_t>(bytes[2]) << 40) |
               (static_cast<uint64_t>(bytes[3]) << 32) |
               (static_cast<uint64_t>(bytes[4]) << 24) |
               (static_cast<uint64_t>(bytes[5]) << 16) |
               (static_cast<uint64_t>(bytes[6]) << 8) |
               (static_cast<uint64_t>(bytes[7]));
    }

    // ===== Header Parsing =====

    auto TZFileParser::readHeader(FILE *fp, TZifHeader &header, ErrorContext *ctx) -> Status
    {
        // Read magic number and version
        if (fread(header.magic, 1, 4, fp) != 4)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read TZif magic number");
            return Status::IO_ERROR;
        }

        // Validate magic number
        if (memcmp(header.magic, "TZif", 4) != 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid TZif magic number");
            return Status::INVALID_ARGUMENT;
        }

        // Read version byte
        header.version = read8(fp);
        if (header.version != '\0' && header.version != '2' && header.version != '3')
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Unsupported TZif version (only versions 1, 2, 3 supported)");
            return Status::INVALID_ARGUMENT;
        }

        // Read reserved bytes
        if (fread(header.reserved, 1, 15, fp) != 15)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read TZif reserved bytes");
            return Status::IO_ERROR;
        }

        // Read counts
        header.ttisgmtcnt = read32(fp);
        header.ttisstdcnt = read32(fp);
        header.leapcnt = read32(fp);
        header.timecnt = read32(fp);
        header.typecnt = read32(fp);
        header.charcnt = read32(fp);

        // Sanity check counts (prevent excessive memory allocation)
        if (header.timecnt > 100000 || header.typecnt > 1000 ||
            header.charcnt > 10000 || header.leapcnt > 1000)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "TZif header counts exceed reasonable limits");
            return Status::INVALID_ARGUMENT;
        }

        return Status::OK;
    }

    // ===== Transition Times Parsing =====

    auto TZFileParser::readTransitions32(FILE *fp, const TZifHeader &header,
                                         std::vector<TransitionTime> &transitions,
                                         ErrorContext *ctx) -> Status
    {
        transitions.clear();
        transitions.reserve(header.timecnt);

        // Read transition times (32-bit timestamps)
        std::vector<int32_t> times;
        times.reserve(header.timecnt);
        for (uint32_t i = 0; i < header.timecnt; i++)
        {
            int32_t time = static_cast<int32_t>(read32(fp));
            times.push_back(time);
        }

        // Read transition type indices
        std::vector<uint8_t> type_indices;
        type_indices.reserve(header.timecnt);
        for (uint32_t i = 0; i < header.timecnt; i++)
        {
            uint8_t type_idx = read8(fp);
            type_indices.push_back(type_idx);
        }

        // Combine into TransitionTime structures
        for (uint32_t i = 0; i < header.timecnt; i++)
        {
            TransitionTime trans;
            trans.time = static_cast<int64_t>(times[i]);
            trans.type_index = type_indices[i];
            transitions.push_back(trans);
        }

        return Status::OK;
    }

    auto TZFileParser::readTransitions64(FILE *fp, const TZifHeader &header,
                                         std::vector<TransitionTime> &transitions,
                                         ErrorContext *ctx) -> Status
    {
        transitions.clear();
        transitions.reserve(header.timecnt);

        // Read transition times (64-bit timestamps)
        std::vector<int64_t> times;
        times.reserve(header.timecnt);
        for (uint32_t i = 0; i < header.timecnt; i++)
        {
            int64_t time = static_cast<int64_t>(read64(fp));
            times.push_back(time);
        }

        // Read transition type indices
        std::vector<uint8_t> type_indices;
        type_indices.reserve(header.timecnt);
        for (uint32_t i = 0; i < header.timecnt; i++)
        {
            uint8_t type_idx = read8(fp);
            type_indices.push_back(type_idx);
        }

        // Combine into TransitionTime structures
        for (uint32_t i = 0; i < header.timecnt; i++)
        {
            TransitionTime trans;
            trans.time = times[i];
            trans.type_index = type_indices[i];
            transitions.push_back(trans);
        }

        return Status::OK;
    }

    // ===== Transition Types Parsing =====

    auto TZFileParser::readTypes(FILE *fp, const TZifHeader &header,
                                  std::vector<TransitionType> &types,
                                  ErrorContext *ctx) -> Status
    {
        types.clear();
        types.reserve(header.typecnt);

        for (uint32_t i = 0; i < header.typecnt; i++)
        {
            TransitionType type;

            // Read UTC offset (signed 32-bit)
            type.utoff = static_cast<int32_t>(read32(fp));

            // Read DST flag (1 byte)
            type.isdst = read8(fp);

            // Read abbreviation index (1 byte)
            type.abbrind = read8(fp);

            types.push_back(type);
        }

        return Status::OK;
    }

    // ===== Abbreviations Parsing =====

    auto TZFileParser::readAbbreviations(FILE *fp, const TZifHeader &header,
                                          std::string &abbrevs,
                                          ErrorContext *ctx) -> Status
    {
        abbrevs.clear();
        if (header.charcnt == 0)
        {
            return Status::OK;
        }

        abbrevs.resize(header.charcnt);
        if (fread(&abbrevs[0], 1, header.charcnt, fp) != header.charcnt)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read timezone abbreviations");
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    // ===== Leap Seconds Parsing =====

    auto TZFileParser::readLeapSeconds32(FILE *fp, const TZifHeader &header,
                                         std::vector<LeapSecond> &leaps,
                                         ErrorContext *ctx) -> Status
    {
        leaps.clear();
        leaps.reserve(header.leapcnt);

        for (uint32_t i = 0; i < header.leapcnt; i++)
        {
            LeapSecond leap;
            leap.time = static_cast<int64_t>(static_cast<int32_t>(read32(fp)));
            leap.total = static_cast<int32_t>(read32(fp));
            leaps.push_back(leap);
        }

        return Status::OK;
    }

    auto TZFileParser::readLeapSeconds64(FILE *fp, const TZifHeader &header,
                                         std::vector<LeapSecond> &leaps,
                                         ErrorContext *ctx) -> Status
    {
        leaps.clear();
        leaps.reserve(header.leapcnt);

        for (uint32_t i = 0; i < header.leapcnt; i++)
        {
            LeapSecond leap;
            leap.time = static_cast<int64_t>(read64(fp));
            leap.total = static_cast<int32_t>(read32(fp));
            leaps.push_back(leap);
        }

        return Status::OK;
    }

    // ===== POSIX TZ String Parsing =====

    auto TZFileParser::readPosixTZ(FILE *fp, std::string &posix_tz, ErrorContext *ctx) -> Status
    {
        posix_tz.clear();

        // Read until newline or EOF
        int ch;
        while ((ch = fgetc(fp)) != EOF)
        {
            if (ch == '\n')
            {
                break;
            }
            posix_tz += static_cast<char>(ch);
        }

        return Status::OK;
    }

    // ===== Timezone Name Extraction =====

    auto TZFileParser::extractTimezoneName(const std::string &filepath) -> std::string
    {
        // Try to extract timezone name from path
        // Examples:
        //   /usr/share/zoneinfo/America/New_York -> America/New_York
        //   resources/timezones/america -> america
        //   /path/to/zoneinfo/UTC -> UTC

        size_t zoneinfo_pos = filepath.find("/zoneinfo/");
        if (zoneinfo_pos != std::string::npos)
        {
            return filepath.substr(zoneinfo_pos + 10); // Skip "/zoneinfo/"
        }

        size_t resources_pos = filepath.find("resources/timezones/");
        if (resources_pos != std::string::npos)
        {
            return filepath.substr(resources_pos + 20); // Skip "resources/timezones/"
        }

        // Fallback: use last component of path
        size_t last_slash = filepath.find_last_of('/');
        if (last_slash != std::string::npos)
        {
            return filepath.substr(last_slash + 1);
        }

        return filepath;
    }

    // ===== Main Parsing Method =====

    auto TZFileParser::parseFile(const std::string &filepath,
                                  TimezoneData &tz_data,
                                  ErrorContext *ctx) -> Status
    {
        // Open file
        FILE *fp = fopen(filepath.c_str(), "rb");
        if (!fp)
        {
            std::string error_msg = "Cannot open timezone file: " + filepath;
            SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND, error_msg.c_str());
            return Status::FILE_NOT_FOUND;
        }

        // Read version 1 header
        TZifHeader header;
        Status status = readHeader(fp, header, ctx);
        if (status != Status::OK)
        {
            fclose(fp);
            return status;
        }

        // For version 2/3 files, we need to skip the version 1 data
        // and read the version 2/3 section instead
        if (header.version == '2' || header.version == '3')
        {
            // Calculate size of version 1 data block
            size_t v1_data_size = 0;

            // Transition times (4 bytes each) + type indices (1 byte each)
            v1_data_size += header.timecnt * 4;  // Times
            v1_data_size += header.timecnt * 1;  // Type indices

            // Transition types (6 bytes each)
            v1_data_size += header.typecnt * 6;

            // Abbreviation characters
            v1_data_size += header.charcnt;

            // Leap seconds (8 bytes each in v1)
            v1_data_size += header.leapcnt * 8;

            // Standard/wall and UTC/local indicators
            v1_data_size += header.ttisstdcnt;
            v1_data_size += header.ttisgmtcnt;

            // Skip version 1 data
            if (fseek(fp, v1_data_size, SEEK_CUR) != 0)
            {
                fclose(fp);
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to seek past version 1 data");
                return Status::IO_ERROR;
            }

            // Read version 2/3 header
            status = readHeader(fp, header, ctx);
            if (status != Status::OK)
            {
                fclose(fp);
                return status;
            }

            // Now parse version 2/3 data (64-bit timestamps)
            std::vector<TransitionTime> raw_transitions;
            status = readTransitions64(fp, header, raw_transitions, ctx);
            if (status != Status::OK)
            {
                fclose(fp);
                return status;
            }

            std::vector<TransitionType> types;
            status = readTypes(fp, header, types, ctx);
            if (status != Status::OK)
            {
                fclose(fp);
                return status;
            }

            std::string abbrevs;
            status = readAbbreviations(fp, header, abbrevs, ctx);
            if (status != Status::OK)
            {
                fclose(fp);
                return status;
            }

            status = readLeapSeconds64(fp, header, tz_data.leap_seconds, ctx);
            if (status != Status::OK)
            {
                fclose(fp);
                return status;
            }

            // Skip standard/wall and UTC/local indicators
            fseek(fp, header.ttisstdcnt + header.ttisgmtcnt, SEEK_CUR);

            // Read POSIX TZ string (after newline)
            int ch = fgetc(fp);
            if (ch == '\n')
            {
                status = readPosixTZ(fp, tz_data.posix_tz_string, ctx);
                if (status != Status::OK)
                {
                    fclose(fp);
                    return status;
                }
            }

            // Combine transitions and types
            tz_data.transitions.clear();
            tz_data.transitions.reserve(raw_transitions.size());

            for (const auto &trans : raw_transitions)
            {
                if (trans.type_index >= types.size())
                {
                    continue; // Skip invalid type index
                }

                const TransitionType &type = types[trans.type_index];

                TimezoneTransition tz_trans;
                tz_trans.timestamp = trans.time;
                tz_trans.utc_offset = type.utoff;
                tz_trans.is_dst = (type.isdst != 0);

                // Extract abbreviation
                if (type.abbrind < abbrevs.size())
                {
                    size_t abbr_start = type.abbrind;
                    size_t abbr_end = abbrevs.find('\0', abbr_start);
                    if (abbr_end == std::string::npos)
                    {
                        abbr_end = abbrevs.size();
                    }
                    tz_trans.abbreviation = abbrevs.substr(abbr_start, abbr_end - abbr_start);
                }

                tz_data.transitions.push_back(tz_trans);
            }
        }
        else
        {
            // Version 1 file - use 32-bit parsing
            std::vector<TransitionTime> raw_transitions;
            status = readTransitions32(fp, header, raw_transitions, ctx);
            if (status != Status::OK)
            {
                fclose(fp);
                return status;
            }

            std::vector<TransitionType> types;
            status = readTypes(fp, header, types, ctx);
            if (status != Status::OK)
            {
                fclose(fp);
                return status;
            }

            std::string abbrevs;
            status = readAbbreviations(fp, header, abbrevs, ctx);
            if (status != Status::OK)
            {
                fclose(fp);
                return status;
            }

            status = readLeapSeconds32(fp, header, tz_data.leap_seconds, ctx);
            if (status != Status::OK)
            {
                fclose(fp);
                return status;
            }

            // Combine transitions and types
            tz_data.transitions.clear();
            tz_data.transitions.reserve(raw_transitions.size());

            for (const auto &trans : raw_transitions)
            {
                if (trans.type_index >= types.size())
                {
                    continue;
                }

                const TransitionType &type = types[trans.type_index];

                TimezoneTransition tz_trans;
                tz_trans.timestamp = trans.time;
                tz_trans.utc_offset = type.utoff;
                tz_trans.is_dst = (type.isdst != 0);

                if (type.abbrind < abbrevs.size())
                {
                    size_t abbr_start = type.abbrind;
                    size_t abbr_end = abbrevs.find('\0', abbr_start);
                    if (abbr_end == std::string::npos)
                    {
                        abbr_end = abbrevs.size();
                    }
                    tz_trans.abbreviation = abbrevs.substr(abbr_start, abbr_end - abbr_start);
                }

                tz_data.transitions.push_back(tz_trans);
            }
        }

        // Extract timezone name from filepath
        tz_data.name = extractTimezoneName(filepath);

        fclose(fp);
        return Status::OK;
    }

    // ===== Directory Scanning =====

    auto TZFileParser::scanDirectory(const std::string &dir_path,
                                      std::vector<std::string> &files,
                                      ErrorContext *ctx) -> Status
    {
        std::error_code path_error;
        if (!std::filesystem::exists(dir_path, path_error) ||
            !std::filesystem::is_directory(dir_path, path_error))
        {
            std::string error_msg = "Cannot open directory: " + dir_path;
            SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND, error_msg.c_str());
            return Status::FILE_NOT_FOUND;
        }
        if (path_error)
        {
            std::string error_msg = "Failed to inspect directory: " + dir_path;
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, error_msg.c_str());
            return Status::IO_ERROR;
        }

        std::error_code iter_error;
        auto iterator = std::filesystem::recursive_directory_iterator(
            dir_path,
            std::filesystem::directory_options::skip_permission_denied,
            iter_error);

        for (const auto &entry : iterator)
        {
            if (iter_error)
            {
                std::string error_msg = "Failed to enumerate directory: " + dir_path;
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, error_msg.c_str());
                return Status::IO_ERROR;
            }

            std::string name = entry.path().filename().string();

            // Skip "." and ".."
            if (name == "." || name == "..")
            {
                continue;
            }

            // Skip known non-timezone files
            if (name == "posixrules" || name == "zone.tab" || name == "zone1970.tab" ||
                name == "iso3166.tab" || name == "leap-seconds.list" || name == "leapseconds" ||
                name == "README" || name == "LICENSE" || name == "CONTRIBUTING" ||
                name == "NEWS" || name == "SECURITY" || name.find(".awk") != std::string::npos ||
                name.find(".tar.gz") != std::string::npos || name.find(".list") != std::string::npos)
            {
                continue;
            }

            std::error_code type_error;
            if (!entry.is_regular_file(type_error))
            {
                continue;
            }
            if (type_error)
            {
                continue;
            }
            files.push_back(entry.path().string());
        }

        return Status::OK;
    }

    auto TZFileParser::parseDirectory(const std::string &zoneinfo_dir,
                                       std::vector<TimezoneData> &timezones,
                                       ErrorContext *ctx) -> Status
    {
        timezones.clear();

        // Scan directory for all timezone files
        std::vector<std::string> files;
        Status status = scanDirectory(zoneinfo_dir, files, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Parse each file
        size_t parsed_count = 0;
        for (const auto &filepath : files)
        {
            TimezoneData tz_data;
            status = parseFile(filepath, tz_data, ctx);

            if (status == Status::OK)
            {
                // Only add if we got valid transition data or POSIX string
                if (!tz_data.transitions.empty() || !tz_data.posix_tz_string.empty())
                {
                    timezones.push_back(std::move(tz_data));
                    parsed_count++;
                }
            }
            // Silently skip files that fail to parse (might be non-TZif files)
        }

        if (parsed_count == 0 && !files.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              "No valid timezone files found in directory");
            return Status::NOT_FOUND;
        }

        return Status::OK;
    }

} // namespace scratchbird::core
