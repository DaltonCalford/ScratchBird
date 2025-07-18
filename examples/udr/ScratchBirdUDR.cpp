/*
 * ScratchBird v0.5.0 - UDR (User Defined Routines) Examples
 * 
 * This module demonstrates creating User Defined Routines (UDR) for
 * ScratchBird, including functions, procedures, and triggers with
 * hierarchical schema support.
 * 
 * Features demonstrated:
 * - UDR Functions with modern C++ interface
 * - UDR Procedures with schema awareness
 * - UDR Triggers with PostgreSQL-compatible data types
 * - JSON processing functions
 * - Network data type functions
 * - Schema-aware routine management
 * 
 * Compilation:
 * g++ -std=c++17 -fPIC -shared -I../include -o libScratchBirdUDR.so ScratchBirdUDR.cpp -lsbclient
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

#include <string>
#include <sstream>
#include <iomanip>
#include <regex>
#include <cmath>
#include <ctime>
#include <memory>
#include <vector>
#include <map>

// ScratchBird UDR Framework
#include "sb_udr.h"
#include "sb_interfaces.h"

using namespace ScratchBird;
using namespace ScratchBird::UDR;

// ================================================================
// UDR Functions
// ================================================================

// Function: Generate hierarchical schema path
FB_UDR_BEGIN_FUNCTION(GenerateSchemaPath)
    FB_UDR_MESSAGE(InMessage,
        (FB_VARCHAR(63), schema_name)
        (FB_INTEGER, level)
        (FB_VARCHAR(63), parent_name)
    );
    
    FB_UDR_MESSAGE(OutMessage,
        (FB_VARCHAR(511), schema_path)
    );
    
    FB_UDR_EXECUTE_FUNCTION
    {
        std::string schemaName = in->schema_name.str;
        int level = in->level;
        std::string parentName = in->parent_name.str;
        
        std::string path;
        
        if (level == 1) {
            path = schemaName;
        } else {
            path = parentName + "." + schemaName;
        }
        
        out->schema_pathNull = FB_FALSE;
        out->schema_path.length = static_cast<FB_USHORT>(path.length());
        path.copy(out->schema_path.str, path.length());
    }
FB_UDR_END_FUNCTION

// Function: Validate network address
FB_UDR_BEGIN_FUNCTION(ValidateNetworkAddress)
    FB_UDR_MESSAGE(InMessage,
        (FB_VARCHAR(45), ip_address)
        (FB_VARCHAR(20), address_type)
    );
    
    FB_UDR_MESSAGE(OutMessage,
        (FB_BOOLEAN, is_valid)
        (FB_VARCHAR(100), validation_message)
    );
    
    FB_UDR_EXECUTE_FUNCTION
    {
        std::string ipAddress = in->ip_address.str;
        std::string addressType = in->address_type.str;
        
        bool isValid = false;
        std::string message;
        
        if (addressType == "IPv4") {
            // Simple IPv4 validation
            std::regex ipv4Regex(R"(^(\d{1,3}\.){3}\d{1,3}$)");
            if (std::regex_match(ipAddress, ipv4Regex)) {
                // Check octets are in valid range
                std::istringstream iss(ipAddress);
                std::string octet;
                isValid = true;
                
                while (std::getline(iss, octet, '.')) {
                    int val = std::stoi(octet);
                    if (val < 0 || val > 255) {
                        isValid = false;
                        break;
                    }
                }
                
                if (isValid) {
                    message = "Valid IPv4 address";
                } else {
                    message = "Invalid IPv4 address: octet out of range";
                }
            } else {
                message = "Invalid IPv4 address format";
            }
        } else if (addressType == "IPv6") {
            // Basic IPv6 validation
            std::regex ipv6Regex(R"(^([0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$)");
            if (std::regex_match(ipAddress, ipv6Regex)) {
                isValid = true;
                message = "Valid IPv6 address";
            } else {
                message = "Invalid IPv6 address format";
            }
        } else {
            message = "Unknown address type";
        }
        
        out->is_validNull = FB_FALSE;
        out->is_valid = isValid ? FB_TRUE : FB_FALSE;
        
        out->validation_messageNull = FB_FALSE;
        out->validation_message.length = static_cast<FB_USHORT>(message.length());
        message.copy(out->validation_message.str, message.length());
    }
FB_UDR_END_FUNCTION

// Function: Calculate network subnet info
FB_UDR_BEGIN_FUNCTION(CalculateSubnetInfo)
    FB_UDR_MESSAGE(InMessage,
        (FB_VARCHAR(18), cidr_notation)
    );
    
    FB_UDR_MESSAGE(OutMessage,
        (FB_VARCHAR(15), network_address)
        (FB_VARCHAR(15), broadcast_address)
        (FB_INTEGER, host_count)
        (FB_INTEGER, prefix_length)
    );
    
    FB_UDR_EXECUTE_FUNCTION
    {
        std::string cidrNotation = in->cidr_notation.str;
        
        // Parse CIDR notation (e.g., "192.168.1.0/24")
        size_t slashPos = cidrNotation.find('/');
        if (slashPos != std::string::npos) {
            std::string networkStr = cidrNotation.substr(0, slashPos);
            int prefixLength = std::stoi(cidrNotation.substr(slashPos + 1));
            
            // Calculate network and broadcast addresses (simplified)
            std::string networkAddr = networkStr;
            std::string broadcastAddr = networkStr;
            
            // For demonstration, modify last octet
            size_t lastDotPos = networkAddr.find_last_of('.');
            if (lastDotPos != std::string::npos) {
                networkAddr = networkAddr.substr(0, lastDotPos + 1) + "0";
                broadcastAddr = broadcastAddr.substr(0, lastDotPos + 1) + "255";
            }
            
            int hostCount = static_cast<int>(std::pow(2, 32 - prefixLength)) - 2;
            
            out->network_addressNull = FB_FALSE;
            out->network_address.length = static_cast<FB_USHORT>(networkAddr.length());
            networkAddr.copy(out->network_address.str, networkAddr.length());
            
            out->broadcast_addressNull = FB_FALSE;
            out->broadcast_address.length = static_cast<FB_USHORT>(broadcastAddr.length());
            broadcastAddr.copy(out->broadcast_address.str, broadcastAddr.length());
            
            out->host_countNull = FB_FALSE;
            out->host_count = hostCount;
            
            out->prefix_lengthNull = FB_FALSE;
            out->prefix_length = prefixLength;
        } else {
            // Invalid CIDR notation
            out->network_addressNull = FB_TRUE;
            out->broadcast_addressNull = FB_TRUE;
            out->host_countNull = FB_TRUE;
            out->prefix_lengthNull = FB_TRUE;
        }
    }
FB_UDR_END_FUNCTION

// Function: JSON path extraction
FB_UDR_BEGIN_FUNCTION(JsonExtractPath)
    FB_UDR_MESSAGE(InMessage,
        (FB_VARCHAR(8191), json_data)
        (FB_VARCHAR(100), json_path)
    );
    
    FB_UDR_MESSAGE(OutMessage,
        (FB_VARCHAR(1000), extracted_value)
    );
    
    FB_UDR_EXECUTE_FUNCTION
    {
        std::string jsonData = in->json_data.str;
        std::string jsonPath = in->json_path.str;
        
        // Simplified JSON path extraction
        // In a real implementation, you would use a proper JSON library
        std::string extractedValue = "null";
        
        // Simple case: extract top-level string value
        if (jsonPath.find('.') == std::string::npos) {
            std::string searchPattern = "\"" + jsonPath + "\":";
            size_t pos = jsonData.find(searchPattern);
            if (pos != std::string::npos) {
                pos += searchPattern.length();
                
                // Skip whitespace
                while (pos < jsonData.length() && std::isspace(jsonData[pos])) {
                    pos++;
                }
                
                if (pos < jsonData.length()) {
                    if (jsonData[pos] == '"') {
                        // String value
                        pos++;
                        size_t endPos = jsonData.find('"', pos);
                        if (endPos != std::string::npos) {
                            extractedValue = jsonData.substr(pos, endPos - pos);
                        }
                    } else {
                        // Numeric or boolean value
                        size_t endPos = jsonData.find_first_of(",}", pos);
                        if (endPos != std::string::npos) {
                            extractedValue = jsonData.substr(pos, endPos - pos);
                            // Trim whitespace
                            extractedValue.erase(0, extractedValue.find_first_not_of(" \t"));
                            extractedValue.erase(extractedValue.find_last_not_of(" \t") + 1);
                        }
                    }
                }
            }
        }
        
        out->extracted_valueNull = FB_FALSE;
        out->extracted_value.length = static_cast<FB_USHORT>(extractedValue.length());
        extractedValue.copy(out->extracted_value.str, extractedValue.length());
    }
FB_UDR_END_FUNCTION

// ================================================================
// UDR Procedures
// ================================================================

// Procedure: Create hierarchical schema structure
FB_UDR_BEGIN_PROCEDURE(CreateSchemaHierarchy)
    FB_UDR_MESSAGE(InMessage,
        (FB_VARCHAR(511), base_schema_path)
        (FB_INTEGER, max_levels)
    );
    
    FB_UDR_MESSAGE(OutMessage,
        (FB_VARCHAR(511), created_schema)
        (FB_INTEGER, schema_level)
        (FB_VARCHAR(100), status_message)
    );
    
    FB_UDR_EXECUTE_PROCEDURE
    {
        std::string baseSchemaPath = in->base_schema_path.str;
        int maxLevels = in->max_levels;
        
        // Generate schema hierarchy
        std::vector<std::string> subSchemas = {
            "users", "products", "orders", "reports", "settings", "logs"
        };
        
        for (int level = 1; level <= maxLevels && level <= static_cast<int>(subSchemas.size()); level++) {
            std::string schemaPath = baseSchemaPath + "." + subSchemas[level - 1];
            
            out->created_schemaNull = FB_FALSE;
            out->created_schema.length = static_cast<FB_USHORT>(schemaPath.length());
            schemaPath.copy(out->created_schema.str, schemaPath.length());
            
            out->schema_levelNull = FB_FALSE;
            out->schema_level = level;
            
            std::string statusMessage = "Schema created successfully";
            out->status_messageNull = FB_FALSE;
            out->status_message.length = static_cast<FB_USHORT>(statusMessage.length());
            statusMessage.copy(out->status_message.str, statusMessage.length());
            
            // Suspend execution to return this row
            procedure->suspend();
        }
    }
FB_UDR_END_PROCEDURE

// Procedure: Network diagnostics
FB_UDR_BEGIN_PROCEDURE(NetworkDiagnostics)
    FB_UDR_MESSAGE(InMessage,
        (FB_VARCHAR(45), target_ip)
        (FB_VARCHAR(20), diagnostic_type)
    );
    
    FB_UDR_MESSAGE(OutMessage,
        (FB_VARCHAR(100), diagnostic_result)
        (FB_TIMESTAMP, test_timestamp)
        (FB_INTEGER, response_time_ms)
    );
    
    FB_UDR_EXECUTE_PROCEDURE
    {
        std::string targetIp = in->target_ip.str;
        std::string diagnosticType = in->diagnostic_type.str;
        
        // Simulate network diagnostics
        std::vector<std::string> diagnosticTests = {
            "Ping test", "Port scan", "DNS resolution", "Traceroute", "Bandwidth test"
        };
        
        for (const auto& test : diagnosticTests) {
            if (diagnosticType == "ALL" || diagnosticType == test) {
                std::string result = test + " for " + targetIp + ": SUCCESS";
                
                out->diagnostic_resultNull = FB_FALSE;
                out->diagnostic_result.length = static_cast<FB_USHORT>(result.length());
                result.copy(out->diagnostic_result.str, result.length());
                
                // Current timestamp
                out->test_timestampNull = FB_FALSE;
                std::time_t now = std::time(nullptr);
                out->test_timestamp = now;
                
                // Random response time
                out->response_time_msNull = FB_FALSE;
                out->response_time_ms = (std::rand() % 100) + 10;
                
                procedure->suspend();
            }
        }
    }
FB_UDR_END_PROCEDURE

// ================================================================
// UDR Triggers
// ================================================================

// Trigger: Audit trail for hierarchical schema changes
FB_UDR_BEGIN_TRIGGER(SchemaAuditTrigger)
    FB_UDR_MESSAGE(FieldsMessage,
        (FB_VARCHAR(511), schema_name)
        (FB_VARCHAR(511), schema_path)
        (FB_INTEGER, schema_level)
        (FB_TIMESTAMP, created_at)
        (FB_VARCHAR(50), created_by)
    );
    
    FB_UDR_EXECUTE_TRIGGER
    {
        // This trigger would be attached to RDB$SCHEMAS table
        // to log schema creation/modification activities
        
        if (trigger->getAction() == IExternalTrigger::ACTION_INSERT) {
            // Log schema creation
            std::string auditMessage = "Schema created: " + std::string(newFields->schema_name.str);
            
            // In a real implementation, you would insert into an audit table
            // For this example, we'll just process the trigger
            
            // You could also validate schema naming conventions here
            std::string schemaName = newFields->schema_name.str;
            if (schemaName.find("..") != std::string::npos) {
                // Invalid schema name with consecutive dots
                throw std::runtime_error("Invalid schema name: consecutive dots not allowed");
            }
        }
        
        if (trigger->getAction() == IExternalTrigger::ACTION_UPDATE) {
            // Log schema modification
            std::string auditMessage = "Schema modified: " + std::string(newFields->schema_name.str);
            
            // Could implement schema path validation here
            int newLevel = newFields->schema_level;
            if (newLevel > 8) {
                throw std::runtime_error("Schema level exceeds maximum depth of 8");
            }
        }
        
        if (trigger->getAction() == IExternalTrigger::ACTION_DELETE) {
            // Log schema deletion
            std::string auditMessage = "Schema deleted: " + std::string(oldFields->schema_name.str);
            
            // Could implement cascade deletion validation here
        }
    }
FB_UDR_END_TRIGGER

// Trigger: Network data validation
FB_UDR_BEGIN_TRIGGER(NetworkDataValidationTrigger)
    FB_UDR_MESSAGE(FieldsMessage,
        (FB_VARCHAR(45), ip_address)
        (FB_VARCHAR(17), mac_address)
        (FB_VARCHAR(18), network_cidr)
        (FB_TIMESTAMP, last_updated)
    );
    
    FB_UDR_EXECUTE_TRIGGER
    {
        if (trigger->getAction() == IExternalTrigger::ACTION_INSERT || 
            trigger->getAction() == IExternalTrigger::ACTION_UPDATE) {
            
            // Validate IP address format
            std::string ipAddress = newFields->ip_address.str;
            if (!ipAddress.empty()) {
                std::regex ipv4Regex(R"(^(\d{1,3}\.){3}\d{1,3}$)");
                if (!std::regex_match(ipAddress, ipv4Regex)) {
                    throw std::runtime_error("Invalid IP address format");
                }
            }
            
            // Validate MAC address format
            std::string macAddress = newFields->mac_address.str;
            if (!macAddress.empty()) {
                std::regex macRegex(R"(^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$)");
                if (!std::regex_match(macAddress, macRegex)) {
                    throw std::runtime_error("Invalid MAC address format");
                }
            }
            
            // Validate CIDR notation
            std::string networkCidr = newFields->network_cidr.str;
            if (!networkCidr.empty()) {
                std::regex cidrRegex(R"(^(\d{1,3}\.){3}\d{1,3}/\d{1,2}$)");
                if (!std::regex_match(networkCidr, cidrRegex)) {
                    throw std::runtime_error("Invalid CIDR notation");
                }
            }
            
            // Update timestamp
            newFields->last_updated = std::time(nullptr);
        }
    }
FB_UDR_END_TRIGGER

// ================================================================
// UDR Registration
// ================================================================

FB_UDR_IMPLEMENT_ENTRY_POINT
{
    // Register functions
    factory->registerFunction<GenerateSchemaPathFunction>("generate_schema_path");
    factory->registerFunction<ValidateNetworkAddressFunction>("validate_network_address");
    factory->registerFunction<CalculateSubnetInfoFunction>("calculate_subnet_info");
    factory->registerFunction<JsonExtractPathFunction>("json_extract_path");
    
    // Register procedures
    factory->registerProcedure<CreateSchemaHierarchyProcedure>("create_schema_hierarchy");
    factory->registerProcedure<NetworkDiagnosticsProcedure>("network_diagnostics");
    
    // Register triggers
    factory->registerTrigger<SchemaAuditTrigger>("schema_audit_trigger");
    factory->registerTrigger<NetworkDataValidationTrigger>("network_data_validation_trigger");
}

/*
 * Build Instructions:
 * 
 * Linux:
 * g++ -std=c++17 -fPIC -shared -I../include -o libScratchBirdUDR.so ScratchBirdUDR.cpp -lsbclient
 * 
 * Installation:
 * 1. Copy libScratchBirdUDR.so to ScratchBird plugins directory
 * 2. Register the UDR functions, procedures, and triggers in your database
 * 
 * Usage Examples:
 * 
 * -- Register UDR functions
 * CREATE FUNCTION generate_schema_path(schema_name VARCHAR(63), level INTEGER, parent_name VARCHAR(63))
 * RETURNS VARCHAR(511)
 * EXTERNAL NAME 'libScratchBirdUDR!generate_schema_path'
 * ENGINE UDR;
 * 
 * -- Register UDR procedures
 * CREATE PROCEDURE create_schema_hierarchy(base_schema_path VARCHAR(511), max_levels INTEGER)
 * RETURNS (created_schema VARCHAR(511), schema_level INTEGER, status_message VARCHAR(100))
 * EXTERNAL NAME 'libScratchBirdUDR!create_schema_hierarchy'
 * ENGINE UDR;
 * 
 * -- Use the functions
 * SELECT generate_schema_path('users', 2, 'company') FROM RDB$DATABASE;
 * 
 * -- Call procedures
 * EXECUTE PROCEDURE create_schema_hierarchy('company', 3);
 * 
 * This UDR module demonstrates:
 * - Custom functions for hierarchical schema management
 * - Network data type validation and processing
 * - JSON data extraction capabilities
 * - Audit triggers for schema changes
 * - Data validation triggers for network types
 * - Modern C++ UDR development patterns
 */