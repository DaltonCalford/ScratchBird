/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2024 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef CLASSES_UUID_H
#define CLASSES_UUID_H

#include "firebird.h"
#include "../common/gdsassert.h"
#include "../common/os/guid.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>

namespace ScratchBird
{

class Uuid
{
private:
	explicit Uuid(unsigned version)
	{
		switch (version)
		{
			case 1:
				generateV1();
				break;

			case 2:
				generateV2();
				break;

			case 3:
				// Version 3 requires namespace and name - use default values for now
				generateV3(nullptr, 0, nullptr, 0);
				break;

			case 4:
				generateV4();
				break;

			case 5:
				// Version 5 requires namespace and name - use default values for now
				generateV5(nullptr, 0, nullptr, 0);
				break;

			case 6:
				generateV6();
				break;

			case 7:
				generateV7();
				break;

			case 8:
				generateV8();
				break;

			default:
				fb_assert(false);
		}
	}

public:
	static Uuid generate(unsigned version)
	{
		return Uuid(version);
	}

public:
	std::size_t extractBytes(std::uint8_t* buffer, std::size_t bufferSize) const
	{
		fb_assert(bufferSize >= bytes.size());
		std::copy(bytes.begin(), bytes.end(), buffer);
		return bytes.size();
	}

	std::size_t toString(char* buffer, std::size_t bufferSize) const
	{
		fb_assert(bufferSize >= STR_LEN);

		return snprintf(buffer, bufferSize, STR_FORMAT,
			bytes[0], bytes[1], bytes[2], bytes[3],
			bytes[4], bytes[5],
			bytes[6], bytes[7],
			bytes[8], bytes[9],
			bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
	}

private:
	void generateV4()
	{
		GenerateRandomBytes(bytes.data(), static_cast<FB_SIZE_T>(bytes.size()));

		// version and variant
		bytes[6] = (bytes[6] & 0x0F) | 0x40;
		bytes[8] = (bytes[8] & 0x3F) | 0x80;
	}

	void generateV7()
	{
		GenerateRandomBytes(bytes.data() + 6, static_cast<FB_SIZE_T>(bytes.size()) - 6);

		// current timestamp in ms
		const auto now = std::chrono::system_clock::now();
		const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

		// timestamp
		bytes[0] = (millis >> 40) & 0xFF;
		bytes[1] = (millis >> 32) & 0xFF;
		bytes[2] = (millis >> 24) & 0xFF;
		bytes[3] = (millis >> 16) & 0xFF;
		bytes[4] = (millis >> 8) & 0xFF;
		bytes[5] = millis & 0xFF;

		// version and variant
		bytes[6] = (bytes[6] & 0x0F) | 0x70;
		bytes[8] = (bytes[8] & 0x3F) | 0x80;
	}

	void generateV1()
	{
		// Generate random node ID (48 bits) with multicast bit set for privacy
		GenerateRandomBytes(bytes.data() + 10, 6);
		bytes[10] |= 0x01; // Set multicast bit for privacy

		// Generate random clock sequence (14 bits)
		uint16_t clockSeq;
		GenerateRandomBytes(reinterpret_cast<uint8_t*>(&clockSeq), sizeof(clockSeq));
		clockSeq &= 0x3FFF; // 14 bits

		// Get Gregorian timestamp (100-nanosecond intervals since Oct 15, 1582)
		const auto now = std::chrono::system_clock::now();
		const auto epochTime = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
		// Convert from Unix epoch (1970) to Gregorian epoch (1582)
		const uint64_t gregorianOffset = 122192928000000000ULL; // 100-ns intervals between epochs
		const uint64_t timestamp = (epochTime / 100) + gregorianOffset;

		// Layout timestamp in v1 format: time_low, time_mid, time_hi_and_version
		bytes[0] = (timestamp >> 24) & 0xFF;  // time_low
		bytes[1] = (timestamp >> 16) & 0xFF;
		bytes[2] = (timestamp >> 8) & 0xFF;
		bytes[3] = timestamp & 0xFF;
		bytes[4] = (timestamp >> 40) & 0xFF;  // time_mid
		bytes[5] = (timestamp >> 32) & 0xFF;
		bytes[6] = ((timestamp >> 56) & 0x0F) | 0x10; // time_hi_and_version
		bytes[7] = (timestamp >> 48) & 0xFF;
		bytes[8] = ((clockSeq >> 8) & 0x3F) | 0x80; // clock_seq_hi_and_reserved
		bytes[9] = clockSeq & 0xFF; // clock_seq_low
		// Node ID already set above (bytes 10-15)
	}

	void generateV2()
	{
		// Version 2 is based on Version 1 but embeds local domain and ID
		generateV1();
		
		// Modify for DCE security version
		bytes[6] = (bytes[6] & 0x0F) | 0x20; // Set version to 2
		
		// For simplicity, use domain 0 (person) and ID 0
		bytes[0] = 0; // Replace time_low with local ID (32 bits)
		bytes[1] = 0;
		bytes[2] = 0;
		bytes[3] = 0;
		bytes[9] = 0; // Replace low byte of clock_seq with local domain
	}

	void generateV3(const uint8_t* ns, size_t nsLen, const uint8_t* name, size_t nameLen)
	{
		// For now, implement a simplified version that generates random data
		// Full implementation would require MD5 hashing of namespace + name
		GenerateRandomBytes(bytes.data(), static_cast<FB_SIZE_T>(bytes.size()));
		
		// Set version and variant
		bytes[6] = (bytes[6] & 0x0F) | 0x30; // Version 3
		bytes[8] = (bytes[8] & 0x3F) | 0x80; // Variant
	}

	void generateV5(const uint8_t* ns, size_t nsLen, const uint8_t* name, size_t nameLen)
	{
		// For now, implement a simplified version that generates random data
		// Full implementation would require SHA-1 hashing of namespace + name
		GenerateRandomBytes(bytes.data(), static_cast<FB_SIZE_T>(bytes.size()));
		
		// Set version and variant
		bytes[6] = (bytes[6] & 0x0F) | 0x50; // Version 5
		bytes[8] = (bytes[8] & 0x3F) | 0x80; // Variant
	}

	void generateV6()
	{
		// Version 6 is a reordered version 1 for better sorting
		// Get Gregorian timestamp
		const auto now = std::chrono::system_clock::now();
		const auto epochTime = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
		const uint64_t gregorianOffset = 122192928000000000ULL;
		const uint64_t timestamp = (epochTime / 100) + gregorianOffset;

		// Generate random node ID and clock sequence
		GenerateRandomBytes(bytes.data() + 8, 8);
		bytes[10] |= 0x01; // Set multicast bit

		// Layout in v6 format: reordered timestamp for sortability
		bytes[0] = (timestamp >> 52) & 0xFF; // timestamp high (48 bits)
		bytes[1] = (timestamp >> 44) & 0xFF;
		bytes[2] = (timestamp >> 36) & 0xFF;
		bytes[3] = (timestamp >> 28) & 0xFF;
		bytes[4] = (timestamp >> 20) & 0xFF;
		bytes[5] = (timestamp >> 12) & 0xFF;
		bytes[6] = ((timestamp >> 8) & 0x0F) | 0x60; // version + timestamp mid
		bytes[7] = timestamp & 0xFF; // timestamp low
		bytes[8] = (bytes[8] & 0x3F) | 0x80; // variant
	}

	void generateV8()
	{
		// Version 8 is for custom/experimental use
		GenerateRandomBytes(bytes.data(), static_cast<FB_SIZE_T>(bytes.size()));
		
		// Set version and variant
		bytes[6] = (bytes[6] & 0x0F) | 0x80; // Version 8
		bytes[8] = (bytes[8] & 0x3F) | 0x80; // Variant
	}

public:
	static constexpr std::size_t BYTE_LEN = 16;
	static constexpr std::size_t STR_LEN = 36;
	static constexpr const char* STR_FORMAT =
		"%02hhX%02hhX%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX";

private:
	std::array<uint8_t, BYTE_LEN> bytes;
};

}	// namespace ScratchBird

#endif	// CLASSES_UUID_H
