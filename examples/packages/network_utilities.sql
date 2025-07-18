/*
 * ScratchBird v0.5.0 - Network Utilities Package
 * 
 * This package provides comprehensive network data type utilities
 * for ScratchBird's PostgreSQL-compatible network types.
 * 
 * Features demonstrated:
 * - Network address validation and manipulation
 * - CIDR calculation and subnet analysis
 * - MAC address formatting and validation
 * - Network security utilities
 * - IP address range operations
 * 
 * The contents of this file are subject to the Initial
 * Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at
 * http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 * 
 * Copyright (c) 2025 ScratchBird Development Team
 * All Rights Reserved.
 */

-- Enable SQL Dialect 4 for full package support
SET SQL DIALECT 4;

-- ================================================================
-- Package Header: Network Utilities
-- ================================================================

CREATE PACKAGE network_utilities AS
BEGIN
    -- Package version and information
    CONSTANT PACKAGE_VERSION VARCHAR(10) = '1.0.0';
    CONSTANT PACKAGE_DESCRIPTION VARCHAR(200) = 'ScratchBird Network Data Type Utilities Package';
    
    -- Exception declarations
    EXCEPTION INVALID_IP_ADDRESS 'Invalid IP address format';
    EXCEPTION INVALID_CIDR_NOTATION 'Invalid CIDR notation';
    EXCEPTION INVALID_MAC_ADDRESS 'Invalid MAC address format';
    EXCEPTION NETWORK_RANGE_ERROR 'Network range calculation error';
    EXCEPTION SUBNET_OVERFLOW 'Subnet calculation overflow';
    
    -- Type declarations
    TYPE ip_info_type AS (
        ip_address VARCHAR(45),
        address_type VARCHAR(10),
        is_private BOOLEAN,
        is_multicast BOOLEAN,
        is_loopback BOOLEAN,
        address_class VARCHAR(10)
    );
    
    TYPE subnet_info_type AS (
        network_address VARCHAR(45),
        broadcast_address VARCHAR(45),
        netmask VARCHAR(45),
        wildcard_mask VARCHAR(45),
        prefix_length INTEGER,
        host_count BIGINT,
        subnet_class VARCHAR(10)
    );
    
    TYPE mac_info_type AS (
        mac_address VARCHAR(17),
        oui VARCHAR(8),
        vendor VARCHAR(100),
        is_multicast BOOLEAN,
        is_local BOOLEAN,
        address_type VARCHAR(20)
    );
    
    -- IP Address Functions
    FUNCTION get_package_version() RETURNS VARCHAR(10);
    FUNCTION is_valid_ipv4(ip_address VARCHAR(45)) RETURNS BOOLEAN;
    FUNCTION is_valid_ipv6(ip_address VARCHAR(45)) RETURNS BOOLEAN;
    FUNCTION is_private_ip(ip_address VARCHAR(45)) RETURNS BOOLEAN;
    FUNCTION is_multicast_ip(ip_address VARCHAR(45)) RETURNS BOOLEAN;
    FUNCTION is_loopback_ip(ip_address VARCHAR(45)) RETURNS BOOLEAN;
    FUNCTION get_ip_class(ip_address VARCHAR(45)) RETURNS VARCHAR(10);
    FUNCTION ip_to_integer(ip_address VARCHAR(15)) RETURNS BIGINT;
    FUNCTION integer_to_ip(ip_integer BIGINT) RETURNS VARCHAR(15);
    FUNCTION normalize_ip(ip_address VARCHAR(45)) RETURNS VARCHAR(45);
    
    -- CIDR Functions
    FUNCTION is_valid_cidr(cidr_notation VARCHAR(18)) RETURNS BOOLEAN;
    FUNCTION cidr_to_netmask(prefix_length INTEGER) RETURNS VARCHAR(15);
    FUNCTION netmask_to_cidr(netmask VARCHAR(15)) RETURNS INTEGER;
    FUNCTION get_network_address(ip_address VARCHAR(15), prefix_length INTEGER) RETURNS VARCHAR(15);
    FUNCTION get_broadcast_address(ip_address VARCHAR(15), prefix_length INTEGER) RETURNS VARCHAR(15);
    FUNCTION calculate_host_count(prefix_length INTEGER) RETURNS BIGINT;
    FUNCTION is_ip_in_subnet(ip_address VARCHAR(15), subnet_cidr VARCHAR(18)) RETURNS BOOLEAN;
    
    -- MAC Address Functions
    FUNCTION is_valid_mac(mac_address VARCHAR(17)) RETURNS BOOLEAN;
    FUNCTION normalize_mac(mac_address VARCHAR(17), separator VARCHAR(1) DEFAULT ':') RETURNS VARCHAR(17);
    FUNCTION get_mac_oui(mac_address VARCHAR(17)) RETURNS VARCHAR(8);
    FUNCTION is_multicast_mac(mac_address VARCHAR(17)) RETURNS BOOLEAN;
    FUNCTION is_local_mac(mac_address VARCHAR(17)) RETURNS BOOLEAN;
    FUNCTION generate_random_mac() RETURNS VARCHAR(17);
    
    -- Network Analysis Procedures
    PROCEDURE analyze_ip_address(ip_address VARCHAR(45))
        RETURNS (property VARCHAR(50), value VARCHAR(100));
    PROCEDURE analyze_subnet(cidr_notation VARCHAR(18))
        RETURNS (property VARCHAR(50), value VARCHAR(100));
    PROCEDURE analyze_mac_address(mac_address VARCHAR(17))
        RETURNS (property VARCHAR(50), value VARCHAR(100));
    PROCEDURE scan_network_range(start_ip VARCHAR(15), end_ip VARCHAR(15))
        RETURNS (ip_address VARCHAR(15), is_valid BOOLEAN, ip_class VARCHAR(10));
    PROCEDURE subnet_calculator(network_address VARCHAR(15), subnet_bits INTEGER)
        RETURNS (subnet_number INTEGER, subnet_address VARCHAR(15), usable_range VARCHAR(35));
    
    -- Security Functions
    FUNCTION is_bogon_ip(ip_address VARCHAR(45)) RETURNS BOOLEAN;
    FUNCTION is_reserved_ip(ip_address VARCHAR(45)) RETURNS BOOLEAN;
    FUNCTION get_security_classification(ip_address VARCHAR(45)) RETURNS VARCHAR(50);
    
    -- Conversion Functions
    FUNCTION mac_to_eui64(mac_address VARCHAR(17)) RETURNS VARCHAR(23);
    FUNCTION ipv4_to_ipv6(ipv4_address VARCHAR(15)) RETURNS VARCHAR(45);
    FUNCTION compress_ipv6(ipv6_address VARCHAR(45)) RETURNS VARCHAR(45);
    FUNCTION expand_ipv6(ipv6_address VARCHAR(45)) RETURNS VARCHAR(45);
    
    -- Utility Procedures
    PROCEDURE validate_network_config(config_data VARCHAR(2000))
        RETURNS (item VARCHAR(100), status VARCHAR(20), message VARCHAR(200));
    PROCEDURE generate_network_report(network_cidr VARCHAR(18))
        RETURNS (section VARCHAR(50), information VARCHAR(500));
