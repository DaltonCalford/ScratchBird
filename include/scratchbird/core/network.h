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

#include <cstdint>
#include <array>
#include <string>
#include <optional>
#include <cstring>

/**
 * Network Types for ScratchBird
 * Task 16: PostgreSQL-compatible network data types
 *
 * Implements:
 * - INET: IPv4/IPv6 addresses with optional netmask
 * - CIDR: IPv4/IPv6 networks in CIDR notation
 * - MACADDR: 6-byte MAC addresses (EUI-48)
 * - MACADDR8: 8-byte MAC addresses (EUI-64)
 */

namespace scratchbird::core
{
    /**
     * IP address family enumeration
     */
    enum class AddressFamily : uint8_t
    {
        IPv4 = 4,  // AF_INET
        IPv6 = 6   // AF_INET6
    };

    /**
     * INET type - Represents an IPv4 or IPv6 address with optional netmask
     *
     * Storage:
     * - IPv4: 4 bytes address + 1 byte netmask (0-32)
     * - IPv6: 16 bytes address + 1 byte netmask (0-128)
     *
     * Examples:
     * - 192.168.1.1
     * - 192.168.1.0/24
     * - 2001:db8::1
     * - 2001:db8::/32
     *
     * Total size: 18 bytes (16 bytes max address + 1 byte family + 1 byte netmask)
     */
    class InetAddr
    {
    public:
        // Constructors
        InetAddr();
        InetAddr(AddressFamily family, const uint8_t *addr, uint8_t netmask);

        // Static factory methods
        static std::optional<InetAddr> fromString(const std::string &str);
        static InetAddr fromIPv4(uint32_t addr, uint8_t netmask = 32);
        static InetAddr fromIPv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t netmask = 32);
        static InetAddr fromIPv6(const std::array<uint8_t, 16> &addr, uint8_t netmask = 128);

        // Accessors
        AddressFamily family() const { return family_; }
        uint8_t netmask() const { return netmask_; }
        bool isIPv4() const { return family_ == AddressFamily::IPv4; }
        bool isIPv6() const { return family_ == AddressFamily::IPv6; }

        // Get raw address bytes
        const uint8_t *data() const { return address_.data(); }
        size_t size() const { return isIPv4() ? 4 : 16; }

        // Get address as uint32_t (IPv4 only)
        uint32_t toIPv4() const;

        // Get address as array (IPv6 or IPv4 mapped to IPv6)
        std::array<uint8_t, 16> toIPv6() const;

        // String representation
        std::string toString() const;
        std::string toStringWithoutNetmask() const;
        std::string toAbbreviated() const; // Abbreviated form (e.g., ::1 instead of 0:0:0:0:0:0:0:1)

        // Network operations
        InetAddr network() const;      // Get network address (host bits zeroed)
        InetAddr broadcast() const;    // Get broadcast address (host bits set to 1)
        InetAddr netmaskAddr() const;  // Get netmask as address (e.g., 255.255.255.0)
        InetAddr hostmask() const;     // Get hostmask (inverse of netmask)

        // Boolean operations
        bool contains(const InetAddr &other) const;          // Does this network contain the other address?
        bool containedBy(const InetAddr &other) const;       // Is this address contained by the other network?
        bool overlaps(const InetAddr &other) const;          // Do these networks overlap?
        bool strictlyLeft(const InetAddr &other) const;      // Is this network strictly left of other?
        bool strictlyRight(const InetAddr &other) const;     // Is this network strictly right of other?
        bool sameFamily(const InetAddr &other) const;        // Same address family?

        // Bitwise operations
        InetAddr bitwiseAnd(const InetAddr &other) const;    // Bitwise AND
        InetAddr bitwiseOr(const InetAddr &other) const;     // Bitwise OR
        InetAddr bitwiseNot() const;                         // Bitwise NOT

