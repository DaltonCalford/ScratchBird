/**
 * Unit tests for Network Types
 * Task 16: PostgreSQL-compatible network data types
 *
 * Tests:
 * - INET: IPv4/IPv6 addresses with netmasks
 * - CIDR: Strict network addresses
 * - MACADDR: 6-byte MAC addresses
 * - MACADDR8: 8-byte MAC addresses
 * - Network operators and functions
 */

#include "scratchbird/core/network.h"
#include "scratchbird/core/types.h"
#include <iostream>
#include <cassert>

using namespace scratchbird::core;

// Test harness
int g_tests_passed = 0;
int g_tests_failed = 0;

#define test_assert(condition, message)                                                            \
    do                                                                                             \
    {                                                                                              \
        if (condition)                                                                             \
        {                                                                                          \
            std::cout << "[PASS] " << message << std::endl;                                       \
            g_tests_passed++;                                                                      \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            std::cout << "[FAIL] " << message << std::endl;                                       \
            g_tests_failed++;                                                                      \
        }                                                                                          \
    } while (0)

// ============================================================================
// INET Type Tests
// ============================================================================

void test_inet_ipv4_basic()
{
    std::cout << "\n=== test_inet_ipv4_basic ===" << std::endl;

    // Create IPv4 address
    auto addr = InetAddr::fromIPv4(192, 168, 1, 1);
    test_assert(addr.isIPv4(), "Address is IPv4");
    test_assert(!addr.isIPv6(), "Address is not IPv6");
    test_assert(addr.netmask() == 32, "Default netmask is 32");
    test_assert(addr.toString() == "192.168.1.1", "String representation is correct");
}

void test_inet_ipv4_with_netmask()
{
    std::cout << "\n=== test_inet_ipv4_with_netmask ===" << std::endl;

    auto addr = InetAddr::fromIPv4(192, 168, 1, 0, 24);
    test_assert(addr.netmask() == 24, "Netmask is 24");
    test_assert(addr.toString() == "192.168.1.0/24", "String includes netmask");
}

void test_inet_ipv4_parsing()
{
    std::cout << "\n=== test_inet_ipv4_parsing ===" << std::endl;

    // Parse simple address
    {
        auto addr = InetAddr::fromString("192.168.1.1");
        test_assert(addr.has_value(), "Parsed simple IPv4");
        test_assert(addr->toString() == "192.168.1.1", "Parsed address is correct");
    }

    // Parse CIDR notation
    {
        auto addr = InetAddr::fromString("10.0.0.0/8");
        test_assert(addr.has_value(), "Parsed CIDR notation");
        test_assert(addr->netmask() == 8, "Netmask parsed correctly");
        test_assert(addr->toString() == "10.0.0.0/8", "CIDR string is correct");
    }

    // Invalid address
    {
        auto addr = InetAddr::fromString("256.0.0.1");
        test_assert(!addr.has_value(), "Rejected invalid IPv4");
    }
}

void test_inet_ipv6_basic()
{
    std::cout << "\n=== test_inet_ipv6_basic ===" << std::endl;

    auto addr = InetAddr::fromString("2001:db8::1");
    test_assert(addr.has_value(), "Parsed IPv6 address");
    test_assert(addr->isIPv6(), "Address is IPv6");
    test_assert(!addr->isIPv4(), "Address is not IPv4");
    test_assert(addr->netmask() == 128, "Default netmask is 128");
}

void test_inet_ipv6_with_netmask()
{
    std::cout << "\n=== test_inet_ipv6_with_netmask ===" << std::endl;

    auto addr = InetAddr::fromString("2001:db8::/32");
    test_assert(addr.has_value(), "Parsed IPv6 CIDR");
    test_assert(addr->netmask() == 32, "Netmask is 32");
}

void test_inet_network_operations()
{
    std::cout << "\n=== test_inet_network_operations ===" << std::endl;

    auto addr = InetAddr::fromIPv4(192, 168, 1, 100, 24);

    // Network address
    auto net = addr.network();
    test_assert(net.toString() == "192.168.1.0/24", "Network address is correct");

    // Broadcast address
    auto bcast = addr.broadcast();
    test_assert(bcast.toStringWithoutNetmask() == "192.168.1.255", "Broadcast address is correct");

    // Netmask
    auto netmask = addr.netmaskAddr();
    test_assert(netmask.toStringWithoutNetmask() == "255.255.255.0", "Netmask is correct");

    // Hostmask
    auto hostmask = addr.hostmask();
    test_assert(hostmask.toStringWithoutNetmask() == "0.0.0.255", "Hostmask is correct");
}

