/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/network.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <arpa/inet.h>

namespace scratchbird::core
{
    // ============================================================================
    // InetAddr Implementation
    // ============================================================================

    InetAddr::InetAddr()
        : family_(AddressFamily::IPv4), address_{}, netmask_(32)
    {
    }

    InetAddr::InetAddr(AddressFamily family, const uint8_t *addr, uint8_t netmask)
        : family_(family), address_{}, netmask_(netmask)
    {
        size_t len = (family == AddressFamily::IPv4) ? 4 : 16;
        std::memcpy(address_.data(), addr, len);
    }

    std::optional<InetAddr> InetAddr::fromString(const std::string &str)
    {
        // Parse CIDR notation: "192.168.1.0/24" or "2001:db8::/32"
        size_t slash_pos = str.find('/');
        std::string addr_str = (slash_pos != std::string::npos) ? str.substr(0, slash_pos) : str;
        uint8_t netmask = 0;

        // Check if it's IPv4 or IPv6
        if (addr_str.find(':') != std::string::npos)
        {
            // IPv6
            std::array<uint8_t, 16> addr;
            if (inet_pton(AF_INET6, addr_str.c_str(), addr.data()) != 1)
            {
                return std::nullopt;
            }

            if (slash_pos != std::string::npos)
            {
                try
                {
                    netmask = std::stoi(str.substr(slash_pos + 1));
                    if (netmask > 128)
                        return std::nullopt;
                }
                catch (...)
                {
                    return std::nullopt;
                }
            }
            else
            {
                netmask = 128;
            }

            return InetAddr::fromIPv6(addr, netmask);
        }
        else
        {
            // IPv4
            uint32_t addr;
            if (inet_pton(AF_INET, addr_str.c_str(), &addr) != 1)
            {
                return std::nullopt;
            }

            if (slash_pos != std::string::npos)
            {
                try
                {
                    netmask = std::stoi(str.substr(slash_pos + 1));
                    if (netmask > 32)
                        return std::nullopt;
                }
                catch (...)
                {
                    return std::nullopt;
                }
            }
            else
            {
                netmask = 32;
            }

            // inet_pton returns network byte order, convert to host
            return InetAddr::fromIPv4(ntohl(addr), netmask);
        }
    }

    InetAddr InetAddr::fromIPv4(uint32_t addr, uint8_t netmask)
    {
        uint8_t bytes[4] = {
            static_cast<uint8_t>((addr >> 24) & 0xFF),
            static_cast<uint8_t>((addr >> 16) & 0xFF),
            static_cast<uint8_t>((addr >> 8) & 0xFF),
            static_cast<uint8_t>(addr & 0xFF)};
        return InetAddr(AddressFamily::IPv4, bytes, netmask);
    }