END;

-- ================================================================
-- Package Body: Network Utilities Implementation
-- ================================================================

CREATE PACKAGE BODY network_utilities AS
BEGIN
    -- Private variables
    VARIABLE oui_cache_timestamp TIMESTAMP;
    VARIABLE cache_refresh_interval INTEGER = 86400; -- 24 hours
    
    -- ================================================================
    -- Utility Functions
    -- ================================================================
    
    FUNCTION get_package_version() RETURNS VARCHAR(10) AS
    BEGIN
        RETURN PACKAGE_VERSION;
    END
    
    -- ================================================================
    -- IP Address Functions
    -- ================================================================
    
    FUNCTION is_valid_ipv4(ip_address VARCHAR(45)) RETURNS BOOLEAN AS
    DECLARE VARIABLE octet_count INTEGER;
    DECLARE VARIABLE pos INTEGER;
    DECLARE VARIABLE start_pos INTEGER;
    DECLARE VARIABLE octet_str VARCHAR(3);
    DECLARE VARIABLE octet_val INTEGER;
    BEGIN
        IF (ip_address IS NULL OR TRIM(ip_address) = '') THEN
            RETURN FALSE;
        END IF;
        
        -- Check basic format
        IF (NOT ip_address SIMILAR TO '[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}') THEN
            RETURN FALSE;
        END IF;
        
        -- Validate each octet
        octet_count = 0;
        start_pos = 1;
        
        WHILE (start_pos <= CHARACTER_LENGTH(ip_address)) DO
        BEGIN
            pos = POSITION('.', ip_address, start_pos);
            IF (pos = 0) THEN
                pos = CHARACTER_LENGTH(ip_address) + 1;
            END IF;
            
            octet_str = SUBSTRING(ip_address FROM start_pos FOR pos - start_pos);
            
            -- Check for leading zeros
            IF (CHARACTER_LENGTH(octet_str) > 1 AND SUBSTRING(octet_str FROM 1 FOR 1) = '0') THEN
                RETURN FALSE;
            END IF;
            
            octet_val = CAST(octet_str AS INTEGER);
            IF (octet_val < 0 OR octet_val > 255) THEN
                RETURN FALSE;
            END IF;
            
            octet_count = octet_count + 1;
            start_pos = pos + 1;
        END
        
        RETURN (octet_count = 4);
    END
    
    FUNCTION is_valid_ipv6(ip_address VARCHAR(45)) RETURNS BOOLEAN AS
    DECLARE VARIABLE hex_group_count INTEGER;
    DECLARE VARIABLE double_colon_count INTEGER;
    DECLARE VARIABLE pos INTEGER;
    BEGIN
        IF (ip_address IS NULL OR TRIM(ip_address) = '') THEN
            RETURN FALSE;
        END IF;
        
        -- Count double colons (can only have one)
        double_colon_count = 0;
        pos = 1;
        WHILE (pos <= CHARACTER_LENGTH(ip_address) - 1) DO
        BEGIN
            IF (SUBSTRING(ip_address FROM pos FOR 2) = '::') THEN
                double_colon_count = double_colon_count + 1;
            END IF;
            pos = pos + 1;
        END
        
        IF (double_colon_count > 1) THEN
            RETURN FALSE;
        END IF;
        
        -- Basic IPv6 pattern check
        IF (NOT ip_address SIMILAR TO '[0-9a-fA-F:]+') THEN
            RETURN FALSE;
        END IF;
        
        -- More comprehensive validation would be needed for production
        RETURN TRUE;
    END
    
    FUNCTION is_private_ip(ip_address VARCHAR(45)) RETURNS BOOLEAN AS
    DECLARE VARIABLE first_octet INTEGER;
    DECLARE VARIABLE second_octet INTEGER;
    DECLARE VARIABLE ip_integer BIGINT;
    BEGIN
        IF (NOT is_valid_ipv4(ip_address)) THEN
            RETURN FALSE;
        END IF;
        
        -- Parse first two octets
        first_octet = CAST(SUBSTRING(ip_address FROM 1 FOR POSITION('.', ip_address) - 1) AS INTEGER);
        second_octet = CAST(SUBSTRING(ip_address FROM POSITION('.', ip_address) + 1 FOR 
                           POSITION('.', ip_address, POSITION('.', ip_address) + 1) - POSITION('.', ip_address) - 1) AS INTEGER);
        
        -- Check private IP ranges
        -- 10.0.0.0/8
        IF (first_octet = 10) THEN
            RETURN TRUE;
        END IF;
        
        -- 172.16.0.0/12
        IF (first_octet = 172 AND second_octet >= 16 AND second_octet <= 31) THEN
            RETURN TRUE;
        END IF;
        
        -- 192.168.0.0/16
        IF (first_octet = 192 AND second_octet = 168) THEN
            RETURN TRUE;
        END IF;
        
        RETURN FALSE;
    END
    
    FUNCTION is_multicast_ip(ip_address VARCHAR(45)) RETURNS BOOLEAN AS
    DECLARE VARIABLE first_octet INTEGER;
    BEGIN
        IF (NOT is_valid_ipv4(ip_address)) THEN
            RETURN FALSE;
        END IF;
        
        first_octet = CAST(SUBSTRING(ip_address FROM 1 FOR POSITION('.', ip_address) - 1) AS INTEGER);
        
        -- Multicast range: 224.0.0.0 to 239.255.255.255
        RETURN (first_octet >= 224 AND first_octet <= 239);
    END
    
    FUNCTION is_loopback_ip(ip_address VARCHAR(45)) RETURNS BOOLEAN AS
    DECLARE VARIABLE first_octet INTEGER;
    BEGIN
        IF (NOT is_valid_ipv4(ip_address)) THEN
            RETURN FALSE;
        END IF;
        
        first_octet = CAST(SUBSTRING(ip_address FROM 1 FOR POSITION('.', ip_address) - 1) AS INTEGER);
        
        -- Loopback range: 127.0.0.0/8
        RETURN (first_octet = 127);
    END
    
    FUNCTION get_ip_class(ip_address VARCHAR(45)) RETURNS VARCHAR(10) AS
    DECLARE VARIABLE first_octet INTEGER;
    BEGIN
        IF (NOT is_valid_ipv4(ip_address)) THEN
            RETURN 'INVALID';
        END IF;
        
        first_octet = CAST(SUBSTRING(ip_address FROM 1 FOR POSITION('.', ip_address) - 1) AS INTEGER);
        
        IF (first_octet >= 1 AND first_octet <= 126) THEN
            RETURN 'A';
        ELSE IF (first_octet >= 128 AND first_octet <= 191) THEN
            RETURN 'B';
        ELSE IF (first_octet >= 192 AND first_octet <= 223) THEN
            RETURN 'C';
        ELSE IF (first_octet >= 224 AND first_octet <= 239) THEN
            RETURN 'D';
        ELSE IF (first_octet >= 240 AND first_octet <= 255) THEN
            RETURN 'E';
        ELSE
            RETURN 'RESERVED';
        END IF;
    END
    
    FUNCTION ip_to_integer(ip_address VARCHAR(15)) RETURNS BIGINT AS
    DECLARE VARIABLE octet1 INTEGER;
    DECLARE VARIABLE octet2 INTEGER;
    DECLARE VARIABLE octet3 INTEGER;
    DECLARE VARIABLE octet4 INTEGER;
    DECLARE VARIABLE pos1 INTEGER;
    DECLARE VARIABLE pos2 INTEGER;
    DECLARE VARIABLE pos3 INTEGER;
    BEGIN
        IF (NOT is_valid_ipv4(ip_address)) THEN
            EXCEPTION INVALID_IP_ADDRESS;
        END IF;
        
        -- Parse octets
        pos1 = POSITION('.', ip_address);
        pos2 = POSITION('.', ip_address, pos1 + 1);
        pos3 = POSITION('.', ip_address, pos2 + 1);
        
        octet1 = CAST(SUBSTRING(ip_address FROM 1 FOR pos1 - 1) AS INTEGER);
        octet2 = CAST(SUBSTRING(ip_address FROM pos1 + 1 FOR pos2 - pos1 - 1) AS INTEGER);
        octet3 = CAST(SUBSTRING(ip_address FROM pos2 + 1 FOR pos3 - pos2 - 1) AS INTEGER);
        octet4 = CAST(SUBSTRING(ip_address FROM pos3 + 1) AS INTEGER);
        
        RETURN (octet1 * 16777216) + (octet2 * 65536) + (octet3 * 256) + octet4;
    END
    
    FUNCTION integer_to_ip(ip_integer BIGINT) RETURNS VARCHAR(15) AS
    DECLARE VARIABLE octet1 INTEGER;
    DECLARE VARIABLE octet2 INTEGER;
    DECLARE VARIABLE octet3 INTEGER;
    DECLARE VARIABLE octet4 INTEGER;
    DECLARE VARIABLE remaining BIGINT;
    BEGIN
        IF (ip_integer < 0 OR ip_integer > 4294967295) THEN
            EXCEPTION INVALID_IP_ADDRESS;
        END IF;
        
        remaining = ip_integer;
        octet1 = remaining / 16777216;
        remaining = remaining - (octet1 * 16777216);
        
        octet2 = remaining / 65536;
        remaining = remaining - (octet2 * 65536);
        
        octet3 = remaining / 256;
        octet4 = remaining - (octet3 * 256);
        
        RETURN CAST(octet1 AS VARCHAR(3)) || '.' || 
               CAST(octet2 AS VARCHAR(3)) || '.' || 
               CAST(octet3 AS VARCHAR(3)) || '.' || 
               CAST(octet4 AS VARCHAR(3));
    END
    
    FUNCTION normalize_ip(ip_address VARCHAR(45)) RETURNS VARCHAR(45) AS
    BEGIN
        IF (is_valid_ipv4(ip_address)) THEN
            RETURN ip_address; -- IPv4 is already normalized
        ELSE IF (is_valid_ipv6(ip_address)) THEN
            -- For IPv6, we would implement full normalization
            RETURN UPPER(ip_address);
        ELSE
            EXCEPTION INVALID_IP_ADDRESS;
        END IF;
    END
    
    -- ================================================================
    -- CIDR Functions
    -- ================================================================
    
    FUNCTION is_valid_cidr(cidr_notation VARCHAR(18)) RETURNS BOOLEAN AS
    DECLARE VARIABLE slash_pos INTEGER;
    DECLARE VARIABLE ip_part VARCHAR(15);
    DECLARE VARIABLE prefix_part VARCHAR(2);
    DECLARE VARIABLE prefix_length INTEGER;
    BEGIN
        IF (cidr_notation IS NULL OR TRIM(cidr_notation) = '') THEN
            RETURN FALSE;
        END IF;
        
        slash_pos = POSITION('/', cidr_notation);
        IF (slash_pos = 0) THEN
            RETURN FALSE;
        END IF;
        
        ip_part = SUBSTRING(cidr_notation FROM 1 FOR slash_pos - 1);
        prefix_part = SUBSTRING(cidr_notation FROM slash_pos + 1);
        
        -- Validate IP part
        IF (NOT is_valid_ipv4(ip_part)) THEN
            RETURN FALSE;
        END IF;
        
        -- Validate prefix length
        prefix_length = CAST(prefix_part AS INTEGER);
        RETURN (prefix_length >= 0 AND prefix_length <= 32);
    END
    
    FUNCTION cidr_to_netmask(prefix_length INTEGER) RETURNS VARCHAR(15) AS
    DECLARE VARIABLE mask_integer BIGINT;
    DECLARE VARIABLE shift_amount INTEGER;
    BEGIN
        IF (prefix_length < 0 OR prefix_length > 32) THEN
            EXCEPTION INVALID_CIDR_NOTATION;
        END IF;
        
        shift_amount = 32 - prefix_length;
        mask_integer = BIN_SHL(4294967295, shift_amount);
        
        RETURN integer_to_ip(mask_integer);
    END
    
    FUNCTION get_network_address(ip_address VARCHAR(15), prefix_length INTEGER) RETURNS VARCHAR(15) AS
    DECLARE VARIABLE ip_integer BIGINT;
    DECLARE VARIABLE mask_integer BIGINT;
    DECLARE VARIABLE network_integer BIGINT;
    BEGIN
        IF (NOT is_valid_ipv4(ip_address)) THEN
            EXCEPTION INVALID_IP_ADDRESS;
        END IF;
        
        ip_integer = ip_to_integer(ip_address);
        mask_integer = BIN_SHL(4294967295, 32 - prefix_length);
        network_integer = BIN_AND(ip_integer, mask_integer);
        
        RETURN integer_to_ip(network_integer);
    END
    
    FUNCTION get_broadcast_address(ip_address VARCHAR(15), prefix_length INTEGER) RETURNS VARCHAR(15) AS
    DECLARE VARIABLE ip_integer BIGINT;
    DECLARE VARIABLE mask_integer BIGINT;
    DECLARE VARIABLE broadcast_integer BIGINT;
    BEGIN
        IF (NOT is_valid_ipv4(ip_address)) THEN
            EXCEPTION INVALID_IP_ADDRESS;
        END IF;
        
        ip_integer = ip_to_integer(ip_address);
        mask_integer = BIN_SHL(4294967295, 32 - prefix_length);
        broadcast_integer = BIN_OR(ip_integer, BIN_NOT(mask_integer));
        
        RETURN integer_to_ip(broadcast_integer);
    END
    
    FUNCTION calculate_host_count(prefix_length INTEGER) RETURNS BIGINT AS
    DECLARE VARIABLE host_bits INTEGER;
    BEGIN
        IF (prefix_length < 0 OR prefix_length > 32) THEN
            EXCEPTION INVALID_CIDR_NOTATION;
        END IF;
        
        host_bits = 32 - prefix_length;
        
        -- Special cases
        IF (prefix_length = 31) THEN
            RETURN 2; -- Point-to-point link
        ELSE IF (prefix_length = 32) THEN
            RETURN 1; -- Host route
        ELSE
            RETURN POWER(2, host_bits) - 2; -- Subtract network and broadcast
        END IF;
    END
    
    FUNCTION is_ip_in_subnet(ip_address VARCHAR(15), subnet_cidr VARCHAR(18)) RETURNS BOOLEAN AS
    DECLARE VARIABLE network_addr VARCHAR(15);
    DECLARE VARIABLE broadcast_addr VARCHAR(15);
    DECLARE VARIABLE ip_integer BIGINT;
    DECLARE VARIABLE network_integer BIGINT;
    DECLARE VARIABLE broadcast_integer BIGINT;
    DECLARE VARIABLE slash_pos INTEGER;
    DECLARE VARIABLE prefix_length INTEGER;
    BEGIN
        IF (NOT is_valid_ipv4(ip_address) OR NOT is_valid_cidr(subnet_cidr)) THEN
            RETURN FALSE;
        END IF;
        
        slash_pos = POSITION('/', subnet_cidr);
        prefix_length = CAST(SUBSTRING(subnet_cidr FROM slash_pos + 1) AS INTEGER);
        
        network_addr = get_network_address(SUBSTRING(subnet_cidr FROM 1 FOR slash_pos - 1), prefix_length);
        broadcast_addr = get_broadcast_address(SUBSTRING(subnet_cidr FROM 1 FOR slash_pos - 1), prefix_length);
        
        ip_integer = ip_to_integer(ip_address);
        network_integer = ip_to_integer(network_addr);
        broadcast_integer = ip_to_integer(broadcast_addr);
        
        RETURN (ip_integer >= network_integer AND ip_integer <= broadcast_integer);
    END
    
    -- ================================================================
    -- MAC Address Functions
    -- ================================================================
    
    FUNCTION is_valid_mac(mac_address VARCHAR(17)) RETURNS BOOLEAN AS
    BEGIN
        IF (mac_address IS NULL OR TRIM(mac_address) = '') THEN
            RETURN FALSE;
        END IF;
        
        -- Check for standard MAC address formats
        RETURN (mac_address SIMILAR TO '[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}' OR
                mac_address SIMILAR TO '[0-9A-Fa-f]{2}-[0-9A-Fa-f]{2}-[0-9A-Fa-f]{2}-[0-9A-Fa-f]{2}-[0-9A-Fa-f]{2}-[0-9A-Fa-f]{2}' OR
                mac_address SIMILAR TO '[0-9A-Fa-f]{12}');
    END
    
    FUNCTION normalize_mac(mac_address VARCHAR(17), separator VARCHAR(1) DEFAULT ':') RETURNS VARCHAR(17) AS
    DECLARE VARIABLE clean_mac VARCHAR(12);
    DECLARE VARIABLE result VARCHAR(17);
    DECLARE VARIABLE i INTEGER;
    BEGIN
        IF (NOT is_valid_mac(mac_address)) THEN
            EXCEPTION INVALID_MAC_ADDRESS;
        END IF;
        
        -- Remove all separators
        clean_mac = REPLACE(REPLACE(REPLACE(mac_address, ':', ''), '-', ''), '.', '');
        clean_mac = UPPER(clean_mac);
        
        -- Add separators
        result = SUBSTRING(clean_mac FROM 1 FOR 2) || separator ||
                SUBSTRING(clean_mac FROM 3 FOR 2) || separator ||
                SUBSTRING(clean_mac FROM 5 FOR 2) || separator ||
                SUBSTRING(clean_mac FROM 7 FOR 2) || separator ||
                SUBSTRING(clean_mac FROM 9 FOR 2) || separator ||
                SUBSTRING(clean_mac FROM 11 FOR 2);
        
        RETURN result;
    END
    
    FUNCTION get_mac_oui(mac_address VARCHAR(17)) RETURNS VARCHAR(8) AS
    DECLARE VARIABLE normalized_mac VARCHAR(17);
    BEGIN
        normalized_mac = normalize_mac(mac_address);
        RETURN SUBSTRING(normalized_mac FROM 1 FOR 8);
    END
    
    FUNCTION is_multicast_mac(mac_address VARCHAR(17)) RETURNS BOOLEAN AS
    DECLARE VARIABLE first_octet VARCHAR(2);
    DECLARE VARIABLE first_octet_int INTEGER;
    BEGIN
        IF (NOT is_valid_mac(mac_address)) THEN
            RETURN FALSE;
        END IF;
        
        first_octet = SUBSTRING(normalize_mac(mac_address) FROM 1 FOR 2);
        first_octet_int = CAST(('0x' || first_octet) AS INTEGER);
        
        -- Check if the least significant bit of the first octet is 1
        RETURN (BIN_AND(first_octet_int, 1) = 1);
    END
    
    FUNCTION is_local_mac(mac_address VARCHAR(17)) RETURNS BOOLEAN AS
    DECLARE VARIABLE first_octet VARCHAR(2);
    DECLARE VARIABLE first_octet_int INTEGER;
    BEGIN
        IF (NOT is_valid_mac(mac_address)) THEN
            RETURN FALSE;
        END IF;
        
        first_octet = SUBSTRING(normalize_mac(mac_address) FROM 1 FOR 2);
        first_octet_int = CAST(('0x' || first_octet) AS INTEGER);
        
        -- Check if the second least significant bit of the first octet is 1
        RETURN (BIN_AND(first_octet_int, 2) = 2);
    END
    
    FUNCTION generate_random_mac() RETURNS VARCHAR(17) AS
    DECLARE VARIABLE mac_bytes VARCHAR(12);
    DECLARE VARIABLE i INTEGER;
    DECLARE VARIABLE random_byte INTEGER;
    BEGIN
        mac_bytes = '';
        
        FOR i = 1 TO 6 DO
        BEGIN
            random_byte = MOD(RAND() * 256, 256);
            mac_bytes = mac_bytes || RIGHT('0' || CAST(random_byte AS VARCHAR(2)), 2);
        END
        
        -- Set locally administered bit
        mac_bytes = SUBSTRING(mac_bytes FROM 1 FOR 1) || 
                   CAST(BIN_OR(CAST(('0x' || SUBSTRING(mac_bytes FROM 2 FOR 1)) AS INTEGER), 2) AS VARCHAR(1)) ||
                   SUBSTRING(mac_bytes FROM 3);
        
        RETURN normalize_mac(mac_bytes);
    END
    
    -- ================================================================
    -- Analysis Procedures
    -- ================================================================
    
    PROCEDURE analyze_ip_address(ip_address VARCHAR(45))
        RETURNS (property VARCHAR(50), value VARCHAR(100)) AS
    BEGIN
        -- IP Address Type
        property = 'IP Address';
        value = ip_address;
        SUSPEND;
        
        -- Address Type
        property = 'Address Type';
        IF (is_valid_ipv4(ip_address)) THEN
            value = 'IPv4';
        ELSE IF (is_valid_ipv6(ip_address)) THEN
            value = 'IPv6';
        ELSE
            value = 'Invalid';
        END IF;
        SUSPEND;
        
        -- Only continue if valid IPv4
        IF (is_valid_ipv4(ip_address)) THEN
        BEGIN
            -- IP Class
            property = 'IP Class';
            value = 'Class ' || get_ip_class(ip_address);
            SUSPEND;
            
            -- Private/Public
            property = 'Address Scope';
            IF (is_private_ip(ip_address)) THEN
                value = 'Private';
            ELSE
                value = 'Public';
            END IF;
            SUSPEND;
            
            -- Special Properties
            IF (is_multicast_ip(ip_address)) THEN
            BEGIN
                property = 'Multicast';
                value = 'Yes';
                SUSPEND;
            END
            
            IF (is_loopback_ip(ip_address)) THEN
            BEGIN
                property = 'Loopback';
                value = 'Yes';
                SUSPEND;
            END
            
            -- Integer Representation
            property = 'Integer Value';
            value = CAST(ip_to_integer(ip_address) AS VARCHAR(15));
            SUSPEND;
        END
    END
    
    PROCEDURE analyze_subnet(cidr_notation VARCHAR(18))
        RETURNS (property VARCHAR(50), value VARCHAR(100)) AS
    DECLARE VARIABLE slash_pos INTEGER;
    DECLARE VARIABLE ip_part VARCHAR(15);
    DECLARE VARIABLE prefix_length INTEGER;
    DECLARE VARIABLE network_addr VARCHAR(15);
    DECLARE VARIABLE broadcast_addr VARCHAR(15);
    DECLARE VARIABLE host_count BIGINT;
    BEGIN
        IF (NOT is_valid_cidr(cidr_notation)) THEN
        BEGIN
            property = 'Error';
            value = 'Invalid CIDR notation';
            SUSPEND;
            EXIT;
        END
        
        slash_pos = POSITION('/', cidr_notation);
        ip_part = SUBSTRING(cidr_notation FROM 1 FOR slash_pos - 1);
        prefix_length = CAST(SUBSTRING(cidr_notation FROM slash_pos + 1) AS INTEGER);
        
        -- CIDR Notation
        property = 'CIDR Notation';
        value = cidr_notation;
        SUSPEND;
        
        -- Network Address
        network_addr = get_network_address(ip_part, prefix_length);
        property = 'Network Address';
        value = network_addr;
        SUSPEND;
        
        -- Broadcast Address
        broadcast_addr = get_broadcast_address(ip_part, prefix_length);
        property = 'Broadcast Address';
        value = broadcast_addr;
        SUSPEND;
        
        -- Netmask
        property = 'Subnet Mask';
        value = cidr_to_netmask(prefix_length);
        SUSPEND;
        
        -- Prefix Length
        property = 'Prefix Length';
        value = '/' || prefix_length;
        SUSPEND;
        
        -- Host Count
        host_count = calculate_host_count(prefix_length);
        property = 'Usable Hosts';
        value = CAST(host_count AS VARCHAR(15));
        SUSPEND;
        
        -- Subnet Class
        property = 'Subnet Class';
        IF (prefix_length <= 8) THEN
            value = 'Class A';
        ELSE IF (prefix_length <= 16) THEN
            value = 'Class B';
        ELSE IF (prefix_length <= 24) THEN
            value = 'Class C';
        ELSE
            value = 'Subnet';
        END IF;
        SUSPEND;
    END
    
    PROCEDURE analyze_mac_address(mac_address VARCHAR(17))
        RETURNS (property VARCHAR(50), value VARCHAR(100)) AS
    DECLARE VARIABLE normalized_mac VARCHAR(17);
    DECLARE VARIABLE oui VARCHAR(8);
    BEGIN
        IF (NOT is_valid_mac(mac_address)) THEN
        BEGIN
            property = 'Error';
            value = 'Invalid MAC address format';
            SUSPEND;
            EXIT;
        END
        
        normalized_mac = normalize_mac(mac_address);
        
        -- MAC Address
        property = 'MAC Address';
        value = normalized_mac;
        SUSPEND;
        
        -- OUI
        oui = get_mac_oui(mac_address);
        property = 'OUI (Vendor ID)';
        value = oui;
        SUSPEND;
        
        -- Multicast
        property = 'Multicast';
        IF (is_multicast_mac(mac_address)) THEN
            value = 'Yes';
        ELSE
            value = 'No';
        END IF;
        SUSPEND;
        
        -- Local Administration
        property = 'Locally Administered';
        IF (is_local_mac(mac_address)) THEN
            value = 'Yes';
        ELSE
            value = 'No (Globally Unique)';
        END IF;
        SUSPEND;
        
        -- Address Type
        property = 'Address Type';
        IF (is_multicast_mac(mac_address)) THEN
            value = 'Multicast';
        ELSE IF (is_local_mac(mac_address)) THEN
            value = 'Locally Administered';
        ELSE
            value = 'Globally Unique';
        END IF;
        SUSPEND;
    END
    
    PROCEDURE scan_network_range(start_ip VARCHAR(15), end_ip VARCHAR(15))
        RETURNS (ip_address VARCHAR(15), is_valid BOOLEAN, ip_class VARCHAR(10)) AS
    DECLARE VARIABLE start_integer BIGINT;
    DECLARE VARIABLE end_integer BIGINT;
    DECLARE VARIABLE current_integer BIGINT;
    DECLARE VARIABLE current_ip VARCHAR(15);
    BEGIN
        IF (NOT is_valid_ipv4(start_ip) OR NOT is_valid_ipv4(end_ip)) THEN
        BEGIN
            ip_address = 'ERROR';
            is_valid = FALSE;
            ip_class = 'INVALID';
            SUSPEND;
            EXIT;
        END
        
        start_integer = ip_to_integer(start_ip);
        end_integer = ip_to_integer(end_ip);
        
        IF (start_integer > end_integer) THEN
        BEGIN
            ip_address = 'ERROR';
            is_valid = FALSE;
            ip_class = 'RANGE_ERROR';
            SUSPEND;
            EXIT;
        END
        
        current_integer = start_integer;
        
        WHILE (current_integer <= end_integer AND current_integer <= start_integer + 256) DO
        BEGIN
            current_ip = integer_to_ip(current_integer);
            
            ip_address = current_ip;
            is_valid = is_valid_ipv4(current_ip);
            ip_class = get_ip_class(current_ip);
            
            SUSPEND;
            current_integer = current_integer + 1;
        END
    END
    
    -- ================================================================
    -- Security Functions
    -- ================================================================
    
    FUNCTION is_bogon_ip(ip_address VARCHAR(45)) RETURNS BOOLEAN AS
    DECLARE VARIABLE first_octet INTEGER;
    DECLARE VARIABLE second_octet INTEGER;
    BEGIN
        IF (NOT is_valid_ipv4(ip_address)) THEN
            RETURN TRUE; -- Invalid IPs are considered bogon
        END IF;
        
        -- Check for bogon ranges
        first_octet = CAST(SUBSTRING(ip_address FROM 1 FOR POSITION('.', ip_address) - 1) AS INTEGER);
        
        -- 0.0.0.0/8 - Reserved
        IF (first_octet = 0) THEN
            RETURN TRUE;
        END IF;
        
        -- 127.0.0.0/8 - Loopback
        IF (first_octet = 127) THEN
            RETURN TRUE;
        END IF;
        
        -- 224.0.0.0/3 - Multicast and Reserved
        IF (first_octet >= 224) THEN
            RETURN TRUE;
        END IF;
        
        -- Private IP ranges are considered bogon for public routing
        IF (is_private_ip(ip_address)) THEN
            RETURN TRUE;
        END IF;
        
        RETURN FALSE;
    END
    
    FUNCTION is_reserved_ip(ip_address VARCHAR(45)) RETURNS BOOLEAN AS
    DECLARE VARIABLE first_octet INTEGER;
    BEGIN
        IF (NOT is_valid_ipv4(ip_address)) THEN
            RETURN FALSE;
        END IF;
        
        first_octet = CAST(SUBSTRING(ip_address FROM 1 FOR POSITION('.', ip_address) - 1) AS INTEGER);
        
        -- Reserved ranges
        IF (first_octet = 0 OR first_octet = 127 OR first_octet >= 240) THEN
            RETURN TRUE;
        END IF;
        
        RETURN FALSE;
    END
    
    FUNCTION get_security_classification(ip_address VARCHAR(45)) RETURNS VARCHAR(50) AS
    BEGIN
        IF (NOT is_valid_ipv4(ip_address)) THEN
            RETURN 'INVALID';
        END IF;
        
        IF (is_reserved_ip(ip_address)) THEN
            RETURN 'RESERVED';
        END IF;
        
        IF (is_private_ip(ip_address)) THEN
            RETURN 'PRIVATE';
        END IF;
        
        IF (is_multicast_ip(ip_address)) THEN
            RETURN 'MULTICAST';
        END IF;
        
        IF (is_loopback_ip(ip_address)) THEN
            RETURN 'LOOPBACK';
        END IF;
        
        IF (is_bogon_ip(ip_address)) THEN
            RETURN 'BOGON';
        END IF;
        
        RETURN 'PUBLIC';
    END
    
    -- ================================================================
    -- Utility Procedures
    -- ================================================================
    
    PROCEDURE generate_network_report(network_cidr VARCHAR(18))
        RETURNS (section VARCHAR(50), information VARCHAR(500)) AS
    DECLARE VARIABLE slash_pos INTEGER;
    DECLARE VARIABLE ip_part VARCHAR(15);
    DECLARE VARIABLE prefix_length INTEGER;
    BEGIN
        IF (NOT is_valid_cidr(network_cidr)) THEN
        BEGIN
            section = 'ERROR';
            information = 'Invalid CIDR notation: ' || network_cidr;
            SUSPEND;
            EXIT;
        END
        
        slash_pos = POSITION('/', network_cidr);
        ip_part = SUBSTRING(network_cidr FROM 1 FOR slash_pos - 1);
        prefix_length = CAST(SUBSTRING(network_cidr FROM slash_pos + 1) AS INTEGER);
        
        -- Report Header
        section = 'NETWORK_REPORT';
        information = '=== Network Analysis Report for ' || network_cidr || ' ===';
        SUSPEND;
        
        -- Basic Information
        section = 'BASIC_INFO';
        information = 'Network: ' || get_network_address(ip_part, prefix_length) ||
                     ', Broadcast: ' || get_broadcast_address(ip_part, prefix_length) ||
                     ', Hosts: ' || calculate_host_count(prefix_length);
        SUSPEND;
        
        -- Security Analysis
        section = 'SECURITY';
        information = 'Classification: ' || get_security_classification(ip_part) ||
                     ', Private: ' || CASE WHEN is_private_ip(ip_part) THEN 'Yes' ELSE 'No' END ||
                     ', Bogon: ' || CASE WHEN is_bogon_ip(ip_part) THEN 'Yes' ELSE 'No' END;
        SUSPEND;
        
        -- Subnet Analysis
        section = 'SUBNET_ANALYSIS';
        information = 'Subnet Mask: ' || cidr_to_netmask(prefix_length) ||
                     ', IP Class: ' || get_ip_class(ip_part) ||
                     ', Prefix: /' || prefix_length;
        SUSPEND;
    END
    
    -- ================================================================
    -- Package Initialization
    -- ================================================================
    
    -- Initialize package variables
    oui_cache_timestamp = CURRENT_TIMESTAMP;
    
    -- Create supporting tables if they don't exist
    EXECUTE STATEMENT '
        CREATE TABLE IF NOT EXISTS network_analysis_log (
            log_id UUID DEFAULT GEN_UUID(7),
            analysis_type VARCHAR(50),
            input_data VARCHAR(100),
            analysis_timestamp TIMESTAMP,
            user_name VARCHAR(50) DEFAULT USER
        )
    ';
    