void test_inet_containment()
{
    std::cout << "\n=== test_inet_containment ===" << std::endl;

    auto network = InetAddr::fromIPv4(192, 168, 1, 0, 24);
    auto addr1 = InetAddr::fromIPv4(192, 168, 1, 100);
    auto addr2 = InetAddr::fromIPv4(192, 168, 2, 1);

    test_assert(network.contains(addr1), "Network contains addr1");
    test_assert(!network.contains(addr2), "Network does not contain addr2");
    test_assert(addr1.containedBy(network), "addr1 is contained by network");
    test_assert(!addr2.containedBy(network), "addr2 is not contained by network");
}

void test_inet_comparison()
{
    std::cout << "\n=== test_inet_comparison ===" << std::endl;

    auto addr1 = InetAddr::fromIPv4(192, 168, 1, 1);
    auto addr2 = InetAddr::fromIPv4(192, 168, 1, 1);
    auto addr3 = InetAddr::fromIPv4(192, 168, 1, 2);

    test_assert(addr1 == addr2, "Equal addresses are equal");
    test_assert(addr1 != addr3, "Different addresses are not equal");
    test_assert(addr1 < addr3, "Comparison works correctly");
}

void test_inet_bitwise_operations()
{
    std::cout << "\n=== test_inet_bitwise_operations ===" << std::endl;

    auto addr1 = InetAddr::fromIPv4(192, 168, 1, 255);
    auto addr2 = InetAddr::fromIPv4(255, 255, 255, 0);

    // Bitwise AND
    auto and_result = addr1.bitwiseAnd(addr2);
    test_assert(and_result.toStringWithoutNetmask() == "192.168.1.0", "Bitwise AND works");

    // Bitwise OR
    auto or_result = addr1.bitwiseOr(addr2);
    test_assert(or_result.toStringWithoutNetmask() == "255.255.255.255", "Bitwise OR works");

    // Bitwise NOT
    auto not_result = InetAddr::fromIPv4(192, 168, 1, 1).bitwiseNot();
    test_assert(not_result.toStringWithoutNetmask() == "63.87.254.254", "Bitwise NOT works");
}

void test_inet_arithmetic()
{
    std::cout << "\n=== test_inet_arithmetic ===" << std::endl;

    auto addr = InetAddr::fromIPv4(192, 168, 1, 1);

    // Addition
    auto addr_plus_1 = addr + 1;
    test_assert(addr_plus_1.toStringWithoutNetmask() == "192.168.1.2", "Addition works");

    // Subtraction
    auto addr_minus_1 = addr - 1;
    test_assert(addr_minus_1.toStringWithoutNetmask() == "192.168.1.0", "Subtraction works");

    // Difference
    auto addr2 = InetAddr::fromIPv4(192, 168, 1, 10);
    int64_t diff = addr2 - addr;
    test_assert(diff == 9, "Difference calculation works");
}

// ============================================================================
// CIDR Type Tests
// ============================================================================

void test_cidr_basic()
{
    std::cout << "\n=== test_cidr_basic ===" << std::endl;

    auto cidr = Cidr::fromIPv4(192, 168, 1, 0, 24);
    test_assert(cidr.toString() == "192.168.1.0/24", "CIDR string is correct");
}

void test_cidr_strict_validation()
{
    std::cout << "\n=== test_cidr_strict_validation ===" << std::endl;

    // Valid CIDR (host bits are zero)
    {
        auto cidr = Cidr::fromString("192.168.1.0/24");
        test_assert(cidr.has_value(), "Accepted valid CIDR");
    }

    // Invalid CIDR (host bits not zero)
    {
        auto cidr = Cidr::fromString("192.168.1.5/24");
        test_assert(!cidr.has_value(), "Rejected invalid CIDR with host bits");
    }
}

void test_cidr_operations()
{
    std::cout << "\n=== test_cidr_operations ===" << std::endl;

    auto cidr1 = Cidr::fromIPv4(10, 0, 0, 0, 8);
    auto cidr2 = Cidr::fromIPv4(10, 1, 0, 0, 16);
    auto addr = InetAddr::fromIPv4(10, 1, 2, 3);

    test_assert(cidr1.contains(cidr2), "Larger CIDR contains smaller CIDR");
    test_assert(cidr1.contains(addr), "CIDR contains address");
    test_assert(cidr2.containedBy(cidr1), "Smaller CIDR is contained by larger");
}

// ============================================================================
// MACADDR Type Tests
// ============================================================================

void test_macaddr_basic()
{
    std::cout << "\n=== test_macaddr_basic ===" << std::endl;

    auto mac = MacAddr(0x08, 0x00, 0x2b, 0x01, 0x02, 0x03);
    test_assert(mac.toString() == "08:00:2b:01:02:03", "MAC address string is correct");
}