    InetAddr InetAddr::fromIPv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t netmask)
    {
        uint8_t bytes[4] = {a, b, c, d};
        return InetAddr(AddressFamily::IPv4, bytes, netmask);
    }

    InetAddr InetAddr::fromIPv6(const std::array<uint8_t, 16> &addr, uint8_t netmask)
    {
        return InetAddr(AddressFamily::IPv6, addr.data(), netmask);
    }

    uint32_t InetAddr::toIPv4() const
    {
        if (family_ != AddressFamily::IPv4)
        {
            return 0;
        }
        return (static_cast<uint32_t>(address_[0]) << 24) |
               (static_cast<uint32_t>(address_[1]) << 16) |
               (static_cast<uint32_t>(address_[2]) << 8) |
               static_cast<uint32_t>(address_[3]);
    }

    std::array<uint8_t, 16> InetAddr::toIPv6() const
    {
        std::array<uint8_t, 16> result{};
        if (family_ == AddressFamily::IPv4)
        {
            // IPv4-mapped IPv6: ::ffff:192.168.1.1
            result[10] = 0xff;
            result[11] = 0xff;
            std::memcpy(&result[12], address_.data(), 4);
        }
        else
        {
            result = address_;
        }
        return result;
    }

    std::string InetAddr::toString() const
    {
        std::string addr_str = toStringWithoutNetmask();

        // Append netmask if not default
        uint8_t default_mask = isIPv4() ? 32 : 128;
        if (netmask_ != default_mask)
        {
            addr_str += "/" + std::to_string(netmask_);
        }

        return addr_str;
    }

    std::string InetAddr::toStringWithoutNetmask() const
    {
        char buf[INET6_ADDRSTRLEN];

        if (isIPv4())
        {
            uint32_t addr = toIPv4();
            uint32_t network_order = htonl(addr);
            inet_ntop(AF_INET, &network_order, buf, sizeof(buf));
        }
        else
        {
            inet_ntop(AF_INET6, address_.data(), buf, sizeof(buf));
        }

        return std::string(buf);
    }

    std::string InetAddr::toAbbreviated() const
    {
        if (isIPv4())
        {
            return toStringWithoutNetmask();
        }

        // For IPv6, inet_ntop already provides abbreviated form
        return toStringWithoutNetmask();
    }

    InetAddr InetAddr::network() const
    {
        InetAddr result = *this;
        result.applyNetmask();
        return result;
    }

    InetAddr InetAddr::broadcast() const
    {
        InetAddr result = *this;
        size_t len = isIPv4() ? 4 : 16;

        // Calculate how many host bits
        uint8_t host_bits = (isIPv4() ? 32 : 128) - netmask_;

        // Set host bits to 1
        for (size_t i = 0; i < len; ++i)
        {
            size_t bit_offset = i * 8;
            if (bit_offset >= netmask_)
            {
                // All bits in this byte are host bits
                result.address_[i] = 0xFF;
            }
            else if (bit_offset + 8 > netmask_)
            {
                // Some bits are network, some are host
                uint8_t net_bits_in_byte = netmask_ - bit_offset;
                uint8_t host_bits_in_byte = 8 - net_bits_in_byte;
                uint8_t host_mask = (1 << host_bits_in_byte) - 1;
                result.address_[i] |= host_mask;
            }
        }

        return result;
    }

    InetAddr InetAddr::netmaskAddr() const
    {
        InetAddr result;
        result.family_ = family_;
        result.netmask_ = isIPv4() ? 32 : 128;
        result.address_ = {};  // Zero initialize

        size_t len = isIPv4() ? 4 : 16;

        // Set network bits to 1, host bits to 0
        for (size_t i = 0; i < len; ++i)
        {
            size_t bit_offset = i * 8;
            if (bit_offset + 8 <= netmask_)
            {
                // All bits in this byte are network bits
                result.address_[i] = 0xFF;
            }
            else if (bit_offset < netmask_)
            {
                // Some bits are network, some are host
                uint8_t net_bits = netmask_ - bit_offset;
                result.address_[i] = static_cast<uint8_t>(0xFF << (8 - net_bits));
            }
            else
            {
                // All bits in this byte are host bits
                result.address_[i] = 0x00;
            }
        }

        return result;
    }

    InetAddr InetAddr::hostmask() const
    {
        InetAddr mask = netmaskAddr();
        return mask.bitwiseNot();
    }

    bool InetAddr::contains(const InetAddr &other) const
    {
        if (family_ != other.family_)
            return false;

        // This network must have a smaller or equal netmask (larger network)
        if (netmask_ > other.netmask_)
            return false;

        // Check if the network portions match
        InetAddr this_net = network();
        InetAddr other_net = other.network();

        size_t len = isIPv4() ? 4 : 16;
        for (size_t i = 0; i < len; ++i)
        {
            size_t bit_offset = i * 8;
            if (bit_offset >= netmask_)
                break;

            uint8_t mask;
            if (bit_offset + 8 <= netmask_)
            {
                mask = 0xFF;
            }
            else
            {
                uint8_t net_bits = netmask_ - bit_offset;
                mask = static_cast<uint8_t>(0xFF << (8 - net_bits));
            }

            if ((this_net.address_[i] & mask) != (other_net.address_[i] & mask))
            {
                return false;
            }
        }

        return true;
    }

    bool InetAddr::containedBy(const InetAddr &other) const
    {
        return other.contains(*this);
    }

    bool InetAddr::overlaps(const InetAddr &other) const
    {
        if (family_ != other.family_)
            return false;

        // Two networks overlap if they share any addresses
        return contains(other) || other.contains(*this);
    }

    bool InetAddr::strictlyLeft(const InetAddr &other) const
    {
        if (family_ != other.family_)
            return false;

        // This network's broadcast must be less than other's network
        InetAddr this_bcast = broadcast();
        InetAddr other_net = other.network();

        return this_bcast < other_net;
    }

    bool InetAddr::strictlyRight(const InetAddr &other) const
    {
        return other.strictlyLeft(*this);
    }

    bool InetAddr::sameFamily(const InetAddr &other) const
    {
        return family_ == other.family_;
    }

    InetAddr InetAddr::bitwiseAnd(const InetAddr &other) const
    {
        if (family_ != other.family_)
        {
            // Return empty address on family mismatch
            return InetAddr();
        }

        InetAddr result = *this;
        size_t len = isIPv4() ? 4 : 16;
        for (size_t i = 0; i < len; ++i)
        {
            result.address_[i] &= other.address_[i];
        }

        return result;
    }

    InetAddr InetAddr::bitwiseOr(const InetAddr &other) const
    {
        if (family_ != other.family_)
        {
            // Return empty address on family mismatch
            return InetAddr();
        }

        InetAddr result = *this;
        size_t len = isIPv4() ? 4 : 16;
        for (size_t i = 0; i < len; ++i)
        {
            result.address_[i] |= other.address_[i];
        }

        return result;
    }

    InetAddr InetAddr::bitwiseNot() const
    {
        InetAddr result = *this;
        size_t len = isIPv4() ? 4 : 16;
        for (size_t i = 0; i < len; ++i)
        {
            result.address_[i] = ~result.address_[i];
        }

        return result;
    }

    InetAddr InetAddr::operator+(int64_t offset) const
    {
        InetAddr result = *this;

        if (isIPv4())
        {
            uint32_t addr = toIPv4();
            uint64_t new_addr = static_cast<uint64_t>(addr) + offset;
            if (new_addr > 0xFFFFFFFF)
            {
                // Overflow - wrap around
                new_addr &= 0xFFFFFFFF;
            }
            return fromIPv4(static_cast<uint32_t>(new_addr), netmask_);
        }
        else
        {
            // IPv6 arithmetic (simplified - just add to last 64 bits)
            uint64_t low = 0;
            for (int i = 8; i < 16; ++i)
            {
                low = (low << 8) | result.address_[i];
            }

            low += offset;

            for (int i = 15; i >= 8; --i)
            {
                result.address_[i] = low & 0xFF;
                low >>= 8;
            }

            return result;
        }
    }

    InetAddr InetAddr::operator-(int64_t offset) const
    {
        return *this + (-offset);
    }

    int64_t InetAddr::operator-(const InetAddr &other) const
    {
        if (family_ != other.family_)
        {
            return 0;
        }

        if (isIPv4())
        {
            return static_cast<int64_t>(toIPv4()) - static_cast<int64_t>(other.toIPv4());
        }
        else
        {
            // IPv6 difference (simplified - just compare last 64 bits)
            uint64_t this_low = 0, other_low = 0;
            for (int i = 8; i < 16; ++i)
            {
                this_low = (this_low << 8) | address_[i];
                other_low = (other_low << 8) | other.address_[i];
            }

            return static_cast<int64_t>(this_low - other_low);
        }
    }

    bool InetAddr::operator==(const InetAddr &other) const
    {
        if (family_ != other.family_ || netmask_ != other.netmask_)
            return false;

        size_t len = isIPv4() ? 4 : 16;
        return std::memcmp(address_.data(), other.address_.data(), len) == 0;
    }

    bool InetAddr::operator!=(const InetAddr &other) const
    {
        return !(*this == other);
    }

    bool InetAddr::operator<(const InetAddr &other) const
    {
        // Compare family first
        if (family_ != other.family_)
        {
            return static_cast<uint8_t>(family_) < static_cast<uint8_t>(other.family_);
        }

        // Compare addresses
        size_t len = isIPv4() ? 4 : 16;
        int cmp = std::memcmp(address_.data(), other.address_.data(), len);
        if (cmp != 0)
            return cmp < 0;

        // Compare netmasks
        return netmask_ < other.netmask_;
    }

    bool InetAddr::operator<=(const InetAddr &other) const
    {
        return *this < other || *this == other;
    }

    bool InetAddr::operator>(const InetAddr &other) const
    {
        return !(*this <= other);
    }

    bool InetAddr::operator>=(const InetAddr &other) const
    {
        return !(*this < other);
    }

    void InetAddr::applyNetmask()
    {
        size_t len = isIPv4() ? 4 : 16;

        for (size_t i = 0; i < len; ++i)
        {
            size_t bit_offset = i * 8;
            if (bit_offset >= netmask_)
            {
                // All bits in this byte are host bits - zero them
                address_[i] = 0;
            }
            else if (bit_offset + 8 > netmask_)
            {
                // Some bits are network, some are host
                uint8_t net_bits = netmask_ - bit_offset;
                uint8_t mask = static_cast<uint8_t>(0xFF << (8 - net_bits));
                address_[i] &= mask;
            }
            // else: all bits in this byte are network bits - keep them
        }
    }

    int InetAddr::compareAddresses(const uint8_t *a, const uint8_t *b, size_t len)
    {
        return std::memcmp(a, b, len);
    }

    // ============================================================================
    // Cidr Implementation
    // ============================================================================

    Cidr::Cidr() : addr_() {}

    Cidr::Cidr(const InetAddr &addr) : addr_(addr)
    {
        // Ensure host bits are zeroed
        addr_ = addr_.network();
    }

    std::optional<Cidr> Cidr::fromString(const std::string &str)
    {
        // Parse CIDR notation: "192.168.1.0/24" or "2001:db8::/32"
        size_t slash_pos = str.find('/');
        std::string addr_str = (slash_pos != std::string::npos) ? str.substr(0, slash_pos) : str;
        uint8_t netmask = 0;

        // Check if it's IPv4 or IPv6
        if (addr_str.find(':') != std::string::npos)
        {
            // IPv6
            std::array<uint8_t, 16> addr;
            if (inet_pton(AF_INET6, addr_str.c_str(), addr.data()) != 1)
            {
                return std::nullopt;
            }

            if (slash_pos != std::string::npos)
            {
                try
                {
                    netmask = std::stoi(str.substr(slash_pos + 1));
                    if (netmask > 128)
                        return std::nullopt;
                }
                catch (...)
                {
                    return std::nullopt;
                }
            }
            else
            {
                netmask = 128;
            }

            // Validate host bits are zero
            for (size_t i = 0; i < 16; ++i)
            {
                size_t bit_offset = i * 8;
                if (bit_offset >= netmask)
                {
                    if (addr[i] != 0)
                        return std::nullopt; // Host bits not zero
                }
                else if (bit_offset + 8 > netmask)
                {
                    uint8_t net_bits = netmask - bit_offset;
                    uint8_t mask = static_cast<uint8_t>(0xFF << (8 - net_bits));
                    if ((addr[i] & ~mask) != 0)
                        return std::nullopt; // Host bits not zero
                }
            }

            return Cidr(InetAddr::fromIPv6(addr, netmask));
        }
        else
        {
            // IPv4
            uint32_t addr;
            if (inet_pton(AF_INET, addr_str.c_str(), &addr) != 1)
            {
                return std::nullopt;
            }

            if (slash_pos != std::string::npos)
            {
                try
                {
                    netmask = std::stoi(str.substr(slash_pos + 1));
                    if (netmask > 32)
                        return std::nullopt;
                }
                catch (...)
                {
                    return std::nullopt;
                }
            }
            else
            {
                netmask = 32;
            }

            // inet_pton returns network byte order, convert to host
            uint32_t host_addr = ntohl(addr);

            // Validate host bits are zero
            if (netmask < 32)
            {
                uint32_t host_mask = (1u << (32 - netmask)) - 1;
                if ((host_addr & host_mask) != 0)
                    return std::nullopt; // Host bits not zero
            }

            return Cidr(InetAddr::fromIPv4(host_addr, netmask));
        }
    }

    Cidr Cidr::fromIPv4(uint32_t addr, uint8_t netmask)
    {
        return Cidr(InetAddr::fromIPv4(addr, netmask));
    }

    Cidr Cidr::fromIPv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t netmask)
    {
        return Cidr(InetAddr::fromIPv4(a, b, c, d, netmask));
    }

    Cidr Cidr::fromIPv6(const std::array<uint8_t, 16> &addr, uint8_t netmask)
    {
        return Cidr(InetAddr::fromIPv6(addr, netmask));
    }

    Cidr Cidr::fromInet(const InetAddr &addr)
    {
        return Cidr(addr);
    }

    // ============================================================================
    // MacAddr Implementation
    // ============================================================================

    MacAddr::MacAddr() : bytes_{} {}

    MacAddr::MacAddr(const std::array<uint8_t, 6> &bytes) : bytes_(bytes) {}

    MacAddr::MacAddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f)
        : bytes_{a, b, c, d, e, f}
    {
    }

    std::optional<MacAddr> MacAddr::fromString(const std::string &str)
    {
        std::array<uint8_t, 6> bytes;

        // Try colon-separated format: 08:00:2b:01:02:03
        if (str.find(':') != std::string::npos)
        {
            std::istringstream iss(str);
            std::string byte_str;
            size_t index = 0;

            while (std::getline(iss, byte_str, ':') && index < 6)
            {
                try
                {
                    bytes[index++] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
                }
                catch (...)
                {
                    return std::nullopt;
                }
            }

            if (index == 6)
            {
                return MacAddr(bytes);
            }
        }

        // Try hyphen-separated format: 08-00-2b-01-02-03
        if (str.find('-') != std::string::npos)
        {
            std::istringstream iss(str);
            std::string byte_str;
            size_t index = 0;

            while (std::getline(iss, byte_str, '-') && index < 6)
            {
                try
                {
                    bytes[index++] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
                }
                catch (...)
                {
                    return std::nullopt;
                }
            }

            if (index == 6)
            {
                return MacAddr(bytes);
            }
        }

        // Try Cisco format: 0800.2b01.0203
        if (str.find('.') != std::string::npos)
        {
            std::istringstream iss(str);
            std::string group_str;
            size_t index = 0;

            while (std::getline(iss, group_str, '.') && index < 3)
            {
                if (group_str.length() != 4)
                    return std::nullopt;

                try
                {
                    uint16_t group = static_cast<uint16_t>(std::stoi(group_str, nullptr, 16));
                    bytes[index * 2] = (group >> 8) & 0xFF;
                    bytes[index * 2 + 1] = group & 0xFF;
                    index++;
                }
                catch (...)
                {
                    return std::nullopt;
                }
            }

            if (index == 3)
            {
                return MacAddr(bytes);
            }
        }

        // Try bare format: 08002b010203
        if (str.length() == 12 && str.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos)
        {
            try
            {
                for (size_t i = 0; i < 6; ++i)
                {
                    bytes[i] = static_cast<uint8_t>(std::stoi(str.substr(i * 2, 2), nullptr, 16));
                }
                return MacAddr(bytes);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        return std::nullopt;
    }

    MacAddr MacAddr::fromBytes(const uint8_t *data)
    {
        std::array<uint8_t, 6> bytes;
        std::memcpy(bytes.data(), data, 6);
        return MacAddr(bytes);
    }

    std::string MacAddr::toString() const
    {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < 6; ++i)
        {
            if (i > 0)
                oss << ':';
            oss << std::setw(2) << static_cast<int>(bytes_[i]);
        }
        return oss.str();
    }

    std::string MacAddr::toStringHyphen() const
    {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < 6; ++i)
        {
            if (i > 0)
                oss << '-';
            oss << std::setw(2) << static_cast<int>(bytes_[i]);
        }
        return oss.str();
    }

    std::string MacAddr::toStringCisco() const
    {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < 3; ++i)
        {
            if (i > 0)
                oss << '.';
            oss << std::setw(2) << static_cast<int>(bytes_[i * 2]);
            oss << std::setw(2) << static_cast<int>(bytes_[i * 2 + 1]);
        }
        return oss.str();
    }

    MacAddr MacAddr::bitwiseAnd(const MacAddr &other) const
    {
        std::array<uint8_t, 6> result;
        for (size_t i = 0; i < 6; ++i)
        {
            result[i] = bytes_[i] & other.bytes_[i];
        }
        return MacAddr(result);
    }

    MacAddr MacAddr::bitwiseOr(const MacAddr &other) const
    {
        std::array<uint8_t, 6> result;
        for (size_t i = 0; i < 6; ++i)
        {
            result[i] = bytes_[i] | other.bytes_[i];
        }
        return MacAddr(result);
    }

    MacAddr MacAddr::bitwiseNot() const
    {
        std::array<uint8_t, 6> result;
        for (size_t i = 0; i < 6; ++i)
        {
            result[i] = ~bytes_[i];
        }
        return MacAddr(result);
    }

    MacAddr MacAddr::trunc() const
    {
        std::array<uint8_t, 6> result = bytes_;
        result[3] = result[4] = result[5] = 0;
        return MacAddr(result);
    }

    bool MacAddr::operator==(const MacAddr &other) const
    {
        return bytes_ == other.bytes_;
    }

    bool MacAddr::operator!=(const MacAddr &other) const
    {
        return bytes_ != other.bytes_;
    }

    bool MacAddr::operator<(const MacAddr &other) const
    {
        return bytes_ < other.bytes_;
    }

    bool MacAddr::operator<=(const MacAddr &other) const
    {
        return bytes_ <= other.bytes_;
    }

    bool MacAddr::operator>(const MacAddr &other) const
    {
        return bytes_ > other.bytes_;
    }

    bool MacAddr::operator>=(const MacAddr &other) const
    {
        return bytes_ >= other.bytes_;
    }

    // ============================================================================
    // MacAddr8 Implementation
    // ============================================================================

    MacAddr8::MacAddr8() : bytes_{} {}

    MacAddr8::MacAddr8(const std::array<uint8_t, 8> &bytes) : bytes_(bytes) {}

    MacAddr8::MacAddr8(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                       uint8_t e, uint8_t f, uint8_t g, uint8_t h)
        : bytes_{a, b, c, d, e, f, g, h}
    {
    }

    std::optional<MacAddr8> MacAddr8::fromString(const std::string &str)
    {
        std::array<uint8_t, 8> bytes;

        // Try colon-separated format: 08:00:2b:01:02:03:04:05
        if (str.find(':') != std::string::npos)
        {
            std::istringstream iss(str);
            std::string byte_str;
            size_t index = 0;

            while (std::getline(iss, byte_str, ':') && index < 8)
            {
                try
                {
                    bytes[index++] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
                }
                catch (...)
                {
                    return std::nullopt;
                }
            }

            if (index == 8)
            {
                return MacAddr8(bytes);
            }
        }

        // Try hyphen-separated format: 08-00-2b-01-02-03-04-05
        if (str.find('-') != std::string::npos)
        {
            std::istringstream iss(str);
            std::string byte_str;
            size_t index = 0;

            while (std::getline(iss, byte_str, '-') && index < 8)
            {
                try
                {
                    bytes[index++] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
                }
                catch (...)
                {
                    return std::nullopt;
                }
            }

            if (index == 8)
            {
                return MacAddr8(bytes);
            }
        }

        // Try bare format: 08002b0102030405
        if (str.length() == 16 && str.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos)
        {
            try
            {
                for (size_t i = 0; i < 8; ++i)
                {
                    bytes[i] = static_cast<uint8_t>(std::stoi(str.substr(i * 2, 2), nullptr, 16));
                }
                return MacAddr8(bytes);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        return std::nullopt;
    }

    MacAddr8 MacAddr8::fromBytes(const uint8_t *data)
    {
        std::array<uint8_t, 8> bytes;
        std::memcpy(bytes.data(), data, 8);
        return MacAddr8(bytes);
    }

    MacAddr8 MacAddr8::fromMacAddr(const MacAddr &mac)
    {
        // EUI-48 to EUI-64 conversion: insert 0xfffe in the middle
        // 08:00:2b:01:02:03 -> 08:00:2b:ff:fe:01:02:03
        std::array<uint8_t, 8> bytes;
        const auto &mac_bytes = mac.bytes();

        bytes[0] = mac_bytes[0];
        bytes[1] = mac_bytes[1];
        bytes[2] = mac_bytes[2];
        bytes[3] = 0xff;
        bytes[4] = 0xfe;
        bytes[5] = mac_bytes[3];
        bytes[6] = mac_bytes[4];
        bytes[7] = mac_bytes[5];

        return MacAddr8(bytes);
    }

    std::string MacAddr8::toString() const
    {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < 8; ++i)
        {
            if (i > 0)
                oss << ':';
            oss << std::setw(2) << static_cast<int>(bytes_[i]);
        }
        return oss.str();
    }

    std::string MacAddr8::toStringHyphen() const
    {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < 8; ++i)
        {
            if (i > 0)
                oss << '-';
            oss << std::setw(2) << static_cast<int>(bytes_[i]);
        }
        return oss.str();
    }

    MacAddr8 MacAddr8::bitwiseAnd(const MacAddr8 &other) const
    {
        std::array<uint8_t, 8> result;
        for (size_t i = 0; i < 8; ++i)
        {
            result[i] = bytes_[i] & other.bytes_[i];
        }
        return MacAddr8(result);
    }

    MacAddr8 MacAddr8::bitwiseOr(const MacAddr8 &other) const
    {
        std::array<uint8_t, 8> result;
        for (size_t i = 0; i < 8; ++i)
        {
            result[i] = bytes_[i] | other.bytes_[i];
        }
        return MacAddr8(result);
    }

    MacAddr8 MacAddr8::bitwiseNot() const
    {
        std::array<uint8_t, 8> result;
        for (size_t i = 0; i < 8; ++i)
        {
            result[i] = ~bytes_[i];
        }
        return MacAddr8(result);
    }

    MacAddr8 MacAddr8::trunc() const
    {
        std::array<uint8_t, 8> result = bytes_;
        result[3] = result[4] = result[5] = result[6] = result[7] = 0;
        return MacAddr8(result);
    }

    bool MacAddr8::operator==(const MacAddr8 &other) const
    {
        return bytes_ == other.bytes_;
    }

    bool MacAddr8::operator!=(const MacAddr8 &other) const
    {
        return bytes_ != other.bytes_;
    }

    bool MacAddr8::operator<(const MacAddr8 &other) const
    {
        return bytes_ < other.bytes_;
    }

    bool MacAddr8::operator<=(const MacAddr8 &other) const
    {
        return bytes_ <= other.bytes_;
    }

    bool MacAddr8::operator>(const MacAddr8 &other) const
    {
        return bytes_ > other.bytes_;
    }

    bool MacAddr8::operator>=(const MacAddr8 &other) const
    {
        return bytes_ >= other.bytes_;
    }

    // ============================================================================
    // Network Utility Functions
    // ============================================================================

    namespace network_utils
    {
        InetAddr inet_merge(const InetAddr &a, const InetAddr &b)
        {
            if (!inet_same_family(a, b))
            {
                // Can't merge different families
                return InetAddr();
            }

            // Find the smallest network that contains both addresses
            InetAddr a_net = a.network();
            InetAddr b_net = b.network();

            // Start with the smaller netmask (larger network)
            uint8_t min_netmask = std::min(a.netmask(), b.netmask());

            // Try progressively smaller netmasks until we find one that contains both
            for (uint8_t netmask = min_netmask; netmask > 0; --netmask)
            {
                InetAddr candidate = a.isIPv4() ? InetAddr::fromIPv4(a_net.toIPv4(), netmask)
                                                : InetAddr::fromIPv6(a_net.toIPv6(), netmask);

                if (candidate.contains(a) && candidate.contains(b))
                {
                    return candidate;
                }
            }

            // If we get here, use the entire address space
            return a.isIPv4() ? InetAddr::fromIPv4(0, 0) : InetAddr::fromIPv6(std::array<uint8_t, 16>{}, 0);
        }

        bool inet_same_family(const InetAddr &a, const InetAddr &b)
        {
            return a.sameFamily(b);
        }

        MacAddr8 macaddr8_set7bit(const MacAddr8 &addr)
        {
            // Set the 7th bit (universal/local bit) in the first byte
            std::array<uint8_t, 8> bytes = addr.bytes();
            bytes[0] |= 0x02;
            return MacAddr8(bytes);
        }
    }

} // namespace scratchbird::core