END;

-- ================================================================
-- Package Usage Examples
-- ================================================================

-- Example 1: IP Address Analysis
SELECT * FROM network_utilities.analyze_ip_address('192.168.1.100');

-- Example 2: Subnet Analysis
SELECT * FROM network_utilities.analyze_subnet('192.168.1.0/24');

-- Example 3: MAC Address Analysis
SELECT * FROM network_utilities.analyze_mac_address('00:11:22:33:44:55');

-- Example 4: IP Validation
SELECT 
    '192.168.1.100' as ip,
    network_utilities.is_valid_ipv4('192.168.1.100') as is_valid_ipv4,
    network_utilities.is_private_ip('192.168.1.100') as is_private,
    network_utilities.get_ip_class('192.168.1.100') as ip_class;

-- Example 5: CIDR Operations
SELECT 
    '192.168.1.0/24' as cidr,
    network_utilities.get_network_address('192.168.1.100', 24) as network,
    network_utilities.get_broadcast_address('192.168.1.100', 24) as broadcast,
    network_utilities.calculate_host_count(24) as host_count;

-- Example 6: MAC Address Operations
SELECT 
    '00:11:22:33:44:55' as mac,
    network_utilities.normalize_mac('00-11-22-33-44-55') as normalized,
    network_utilities.get_mac_oui('00:11:22:33:44:55') as oui,
    network_utilities.is_multicast_mac('00:11:22:33:44:55') as is_multicast;