void test_macaddr_parsing()
{
    std::cout << "\n=== test_macaddr_parsing ===" << std::endl;

    // Colon-separated
    {
        auto mac = MacAddr::fromString("08:00:2b:01:02:03");
        test_assert(mac.has_value(), "Parsed colon-separated MAC");
        test_assert(mac->toString() == "08:00:2b:01:02:03", "Parsed MAC is correct");
    }

    // Hyphen-separated
    {
        auto mac = MacAddr::fromString("08-00-2b-01-02-03");
        test_assert(mac.has_value(), "Parsed hyphen-separated MAC");
        test_assert(mac->toStringHyphen() == "08-00-2b-01-02-03", "Hyphen format is correct");
    }

    // Cisco format
    {
        auto mac = MacAddr::fromString("0800.2b01.0203");
        test_assert(mac.has_value(), "Parsed Cisco format MAC");
        test_assert(mac->toStringCisco() == "0800.2b01.0203", "Cisco format is correct");
    }

    // Bare format
    {
        auto mac = MacAddr::fromString("08002b010203");
        test_assert(mac.has_value(), "Parsed bare format MAC");
    }
}

void test_macaddr_operations()
{
    std::cout << "\n=== test_macaddr_operations ===" << std::endl;

    auto mac1 = MacAddr(0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
    auto mac2 = MacAddr(0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0);

    // Bitwise AND
    auto and_result = mac1.bitwiseAnd(mac2);
    test_assert(and_result.toString() == "f0:f0:f0:f0:f0:f0", "MAC AND works");

    // Bitwise OR
    auto mac3 = MacAddr(0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f);
    auto or_result = mac2.bitwiseOr(mac3);
    test_assert(or_result.toString() == "ff:ff:ff:ff:ff:ff", "MAC OR works");

    // Bitwise NOT
    auto not_result = MacAddr(0x00, 0x00, 0x00, 0x00, 0x00, 0x00).bitwiseNot();
    test_assert(not_result.toString() == "ff:ff:ff:ff:ff:ff", "MAC NOT works");
}

void test_macaddr_trunc()
{
    std::cout << "\n=== test_macaddr_trunc ===" << std::endl;

    auto mac = MacAddr(0x08, 0x00, 0x2b, 0x01, 0x02, 0x03);
    auto truncated = mac.trunc();
    test_assert(truncated.toString() == "08:00:2b:00:00:00", "MAC truncation works");
}

void test_macaddr_comparison()
{
    std::cout << "\n=== test_macaddr_comparison ===" << std::endl;

    auto mac1 = MacAddr(0x08, 0x00, 0x2b, 0x01, 0x02, 0x03);
    auto mac2 = MacAddr(0x08, 0x00, 0x2b, 0x01, 0x02, 0x03);
    auto mac3 = MacAddr(0x08, 0x00, 0x2b, 0x01, 0x02, 0x04);

    test_assert(mac1 == mac2, "Equal MACs are equal");
    test_assert(mac1 != mac3, "Different MACs are not equal");
    test_assert(mac1 < mac3, "MAC comparison works");
}

// ============================================================================
// MACADDR8 Type Tests
// ============================================================================

void test_macaddr8_basic()
{
    std::cout << "\n=== test_macaddr8_basic ===" << std::endl;

    auto mac8 = MacAddr8(0x08, 0x00, 0x2b, 0x01, 0x02, 0x03, 0x04, 0x05);
    test_assert(mac8.toString() == "08:00:2b:01:02:03:04:05", "MACADDR8 string is correct");
}

void test_macaddr8_parsing()
{
    std::cout << "\n=== test_macaddr8_parsing ===" << std::endl;

    // Colon-separated
    {
        auto mac8 = MacAddr8::fromString("08:00:2b:01:02:03:04:05");
        test_assert(mac8.has_value(), "Parsed colon-separated MACADDR8");
        test_assert(mac8->toString() == "08:00:2b:01:02:03:04:05", "Parsed MACADDR8 is correct");
    }

    // Hyphen-separated
    {
        auto mac8 = MacAddr8::fromString("08-00-2b-01-02-03-04-05");
        test_assert(mac8.has_value(), "Parsed hyphen-separated MACADDR8");
    }
}

void test_macaddr8_from_macaddr()
{
    std::cout << "\n=== test_macaddr8_from_macaddr ===" << std::endl;

    auto mac = MacAddr(0x08, 0x00, 0x2b, 0x01, 0x02, 0x03);
    auto mac8 = MacAddr8::fromMacAddr(mac);

    // EUI-48 to EUI-64: insert 0xfffe in the middle
    test_assert(mac8.toString() == "08:00:2b:ff:fe:01:02:03", "EUI-48 to EUI-64 conversion works");
}

void test_macaddr8_operations()
{
    std::cout << "\n=== test_macaddr8_operations ===" << std::endl;

    auto mac8_1 = MacAddr8(0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
    auto mac8_2 = MacAddr8(0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0);

    // Bitwise AND
    auto and_result = mac8_1.bitwiseAnd(mac8_2);
    test_assert(and_result.toString() == "f0:f0:f0:f0:f0:f0:f0:f0", "MACADDR8 AND works");
}

// ============================================================================
// Network Utility Functions
// ============================================================================

void test_inet_same_family()
{
    std::cout << "\n=== test_inet_same_family ===" << std::endl;

    auto ipv4_1 = InetAddr::fromIPv4(192, 168, 1, 1);
    auto ipv4_2 = InetAddr::fromIPv4(10, 0, 0, 1);
    auto ipv6_1 = InetAddr::fromString("2001:db8::1").value();

    test_assert(network_utils::inet_same_family(ipv4_1, ipv4_2), "IPv4 addresses are same family");
    test_assert(!network_utils::inet_same_family(ipv4_1, ipv6_1), "IPv4 and IPv6 are different families");
}

void test_inet_merge()
{
    std::cout << "\n=== test_inet_merge ===" << std::endl;

    auto addr1 = InetAddr::fromIPv4(192, 168, 1, 1);
    auto addr2 = InetAddr::fromIPv4(192, 168, 1, 100);

    auto merged = network_utils::inet_merge(addr1, addr2);

    // Should find the smallest network containing both addresses
    test_assert(merged.contains(addr1), "Merged network contains addr1");
    test_assert(merged.contains(addr2), "Merged network contains addr2");
}

// ============================================================================
// TypedValue Integration Tests
// ============================================================================

void test_typed_value_integration()
{
    std::cout << "\n=== test_typed_value_integration ===" << std::endl;

    // INET
    {
        auto addr = InetAddr::fromIPv4(192, 168, 1, 1);
        auto tv = TypedValue::makeInet(addr);
        test_assert(tv.type() == DataType::INET, "TypedValue has INET type");
        test_assert(tv.getInet() == addr, "Can extract INET from TypedValue");
        test_assert(tv.toString() == "192.168.1.1", "INET toString works");
    }

    // CIDR
    {
        auto cidr = Cidr::fromIPv4(10, 0, 0, 0, 8);
        auto tv = TypedValue::makeCidr(cidr);
        test_assert(tv.type() == DataType::CIDR, "TypedValue has CIDR type");
        test_assert(tv.toString() == "10.0.0.0/8", "CIDR toString works");
    }

    // MACADDR
    {
        auto mac = MacAddr(0x08, 0x00, 0x2b, 0x01, 0x02, 0x03);
        auto tv = TypedValue::makeMacAddr(mac);
        test_assert(tv.type() == DataType::MACADDR, "TypedValue has MACADDR type");
        test_assert(tv.getMacAddr() == mac, "Can extract MACADDR from TypedValue");
        test_assert(tv.toString() == "08:00:2b:01:02:03", "MACADDR toString works");
    }

    // MACADDR8
    {
        auto mac8 = MacAddr8(0x08, 0x00, 0x2b, 0x01, 0x02, 0x03, 0x04, 0x05);
        auto tv = TypedValue::makeMacAddr8(mac8);
        test_assert(tv.type() == DataType::MACADDR8, "TypedValue has MACADDR8 type");
        test_assert(tv.getMacAddr8() == mac8, "Can extract MACADDR8 from TypedValue");
        test_assert(tv.toString() == "08:00:2b:01:02:03:04:05", "MACADDR8 toString works");
    }
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===========================================\n";
    std::cout << "Network Types Tests\n";
    std::cout << "Task 16: Network Types Implementation\n";
    std::cout << "===========================================\n";

    // INET tests
    test_inet_ipv4_basic();
    test_inet_ipv4_with_netmask();
    test_inet_ipv4_parsing();
    test_inet_ipv6_basic();
    test_inet_ipv6_with_netmask();
    test_inet_network_operations();
    test_inet_containment();
    test_inet_comparison();
    test_inet_bitwise_operations();
    test_inet_arithmetic();

    // CIDR tests
    test_cidr_basic();
    test_cidr_strict_validation();
    test_cidr_operations();

    // MACADDR tests
    test_macaddr_basic();
    test_macaddr_parsing();
    test_macaddr_operations();
    test_macaddr_trunc();
    test_macaddr_comparison();

    // MACADDR8 tests
    test_macaddr8_basic();
    test_macaddr8_parsing();
    test_macaddr8_from_macaddr();
    test_macaddr8_operations();

    // Utility functions
    test_inet_same_family();
    test_inet_merge();

    // TypedValue integration
    test_typed_value_integration();

    // Summary
    std::cout << "\n===========================================\n";
    std::cout << "Tests passed: " << g_tests_passed << "\n";
    std::cout << "Tests failed: " << g_tests_failed << "\n";
    std::cout << "===========================================\n";

    return g_tests_failed > 0 ? 1 : 0;
}
