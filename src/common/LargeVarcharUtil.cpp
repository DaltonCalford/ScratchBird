/*
 *	PROGRAM:		Large VARCHAR support
 *	MODULE:			LargeVarcharUtil.cpp
 *	DESCRIPTION:	Large VARCHAR (128KB) utility implementation
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

#include "firebird.h"
#include "../include/sb_large_varchar.h"
#include <cstring>
#include <algorithm>

namespace ScratchBird {

// Determine storage strategy for given length
VarcharOverflowInfo LargeVarcharUtil::getStorageInfo(ULONG length) {
    return VarcharOverflowInfo(length);
}

// Convert between standard vary and large_vary
void LargeVarcharUtil::convertToLarge(const vary* src, LARGE_VARY* dest) {
    dest->vary_length = src->vary_length;
    if (src->vary_length > 0) {
        memcpy(dest->vary_string, src->vary_string, src->vary_length);
    }
}

void LargeVarcharUtil::convertFromLarge(const LARGE_VARY* src, vary* dest, ULONG maxLength) {
    ULONG copyLength = std::min(src->vary_length, maxLength);
    dest->vary_length = static_cast<USHORT>(copyLength);
    if (copyLength > 0) {
        memcpy(dest->vary_string, src->vary_string, copyLength);
    }
}

// Storage operations
void LargeVarcharUtil::storeInline(LARGE_VARY* dest, const UCHAR* data, ULONG length) {
    dest->vary_length = length;
    if (length > 0 && data) {
        memcpy(dest->vary_string, data, length);
    }
}

void LargeVarcharUtil::storeBlobOverflow(ISC_QUAD* blobId, const UCHAR* data, ULONG length) {
    // TODO: Implement blob storage for very large VARCHARs
    // This would integrate with ScratchBird's blob subsystem
    // For now, we'll use inline storage up to the threshold
    blobId->gds_quad_high = 0;
    blobId->gds_quad_low = 0;
}

// UTF-8 character counting and validation
ULONG LargeVarcharUtil::countUtf8Characters(const UCHAR* data, ULONG byteLength) {
    if (!data || byteLength == 0) {
        return 0;
    }
    
    ULONG charCount = 0;
    const UCHAR* end = data + byteLength;
    
    while (data < end) {
        UCHAR byte = *data;
        
        if (byte < 0x80) {
            // ASCII character (1 byte)
            data += 1;
        } else if ((byte & 0xE0) == 0xC0) {
            // 2-byte UTF-8 sequence
            if (data + 1 >= end || (data[1] & 0xC0) != 0x80) {
                break;  // Invalid sequence
            }
            data += 2;
        } else if ((byte & 0xF0) == 0xE0) {
            // 3-byte UTF-8 sequence
            if (data + 2 >= end || (data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80) {
                break;  // Invalid sequence
            }
            data += 3;
        } else if ((byte & 0xF8) == 0xF0) {
            // 4-byte UTF-8 sequence
            if (data + 3 >= end || (data[1] & 0xC0) != 0x80 || 
                (data[2] & 0xC0) != 0x80 || (data[3] & 0xC0) != 0x80) {
                break;  // Invalid sequence
            }
            data += 4;
        } else {
            // Invalid UTF-8 start byte
            break;
        }
        
        charCount++;
    }
    
    return charCount;
}

bool LargeVarcharUtil::validateUtf8(const UCHAR* data, ULONG length) {
    if (!data || length == 0) {
        return true;  // Empty string is valid
    }
    
    const UCHAR* end = data + length;
    
    while (data < end) {
        UCHAR byte = *data;
        
        if (byte < 0x80) {
            // ASCII character
            data += 1;
        } else if ((byte & 0xE0) == 0xC0) {
            // 2-byte sequence
            if (data + 1 >= end || (data[1] & 0xC0) != 0x80) {
                return false;
            }
            // Check for overlong encoding
            if (byte < 0xC2) {
                return false;
            }
            data += 2;
        } else if ((byte & 0xF0) == 0xE0) {
            // 3-byte sequence
            if (data + 2 >= end || (data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80) {
                return false;
            }
            // Check for overlong encoding and surrogates
            if (byte == 0xE0 && data[1] < 0xA0) {
                return false;  // Overlong
            }
            if (byte == 0xED && data[1] >= 0xA0) {
                return false;  // Surrogate
            }
            data += 3;
        } else if ((byte & 0xF8) == 0xF0) {
            // 4-byte sequence
            if (data + 3 >= end || (data[1] & 0xC0) != 0x80 || 
                (data[2] & 0xC0) != 0x80 || (data[3] & 0xC0) != 0x80) {
                return false;
            }
            // Check for overlong encoding and out-of-range
            if (byte == 0xF0 && data[1] < 0x90) {
                return false;  // Overlong
            }
            if (byte > 0xF4 || (byte == 0xF4 && data[1] >= 0x90)) {
                return false;  // Out of range
            }
            data += 4;
        } else {
            // Invalid start byte
            return false;
        }
    }
    
    return true;
}

ULONG LargeVarcharUtil::utf8CharacterToByteOffset(const UCHAR* data, ULONG charOffset) {
    if (!data || charOffset == 0) {
        return 0;
    }
    
    ULONG currentChar = 0;
    ULONG byteOffset = 0;
    
    while (currentChar < charOffset) {
        UCHAR byte = data[byteOffset];
        
        if (byte < 0x80) {
            byteOffset += 1;
        } else if ((byte & 0xE0) == 0xC0) {
            byteOffset += 2;
        } else if ((byte & 0xF0) == 0xE0) {
            byteOffset += 3;
        } else if ((byte & 0xF8) == 0xF0) {
            byteOffset += 4;
        } else {
            break;  // Invalid UTF-8
        }
        
        currentChar++;
    }
    
    return byteOffset;
}

// Index key generation for large VARCHARs
ULONG LargeVarcharUtil::makeIndexKey(const LARGE_VARY* varchar, vary* keyBuffer, ULONG maxKeySize) {
    if (!varchar || !keyBuffer || maxKeySize == 0) {
        keyBuffer->vary_length = 0;
        return 0;
    }
    
    // For indexing, we truncate to the maximum key size
    ULONG copyLength = std::min(static_cast<ULONG>(varchar->vary_length), static_cast<ULONG>(maxKeySize - sizeof(USHORT)));
    keyBuffer->vary_length = static_cast<USHORT>(copyLength);
    
    if (copyLength > 0) {
        memcpy(keyBuffer->vary_string, varchar->vary_string, copyLength);
    }
    
    return sizeof(USHORT) + copyLength;
}

} // namespace ScratchBird