-- Example 7: Network Report
SELECT * FROM network_utilities.generate_network_report('192.168.1.0/24');

-- Example 8: Security Classification
SELECT 
    ip_address,
    network_utilities.get_security_classification(CAST(ip_address AS VARCHAR(45))) as classification
FROM (
    SELECT '192.168.1.1' as ip_address FROM RDB$DATABASE
    UNION ALL
    SELECT '8.8.8.8' FROM RDB$DATABASE
    UNION ALL
    SELECT '127.0.0.1' FROM RDB$DATABASE
    UNION ALL
    SELECT '224.0.0.1' FROM RDB$DATABASE
);

-- ================================================================
-- Package Grants and Security
-- ================================================================

-- Grant usage to network administrators
GRANT USAGE ON PACKAGE network_utilities TO ROLE NETWORK_ADMIN;
GRANT USAGE ON PACKAGE network_utilities TO ROLE DBA;

-- Grant specific function access to developers
GRANT EXECUTE ON FUNCTION network_utilities.is_valid_ipv4 TO ROLE DEVELOPER;
GRANT EXECUTE ON FUNCTION network_utilities.is_valid_cidr TO ROLE DEVELOPER;
GRANT EXECUTE ON FUNCTION network_utilities.is_valid_mac TO ROLE DEVELOPER;