        // Arithmetic operations
        InetAddr operator+(int64_t offset) const;            // Add offset to address
        InetAddr operator-(int64_t offset) const;            // Subtract offset from address
        int64_t operator-(const InetAddr &other) const;      // Difference between addresses

        // Comparison operators
        bool operator==(const InetAddr &other) const;
        bool operator!=(const InetAddr &other) const;
        bool operator<(const InetAddr &other) const;
        bool operator<=(const InetAddr &other) const;
        bool operator>(const InetAddr &other) const;
        bool operator>=(const InetAddr &other) const;

    private:
        AddressFamily family_;
        std::array<uint8_t, 16> address_;  // IPv6 max size (IPv4 uses first 4 bytes)
        uint8_t netmask_;                  // 0-32 for IPv4, 0-128 for IPv6

        // Helper methods
        void applyNetmask();
        static int compareAddresses(const uint8_t *a, const uint8_t *b, size_t len);
    };

    /**
     * CIDR type - Represents an IPv4 or IPv6 network in CIDR notation
     *
     * Similar to INET, but enforces that host bits are zero (strict CIDR).
     * Rejects addresses like 192.168.1.5/24 (should be 192.168.1.0/24).
     *
     * Examples:
     * - 192.168.1.0/24  ✓
     * - 192.168.1.5/24  ✗ (host bits not zero)
     * - 2001:db8::/32   ✓
     * - 2001:db8::1/32  ✗ (host bits not zero)
     */
    class Cidr
    {
    public:
        // Constructors
        Cidr();
        Cidr(const InetAddr &addr);  // Converts INET to CIDR (zeroes host bits)

        // Static factory methods
        static std::optional<Cidr> fromString(const std::string &str);
        static Cidr fromIPv4(uint32_t addr, uint8_t netmask);
        static Cidr fromIPv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t netmask);
        static Cidr fromIPv6(const std::array<uint8_t, 16> &addr, uint8_t netmask);

        // Convert to/from INET
        InetAddr toInet() const { return addr_; }
        static Cidr fromInet(const InetAddr &addr);

        // Accessors (delegate to InetAddr)
        AddressFamily family() const { return addr_.family(); }
        uint8_t netmask() const { return addr_.netmask(); }
        bool isIPv4() const { return addr_.isIPv4(); }
        bool isIPv6() const { return addr_.isIPv6(); }

        // String representation
        std::string toString() const { return addr_.toString(); }

        // Network operations (delegate to InetAddr)
        Cidr broadcast() const { return Cidr(addr_.broadcast()); }
        Cidr netmaskAddr() const { return Cidr(addr_.netmaskAddr()); }
        Cidr hostmask() const { return Cidr(addr_.hostmask()); }

        // Boolean operations
        bool contains(const InetAddr &other) const { return addr_.contains(other); }
        bool contains(const Cidr &other) const { return addr_.contains(other.addr_); }
        bool containedBy(const Cidr &other) const { return addr_.containedBy(other.addr_); }
        bool overlaps(const Cidr &other) const { return addr_.overlaps(other.addr_); }

        // Comparison operators
        bool operator==(const Cidr &other) const { return addr_ == other.addr_; }
        bool operator!=(const Cidr &other) const { return addr_ != other.addr_; }
        bool operator<(const Cidr &other) const { return addr_ < other.addr_; }
        bool operator<=(const Cidr &other) const { return addr_ <= other.addr_; }
        bool operator>(const Cidr &other) const { return addr_ > other.addr_; }
        bool operator>=(const Cidr &other) const { return addr_ >= other.addr_; }

    private:
        InetAddr addr_;  // Always has host bits zeroed
    };

    /**
     * MACADDR type - Represents a 6-byte MAC address (EUI-48)
     *
     * Storage: 6 bytes
     *
     * Examples:
     * - 08:00:2b:01:02:03
     * - 08-00-2b-01-02-03
     * - 08002b:010203
     * - 08002b010203
     *
     * Formats supported:
     * - Colon-separated: 08:00:2b:01:02:03
     * - Hyphen-separated: 08-00-2b-01-02-03
     * - Cisco format: 0800.2b01.0203
     */
    class MacAddr
    {
    public:
        // Constructors
        MacAddr();
        MacAddr(const std::array<uint8_t, 6> &bytes);
        MacAddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f);

        // Static factory methods
        static std::optional<MacAddr> fromString(const std::string &str);
        static MacAddr fromBytes(const uint8_t *data);

        // Accessors
        const std::array<uint8_t, 6> &bytes() const { return bytes_; }
        uint8_t operator[](size_t index) const { return bytes_[index]; }

        // String representation
        std::string toString() const;  // Default: colon-separated
        std::string toStringHyphen() const;  // Hyphen-separated
        std::string toStringCisco() const;   // Cisco format (0800.2b01.0203)

        // Bitwise operations
        MacAddr bitwiseAnd(const MacAddr &other) const;
        MacAddr bitwiseOr(const MacAddr &other) const;
        MacAddr bitwiseNot() const;

        // Truncation/extension
        MacAddr trunc() const;  // Truncate to manufacturer ID (first 3 bytes)

        // Comparison operators
        bool operator==(const MacAddr &other) const;
        bool operator!=(const MacAddr &other) const;
        bool operator<(const MacAddr &other) const;
        bool operator<=(const MacAddr &other) const;
        bool operator>(const MacAddr &other) const;
        bool operator>=(const MacAddr &other) const;

    private:
        std::array<uint8_t, 6> bytes_;
    };

    /**
     * MACADDR8 type - Represents an 8-byte MAC address (EUI-64)
     *
     * Storage: 8 bytes
     *
     * Examples:
     * - 08:00:2b:01:02:03:04:05
     * - 08-00-2b-01-02-03-04-05
     *
     * EUI-64 addresses are used in IPv6 link-local addresses and some
     * modern network interfaces.
     */
    class MacAddr8
    {
    public:
        // Constructors
        MacAddr8();
        MacAddr8(const std::array<uint8_t, 8> &bytes);
        MacAddr8(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                 uint8_t e, uint8_t f, uint8_t g, uint8_t h);

        // Static factory methods
        static std::optional<MacAddr8> fromString(const std::string &str);
        static MacAddr8 fromBytes(const uint8_t *data);
        static MacAddr8 fromMacAddr(const MacAddr &mac);  // Convert EUI-48 to EUI-64

        // Accessors
        const std::array<uint8_t, 8> &bytes() const { return bytes_; }
        uint8_t operator[](size_t index) const { return bytes_[index]; }

        // String representation
        std::string toString() const;  // Default: colon-separated
        std::string toStringHyphen() const;  // Hyphen-separated

        // Bitwise operations
        MacAddr8 bitwiseAnd(const MacAddr8 &other) const;
        MacAddr8 bitwiseOr(const MacAddr8 &other) const;
        MacAddr8 bitwiseNot() const;

        // Truncation
        MacAddr8 trunc() const;  // Truncate to manufacturer ID (first 3 bytes)

        // Comparison operators
        bool operator==(const MacAddr8 &other) const;
        bool operator!=(const MacAddr8 &other) const;
        bool operator<(const MacAddr8 &other) const;
        bool operator<=(const MacAddr8 &other) const;
        bool operator>(const MacAddr8 &other) const;
        bool operator>=(const MacAddr8 &other) const;

    private:
        std::array<uint8_t, 8> bytes_;
    };

    /**
     * Network utility functions
     */
    namespace network_utils
    {
        // INET functions
        InetAddr inet_merge(const InetAddr &a, const InetAddr &b);  // Smallest network containing both
        bool inet_same_family(const InetAddr &a, const InetAddr &b);

        // MAC address functions
        MacAddr8 macaddr8_set7bit(const MacAddr8 &addr);  // Set 7th bit (used for IPv6 link-local)
    }

} // namespace scratchbird::core
