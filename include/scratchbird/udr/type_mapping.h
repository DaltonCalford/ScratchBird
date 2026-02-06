/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * Complete Type Mapping System
 * 
 * Maps between ScratchBird internal types and external database types:
 * - PostgreSQL OIDs
 * - MySQL field types  
 * - Firebird BLR types
 * - SBWP type codes
 */

#include "scratchbird/core/types.h"
#include <string>

namespace scratchbird {
namespace udr {

/**
 * Type mapping utility class
 * 
 * Provides bidirectional mapping between ScratchBird's internal DataType
 * enum and external database type representations.
 */
class TypeMapping {
public:
    // ========================================================================
    // PostgreSQL Type Mapping
    // ========================================================================
    
    /**
     * Convert ScratchBird DataType to PostgreSQL OID
     */
    static uint32_t toPostgreSQL(core::DataType type);
    
    /**
     * Convert PostgreSQL OID to ScratchBird DataType
     */
    static core::DataType fromPostgreSQL(uint32_t oid);
    
    // ========================================================================
    // MySQL Type Mapping
    // ========================================================================
    
    /**
     * Convert ScratchBird DataType to MySQL field type
     */
    static uint8_t toMySQL(core::DataType type);
    
    /**
     * Convert MySQL field type to ScratchBird DataType
     */
    static core::DataType fromMySQL(uint8_t mysql_type);
    
    // ========================================================================
    // Firebird Type Mapping
    // ========================================================================
    
    /**
     * Convert ScratchBird DataType to Firebird BLR type
     */
    static uint32_t toFirebird(core::DataType type);
    
    /**
     * Convert Firebird BLR type to ScratchBird DataType
     */
    static core::DataType fromFirebird(uint32_t blr_type);
    
    // ========================================================================
    // SBWP Type Mapping
    // ========================================================================
    
    /**
     * Convert ScratchBird DataType to SBWP type code
     */
    static uint32_t toSBWP(core::DataType type);
    
    /**
     * Convert SBWP type code to ScratchBird DataType
     */
    static core::DataType fromSBWP(uint32_t type_code);
    
    // ========================================================================
    // Array Type Handling
    // ========================================================================
    
    /**
     * Get element type from PostgreSQL array type OID
     */
    static uint32_t getArrayElementType(uint32_t array_type_oid);
    
    /**
     * Get PostgreSQL array type OID from element type
     */
    static uint32_t getArrayTypeOid(uint32_t element_type_oid);
    
    /**
     * Check if PostgreSQL OID is an array type
     */
    static bool isPostgreSQLArray(uint32_t oid);
    
    // ========================================================================
    // Type Information
    // ========================================================================
    
    /**
     * Get human-readable type name
     */
    static std::string getTypeName(core::DataType type);
    
    /**
     * Get fixed size for type (0 for variable-length)
     */
    static uint32_t getTypeSize(core::DataType type);
    
    /**
     * Check if type is variable-length
     */
    static bool isVariableLength(core::DataType type);
    
    /**
     * Check if type is numeric
     */
    static bool isNumericType(core::DataType type);
    
    /**
     * Check if type is string/character
     */
    static bool isStringType(core::DataType type);
    
    /**
     * Check if type is temporal (date/time)
     */
    static bool isTemporalType(core::DataType type);
    
    /**
     * Check if type is binary
     */
    static bool isBinaryType(core::DataType type);
    
    /**
     * Get alignment requirement for type
     */
    static uint8_t getAlignment(core::DataType type);
};

} // namespace udr
} // namespace scratchbird