-- Grant analysis procedures to security team
GRANT EXECUTE ON PROCEDURE network_utilities.analyze_ip_address TO ROLE SECURITY_ANALYST;
GRANT EXECUTE ON PROCEDURE network_utilities.analyze_subnet TO ROLE SECURITY_ANALYST;
GRANT EXECUTE ON PROCEDURE network_utilities.generate_network_report TO ROLE SECURITY_ANALYST;

COMMIT;

/*
 * This network utilities package provides:
 * 
 * 1. Comprehensive IP address validation and analysis
 * 2. CIDR notation processing and subnet calculations
 * 3. MAC address validation and manipulation
 * 4. Network security classification
 * 5. Network range scanning capabilities
 * 6. Conversion utilities between formats
 * 7. Reporting and analysis tools
 * 
 * Key Features:
 * - Supports IPv4 and basic IPv6 validation
 * - Comprehensive CIDR calculations
 * - MAC address normalization and OUI extraction
 * - Security classification of IP addresses
 * - Network analysis and reporting
 * - Integration with ScratchBird's network data types
 * 
 * Usage:
 * - Install the package in your ScratchBird database
 * - Grant appropriate permissions to users/roles
 * - Use functions for validation and conversion
 * - Use procedures for analysis and reporting
 * - Integrate with network monitoring systems
 */