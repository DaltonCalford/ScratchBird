/*
 *	PROGRAM:		Network Address Types
 *	MODULE:			InetAddr.h
 *	DESCRIPTION:	Internet address support (IPv4/IPv6)
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
 *
 */

#ifndef SB_INET_ADDR
#define SB_INET_ADDR

#include "firebird/Interface.h"
#include "sb_exception.h"
#include "classes/fb_string.h"

#include <string.h>
#include <netinet/in.h>

namespace ScratchBird {

enum InetFamily {
    INET_IPV4 = 4,
    INET_IPV6 = 6
};

class InetAddr
{
public:
    InetAddr();
    InetAddr(const char* addr_string);
    InetAddr(const sockaddr* addr);
    InetAddr(const sockaddr_in* addr);
    InetAddr(const sockaddr_in6* addr);

    // Assignment and conversion
    InetAddr& operator=(const char* addr_string);
    InetAddr& operator=(const InetAddr& other);

    // String representation
    void toString(string& result) const;
    string toString() const;

    // Network operations
    bool isValid() const { return family == INET_IPV4 || family == INET_IPV6; }
    bool isIPv4() const { return family == INET_IPV4; }
    bool isIPv6() const { return family == INET_IPV6; }
    
    InetFamily getFamily() const { return family; }
    
    // Comparison operators
    bool operator==(const InetAddr& other) const;
    bool operator!=(const InetAddr& other) const { return !(*this == other); }
    bool operator<(const InetAddr& other) const;
    bool operator>(const InetAddr& other) const { return other < *this; }
    bool operator<=(const InetAddr& other) const { return !(other < *this); }
    bool operator>=(const InetAddr& other) const { return !(*this < other); }

    // Network utilities
    bool isPrivate() const;
    bool isLoopback() const;
    bool isMulticast() const;
    
    // Data access
    const UCHAR* getBytes() const { return addr_bytes; }
    ULONG getSize() const { return family == INET_IPV4 ? 4 : 16; }

    // Index key generation
    ULONG makeIndexKey(vary* buf) const;
    static ULONG getIndexKeyLength() { return 17; } // 1 byte family + 16 bytes address

    // Parsing and validation
    static bool isValidAddress(const char* addr_string);
    static InetAddr parse(const char* addr_string);

private:
    InetFamily family;
    UCHAR addr_bytes[16];  // IPv6 is largest (16 bytes)
    
    void parseAddress(const char* addr_string);
    static void invalid_address();
};

class CidrBlock
{
public:
    CidrBlock();
    CidrBlock(const char* cidr_string);
    CidrBlock(const InetAddr& address, int prefix_length);

    // Assignment and conversion  
    CidrBlock& operator=(const char* cidr_string);
    CidrBlock& operator=(const CidrBlock& other);

    // String representation
    void toString(string& result) const;
    string toString() const;

    // Network operations
    bool contains(const InetAddr& addr) const;
    bool contains(const CidrBlock& other) const;
    bool overlaps(const CidrBlock& other) const;
    
    bool isValid() const { return address.isValid() && prefix_length >= 0 && prefix_length <= getMaxPrefixLength(); }
    
    // Accessors
    const InetAddr& getAddress() const { return address; }
    int getPrefixLength() const { return prefix_length; }
    InetFamily getFamily() const { return address.getFamily(); }
    int getMaxPrefixLength() const { return address.isIPv4() ? 32 : 128; }

    // Comparison operators  
    bool operator==(const CidrBlock& other) const;
    bool operator!=(const CidrBlock& other) const { return !(*this == other); }
    bool operator<(const CidrBlock& other) const;

    // Contains operators (PostgreSQL-style)
    bool operator>>(const InetAddr& addr) const { return contains(addr); }      // CIDR contains INET
    bool operator>>(const CidrBlock& other) const { return contains(other); }   // CIDR contains CIDR
    bool operator<<(const CidrBlock& other) const { return other.contains(*this); } // CIDR contained by CIDR

    // Network utilities
    InetAddr getNetworkAddress() const;
    InetAddr getBroadcastAddress() const;
    CidrBlock getSupernet(int new_prefix_length) const;
    
    // Index key generation
    ULONG makeIndexKey(vary* buf) const;
    static ULONG getIndexKeyLength() { return 18; } // 1 byte family + 16 bytes address + 1 byte prefix

    // Parsing
    static bool isValidCidr(const char* cidr_string);
    static CidrBlock parse(const char* cidr_string);

private:
    InetAddr address;
    int prefix_length;
    
    void parseCidr(const char* cidr_string);
    void applyNetmask();
    static void invalid_cidr();
};

class MacAddr
{
public:
    MacAddr();
    MacAddr(const char* mac_string);
    MacAddr(const UCHAR* mac_bytes);

    // Assignment and conversion
    MacAddr& operator=(const char* mac_string);
    MacAddr& operator=(const MacAddr& other);

    // String representation  
    void toString(string& result) const;
    string toString() const;

    // Validation
    bool isValid() const;
    
    // Comparison operators
    bool operator==(const MacAddr& other) const;
    bool operator!=(const MacAddr& other) const { return !(*this == other); }
    bool operator<(const MacAddr& other) const;

    // Data access
    const UCHAR* getBytes() const { return mac_bytes; }
    static ULONG getSize() { return 6; }

    // MAC address utilities
    bool isUnicast() const { return (mac_bytes[0] & 0x01) == 0; }
    bool isMulticast() const { return (mac_bytes[0] & 0x01) == 1; }
    bool isBroadcast() const;
    bool isLocallyAdministered() const { return (mac_bytes[0] & 0x02) != 0; }
    bool isUniversallyAdministered() const { return (mac_bytes[0] & 0x02) == 0; }

    // Index key generation
    ULONG makeIndexKey(vary* buf) const;
    static ULONG getIndexKeyLength() { return 6; } // 6 bytes MAC address

    // Parsing and validation
    static bool isValidMac(const char* mac_string);
    static MacAddr parse(const char* mac_string);

private:
    UCHAR mac_bytes[6];
    
    void parseMac(const char* mac_string);
    static void invalid_mac();
};

} // namespace ScratchBird

#endif // SB_INET_ADDR