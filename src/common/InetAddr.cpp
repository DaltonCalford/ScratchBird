/*
 *	PROGRAM:		Network Address Types
 *	MODULE:			InetAddr.cpp
 *	DESCRIPTION:	Internet address implementation
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

#include "InetAddr.h"
#include <arpa/inet.h>
#include <cstring>
#include <cstdio>

namespace ScratchBird {

// InetAddr implementation
InetAddr::InetAddr()
    : family(static_cast<InetFamily>(0))
{
    memset(addr_bytes, 0, sizeof(addr_bytes));
}

InetAddr::InetAddr(const char* addr_string)
    : family(static_cast<InetFamily>(0))
{
    memset(addr_bytes, 0, sizeof(addr_bytes));
    parseAddress(addr_string);
}

void InetAddr::parseAddress(const char* addr_string)
{
    if (!addr_string)
        invalid_address();

    // Try IPv4 first
    struct in_addr ipv4_addr;
    if (inet_pton(AF_INET, addr_string, &ipv4_addr) == 1)
    {
        family = INET_IPV4;
        memcpy(addr_bytes, &ipv4_addr, 4);
        return;
    }

    // Try IPv6
    struct in6_addr ipv6_addr;
    if (inet_pton(AF_INET6, addr_string, &ipv6_addr) == 1)
    {
        family = INET_IPV6;
        memcpy(addr_bytes, &ipv6_addr, 16);
        return;
    }

    invalid_address();
}

void InetAddr::toString(string& result) const
{
    if (!isValid())
    {
        result = "";
        return;
    }

    char buffer[INET6_ADDRSTRLEN];
    const char* str_result = nullptr;

    if (family == INET_IPV4)
    {
        str_result = inet_ntop(AF_INET, addr_bytes, buffer, INET_ADDRSTRLEN);
    }
    else if (family == INET_IPV6)
    {
        str_result = inet_ntop(AF_INET6, addr_bytes, buffer, INET6_ADDRSTRLEN);
    }

    if (str_result)
        result = str_result;
    else
        result = "";
}

string InetAddr::toString() const
{
    string result;
    toString(result);
    return result;
}

bool InetAddr::operator==(const InetAddr& other) const
{
    if (family != other.family)
        return false;
    
    ULONG size = getSize();
    return memcmp(addr_bytes, other.addr_bytes, size) == 0;
}

bool InetAddr::operator<(const InetAddr& other) const
{
    if (family != other.family)
        return family < other.family;
    
    ULONG size = getSize();
    return memcmp(addr_bytes, other.addr_bytes, size) < 0;
}

bool InetAddr::isPrivate() const
{
    if (family == INET_IPV4)
    {
        uint32_t addr = ntohl(*reinterpret_cast<const uint32_t*>(addr_bytes));
        // 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16
        return ((addr & 0xFF000000) == 0x0A000000) ||  // 10.0.0.0/8
               ((addr & 0xFFF00000) == 0xAC100000) ||  // 172.16.0.0/12
               ((addr & 0xFFFF0000) == 0xC0A80000);    // 192.168.0.0/16
    }
    else if (family == INET_IPV6)
    {
        // fd00::/8 and fe80::/10
        return (addr_bytes[0] == 0xfd) ||                    // fd00::/8
               ((addr_bytes[0] == 0xfe) && ((addr_bytes[1] & 0xc0) == 0x80)); // fe80::/10
    }
    return false;
}

bool InetAddr::isLoopback() const
{
    if (family == INET_IPV4)
    {
        return addr_bytes[0] == 127;  // 127.0.0.0/8
    }
    else if (family == INET_IPV6)
    {
        // ::1
        for (int i = 0; i < 15; i++)
            if (addr_bytes[i] != 0)
                return false;
        return addr_bytes[15] == 1;
    }
    return false;
}

ULONG InetAddr::makeIndexKey(vary* buf) const
{
    UCHAR* p = buf->vary_string;
    
    // Store family first
    *p++ = static_cast<UCHAR>(family);
    
    // Store address bytes (pad IPv4 to 16 bytes)
    if (family == INET_IPV4)
    {
        memcpy(p, addr_bytes, 4);
        memset(p + 4, 0, 12);  // Pad with zeros
    }
    else
    {
        memcpy(p, addr_bytes, 16);
    }
    
    buf->vary_length = 17;  // 1 + 16 bytes
    return 17;
}

void InetAddr::invalid_address()
{
    // Throw appropriate ScratchBird exception
    // This should integrate with ScratchBird's exception system
    throw std::invalid_argument("Invalid IP address format");
}

// CidrBlock implementation
CidrBlock::CidrBlock()
    : prefix_length(-1)
{
}

CidrBlock::CidrBlock(const char* cidr_string)
    : prefix_length(-1)
{
    parseCidr(cidr_string);
}

CidrBlock::CidrBlock(const InetAddr& addr, int prefix_len)
    : address(addr), prefix_length(prefix_len)
{
    if (prefix_length < 0 || prefix_length > getMaxPrefixLength())
        invalid_cidr();
    applyNetmask();
}

void CidrBlock::parseCidr(const char* cidr_string)
{
    if (!cidr_string)
        invalid_cidr();

    string cidr_str(cidr_string);
    size_t slash_pos = cidr_str.find('/');
    
    if (slash_pos == string::npos)
        invalid_cidr();

    string addr_part = cidr_str.substr(0, slash_pos);
    string prefix_part = cidr_str.substr(slash_pos + 1);

    // Parse address part
    address = InetAddr(addr_part.c_str());
    
    // Parse prefix length
    char* endptr;
    prefix_length = strtol(prefix_part.c_str(), &endptr, 10);
    
    if (*endptr != '\0' || prefix_length < 0 || prefix_length > getMaxPrefixLength())
        invalid_cidr();

    applyNetmask();
}

void CidrBlock::applyNetmask()
{
    // Apply network mask to ensure network address
    UCHAR* addr_bytes = const_cast<UCHAR*>(address.getBytes());
    int addr_size = address.getSize();
    int bits_to_mask = (addr_size * 8) - prefix_length;
    
    // Clear host bits
    for (int i = addr_size - 1; i >= 0 && bits_to_mask > 0; i--)
    {
        int bits_in_byte = std::min(8, bits_to_mask);
        UCHAR mask = 0xFF << bits_in_byte;
        addr_bytes[i] &= mask;
        bits_to_mask -= bits_in_byte;
    }
}

bool CidrBlock::contains(const InetAddr& addr) const
{
    if (!isValid() || !addr.isValid() || address.getFamily() != addr.getFamily())
        return false;

    int addr_size = address.getSize();
    const UCHAR* net_bytes = address.getBytes();
    const UCHAR* addr_bytes = addr.getBytes();
    
    // Compare network portion
    int bytes_to_check = prefix_length / 8;
    int remaining_bits = prefix_length % 8;
    
    // Check complete bytes
    if (memcmp(net_bytes, addr_bytes, bytes_to_check) != 0)
        return false;
    
    // Check remaining bits in partial byte
    if (remaining_bits > 0 && bytes_to_check < addr_size)
    {
        UCHAR mask = 0xFF << (8 - remaining_bits);
        if ((net_bytes[bytes_to_check] & mask) != (addr_bytes[bytes_to_check] & mask))
            return false;
    }
    
    return true;
}

void CidrBlock::toString(string& result) const
{
    if (!isValid())
    {
        result = "";
        return;
    }

    address.toString(result);
    result += "/";
    result += std::to_string(prefix_length);
}

void CidrBlock::invalid_cidr()
{
    throw std::invalid_argument("Invalid CIDR block format");
}

// MacAddr implementation
MacAddr::MacAddr()
{
    memset(mac_bytes, 0, sizeof(mac_bytes));
}

MacAddr::MacAddr(const char* mac_string)
{
    memset(mac_bytes, 0, sizeof(mac_bytes));
    parseMac(mac_string);
}

void MacAddr::parseMac(const char* mac_string)
{
    if (!mac_string)
        invalid_mac();

    // Support formats: AA:BB:CC:DD:EE:FF, AA-BB-CC-DD-EE-FF, AABBCCDDEEFF
    string mac_str(mac_string);
    
    // Remove separators
    string clean_mac;
    for (char c : mac_str)
    {
        if (isxdigit(c))
            clean_mac += c;
    }
    
    if (clean_mac.length() != 12)
        invalid_mac();
    
    // Parse hex bytes
    for (int i = 0; i < 6; i++)
    {
        string byte_str = clean_mac.substr(i * 2, 2);
        char* endptr;
        unsigned long byte_val = strtoul(byte_str.c_str(), &endptr, 16);
        
        if (*endptr != '\0' || byte_val > 255)
            invalid_mac();
        
        mac_bytes[i] = static_cast<UCHAR>(byte_val);
    }
}

void MacAddr::toString(string& result) const
{
    char buffer[18];  // AA:BB:CC:DD:EE:FF\0
    snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_bytes[0], mac_bytes[1], mac_bytes[2],
             mac_bytes[3], mac_bytes[4], mac_bytes[5]);
    result = buffer;
}

bool MacAddr::operator==(const MacAddr& other) const
{
    return memcmp(mac_bytes, other.mac_bytes, 6) == 0;
}

bool MacAddr::operator<(const MacAddr& other) const
{
    return memcmp(mac_bytes, other.mac_bytes, 6) < 0;
}

bool MacAddr::isBroadcast() const
{
    for (int i = 0; i < 6; i++)
        if (mac_bytes[i] != 0xFF)
            return false;
    return true;
}

ULONG MacAddr::makeIndexKey(vary* buf) const
{
    memcpy(buf->vary_string, mac_bytes, 6);
    buf->vary_length = 6;
    return 6;
}

void MacAddr::invalid_mac()
{
    throw std::invalid_argument("Invalid MAC address format");
}

} // namespace ScratchBird