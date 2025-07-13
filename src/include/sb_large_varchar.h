/*
 *	PROGRAM:		Large VARCHAR support
 *	MODULE:			sb_large_varchar.h
 *	DESCRIPTION:	Large VARCHAR (128KB) data structures
 *
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
 *  The Original Code was created by ScratchBird Development Team
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2025 ScratchBird Development Team
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): _______________________________________.
 *
 */

#ifndef SB_LARGE_VARCHAR_H
#define SB_LARGE_VARCHAR_H

#include "firebird/Interface.h"

namespace ScratchBird {

// Large VARCHAR structure (up to 128KB)
// Uses 32-bit length field instead of 16-bit
typedef struct large_vary {
    ULONG		vary_length;		// 32-bit length (up to 4GB theoretical, 128KB practical)
    UCHAR		vary_string[1];		// Variable length string data
} LARGE_VARY;

// Smart overflow architecture for large VARCHARs
// Determines whether to use inline storage or blob storage
inline constexpr ULONG VARCHAR_INLINE_THRESHOLD = 28672;  // 28KB threshold

struct VarcharOverflowInfo {
    bool isInline;
    ULONG actualLength;
    ULONG storageLength;
    
    VarcharOverflowInfo(ULONG length) {
        actualLength = length;
        isInline = (length <= VARCHAR_INLINE_THRESHOLD);
        
        if (isInline) {
            storageLength = sizeof(LARGE_VARY) + length;
        } else {
            // Use blob storage for very large VARCHARs
            storageLength = sizeof(ISC_QUAD);  // Store as blob reference
        }
    }
};

// Helper functions for large VARCHAR operations
class LargeVarcharUtil {
public:
    // Determine storage strategy for given length
    static VarcharOverflowInfo getStorageInfo(ULONG length);
    
    // Convert between standard vary and large_vary
    static void convertToLarge(const vary* src, LARGE_VARY* dest);
    static void convertFromLarge(const LARGE_VARY* src, vary* dest, ULONG maxLength);
    
    // Storage and retrieval operations
    static void storeInline(LARGE_VARY* dest, const UCHAR* data, ULONG length);
    static void storeBlobOverflow(ISC_QUAD* blobId, const UCHAR* data, ULONG length);
    
    // UTF-8 character counting and validation
    static ULONG countUtf8Characters(const UCHAR* data, ULONG byteLength);
    static bool validateUtf8(const UCHAR* data, ULONG length);
    static ULONG utf8CharacterToByteOffset(const UCHAR* data, ULONG charOffset);
    
    // Index key generation for large VARCHARs
    static ULONG makeIndexKey(const LARGE_VARY* varchar, vary* keyBuffer, ULONG maxKeySize);
};

} // namespace ScratchBird

#endif // SB_LARGE_VARCHAR_H