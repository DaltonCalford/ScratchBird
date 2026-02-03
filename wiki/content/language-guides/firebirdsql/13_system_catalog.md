# System Catalog

**Last Updated:** 2026-02-03

---

Firebird metadata is exposed via RDB$ system tables, MON$ monitoring tables,
and SEC$ security tables. Tables listed here include strict per-column status
and source mapping based on the Firebird 5.0 reference and the current
ScratchBird Firebird catalog handler.

Statuses:
- ScratchBird tracked: populated from a ScratchBird runtime source or constant.
- Always NULL: column exists but is never populated.
- Always 0: column is always returned as 0/false.

---

## RDB$AUTH_MAPPING

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$MAP_NAME` | Always NULL | Schema-only view required | Never. |
| `RDB$MAP_USING` | Always NULL | Schema-only view required | Never. |
| `RDB$MAP_PLUGIN` | Always NULL | Schema-only view required | Never. |
| `RDB$MAP_DB` | Always NULL | Schema-only view required | Never. |
| `RDB$MAP_FROM_TYPE` | Always NULL | Schema-only view required | Never. |
| `RDB$MAP_FROM` | Always NULL | Schema-only view required | Never. |
| `RDB$MAP_TO_TYPE` | Always NULL | Schema-only view required | Never. |
| `RDB$MAP_TO` | Always NULL | Schema-only view required | Never. |
| `RDB$SYSTEM_FLAG` | Always NULL | Schema-only view required | Never. |
| `RDB$DESCRIPTION` | Always NULL | Schema-only view required | Never. |

## RDB$BACKUP_HISTORY

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$BACKUP_ID` | Always NULL | Schema-only view required | Never. |
| `RDB$TIMESTAMP` | Always NULL | Schema-only view required | Never. |
| `RDB$BACKUP_LEVEL` | Always NULL | Schema-only view required | Never. |
| `RDB$GUID` | Always NULL | Schema-only view required | Never. |
| `RDB$SCN` | Always NULL | Schema-only view required | Never. |
| `RDB$FILE_NAME` | Always NULL | Schema-only view required | Never. |

## RDB$CHARACTER_SETS

Table status: Implemented
Source: queryRdbCharacterSets

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$CHARACTER_SET_NAME` | ScratchBird tracked | queryRdbCharacterSets | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$FORM_OF_USE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$NUMBER_OF_CHARACTERS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DEFAULT_COLLATE_NAME` | ScratchBird tracked | queryRdbCharacterSets | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$CHARACTER_SET_ID` | ScratchBird tracked | queryRdbCharacterSets | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$SYSTEM_FLAG` | ScratchBird tracked | Constant 1 | Always. |
| `RDB$DESCRIPTION` | Always NULL | Constant NULL | Always. |
| `RDB$FUNCTION_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$BYTES_PER_CHARACTER` | ScratchBird tracked | queryRdbCharacterSets | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$SECURITY_CLASS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$OWNER_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$CHECK_CONSTRAINTS

Table status: Implemented
Source: queryRdbCheckConstraints

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$CONSTRAINT_NAME` | ScratchBird tracked | queryRdbCheckConstraints | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$TRIGGER_NAME` | ScratchBird tracked | queryRdbCheckConstraints | Row emitted; value derived from ScratchBird catalog or constants. |

## RDB$COLLATIONS

Table status: Implemented
Source: queryRdbCollations

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$COLLATION_NAME` | ScratchBird tracked | queryRdbCollations | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$COLLATION_ID` | Always 0 | Constant 0/false | Always. |
| `RDB$CHARACTER_SET_ID` | ScratchBird tracked | Constant 4 | Always. |
| `RDB$COLLATION_ATTRIBUTES` | Always 0 | Constant 0/false | Always. |
| `RDB$SYSTEM_FLAG` | ScratchBird tracked | Constant 1 | Always. |
| `RDB$DESCRIPTION` | Always NULL | Constant NULL | Always. |
| `RDB$FUNCTION_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$BASE_COLLATION_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$SPECIFIC_ATTRIBUTES` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$SECURITY_CLASS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$OWNER_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$CONFIG

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$CONFIG_ID` | Always NULL | Schema-only view required | Never. |
| `RDB$CONFIG_NAME` | Always NULL | Schema-only view required | Never. |
| `RDB$CONFIG_VALUE` | Always NULL | Schema-only view required | Never. |
| `RDB$CONFIG_DEFAULT` | Always NULL | Schema-only view required | Never. |
| `RDB$CONFIG_IS_SET` | Always NULL | Schema-only view required | Never. |
| `RDB$CONFIG_SOURCE` | Always NULL | Schema-only view required | Never. |

## RDB$DATABASE

Table status: Implemented
Source: queryRdbDatabase

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$DESCRIPTION` | ScratchBird tracked | queryRdbDatabase | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$RELATION_ID` | Always 0 | Constant 0/false | Always. |
| `RDB$SECURITY_CLASS` | Always NULL | Constant NULL | Always. |
| `RDB$CHARACTER_SET_NAME` | ScratchBird tracked | Constant "UTF8" | Always. |
| `RDB$LINGER` | Always 0 | Constant 0/false | Always. |
| `RDB$SQL_SECURITY` | ScratchBird tracked | Constant true | Always. |

## RDB$DB_CREATORS

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$USER` | Always NULL | Schema-only view required | Never. |
| `RDB$USER_TYPE` | Always NULL | Schema-only view required | Never. |

## RDB$DEPENDENCIES

Table status: Implemented
Source: queryRdbDependencies

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$DEPENDENT_NAME` | ScratchBird tracked | queryRdbDependencies | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$DEPENDED_ON_NAME` | ScratchBird tracked | queryRdbDependencies | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$FIELD_NAME` | Always NULL | Constant NULL | Always. |
| `RDB$DEPENDENT_TYPE` | ScratchBird tracked | queryRdbDependencies | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$DEPENDED_ON_TYPE` | ScratchBird tracked | queryRdbDependencies | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$PACKAGE_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$EXCEPTIONS

Table status: Implemented
Source: queryRdbExceptions

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$EXCEPTION_NAME` | ScratchBird tracked | queryRdbExceptions | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$EXCEPTION_NUMBER` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$MESSAGE` | ScratchBird tracked | queryRdbExceptions | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$DESCRIPTION` | Always NULL | Constant NULL | Always. |
| `RDB$SYSTEM_FLAG` | Always 0 | Constant 0/false | Always. |
| `RDB$SECURITY_CLASS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$OWNER_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$FIELDS

Table status: Implemented
Source: queryRdbFields

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$FIELD_NAME` | ScratchBird tracked | queryRdbFields | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$QUERY_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$VALIDATION_BLR` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$VALIDATION_SOURCE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$COMPUTED_BLR` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$COMPUTED_SOURCE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DEFAULT_VALUE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DEFAULT_SOURCE` | Always NULL | Constant NULL | Always. |
| `RDB$FIELD_LENGTH` | ScratchBird tracked | queryRdbFields | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$FIELD_SCALE` | ScratchBird tracked | queryRdbFields | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$FIELD_TYPE` | ScratchBird tracked | queryRdbFields | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$FIELD_SUB_TYPE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$MISSING_VALUE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$MISSING_SOURCE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DESCRIPTION` | Always NULL | Constant NULL | Always. |
| `RDB$SYSTEM_FLAG` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$QUERY_HEADER` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$SEGMENT_LENGTH` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$EDIT_STRING` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$EXTERNAL_LENGTH` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$EXTERNAL_SCALE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$EXTERNAL_TYPE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DIMENSIONS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$NULL_FLAG` | Always NULL | Constant NULL | Always. |
| `RDB$CHARACTER_LENGTH` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$COLLATION_ID` | ScratchBird tracked | queryRdbFields | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$CHARACTER_SET_ID` | ScratchBird tracked | queryRdbFields | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$FIELD_PRECISION` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$SECURITY_CLASS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$OWNER_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$FIELD_DIMENSIONS

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$FIELD_NAME` | Always NULL | Schema-only view required | Never. |
| `RDB$DIMENSION` | Always NULL | Schema-only view required | Never. |
| `RDB$LOWER_BOUND` | Always NULL | Schema-only view required | Never. |
| `RDB$UPPER_BOUND` | Always NULL | Schema-only view required | Never. |

## RDB$FILES

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$FILE_NAME` | Always NULL | Schema-only view required | Never. |
| `RDB$FILE_SEQUENCE` | Always NULL | Schema-only view required | Never. |
| `RDB$FILE_START` | Always NULL | Schema-only view required | Never. |
| `RDB$FILE_LENGTH` | Always NULL | Schema-only view required | Never. |
| `RDB$FILE_FLAGS` | Always NULL | Schema-only view required | Never. |
| `RDB$SHADOW_NUMBER` | Always NULL | Schema-only view required | Never. |

## RDB$FILTERS

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$FUNCTION_NAME` | Always NULL | Schema-only view required | Never. |
| `RDB$DESCRIPTION` | Always NULL | Schema-only view required | Never. |
| `RDB$MODULE_NAME` | Always NULL | Schema-only view required | Never. |
| `RDB$ENTRYPOINT` | Always NULL | Schema-only view required | Never. |
| `RDB$INPUT_SUB_TYPE` | Always NULL | Schema-only view required | Never. |
| `RDB$OUTPUT_SUB_TYPE` | Always NULL | Schema-only view required | Never. |
| `RDB$SYSTEM_FLAG` | Always NULL | Schema-only view required | Never. |
| `RDB$SECURITY_CLASS` | Always NULL | Schema-only view required | Never. |
| `RDB$OWNER_NAME` | Always NULL | Schema-only view required | Never. |

## RDB$FORMATS

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$RELATION_ID` | Always NULL | Schema-only view required | Never. |
| `RDB$FORMAT` | Always NULL | Schema-only view required | Never. |
| `RDB$DESCRIPTOR` | Always NULL | Schema-only view required | Never. |

## RDB$FUNCTIONS

Table status: Implemented
Source: queryRdbFunctions

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$FUNCTION_NAME` | ScratchBird tracked | queryRdbFunctions | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$FUNCTION_TYPE` | Always 0 | Constant 0/false | Always. |
| `RDB$QUERY_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DESCRIPTION` | Always NULL | Constant NULL | Always. |
| `RDB$MODULE_NAME` | Always NULL | Constant NULL | Always. |
| `RDB$ENTRYPOINT` | Always NULL | Constant NULL | Always. |
| `RDB$RETURN_ARGUMENT` | Always 0 | Constant 0/false | Always. |
| `RDB$SYSTEM_FLAG` | Always 0 | Constant 0/false | Always. |
| `RDB$ENGINE_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$PACKAGE_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$PRIVATE_FLAG` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$FUNCTION_SOURCE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$FUNCTION_ID` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$FUNCTION_BLR` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$VALID_BLR` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DEBUG_INFO` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$SECURITY_CLASS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$OWNER_NAME` | ScratchBird tracked | Constant "SYSDBA" | Always. |
| `RDB$LEGACY_FLAG` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DETERMINISTIC_FLAG` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$SQL_SECURITY` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$FUNCTION_ARGUMENTS

Table status: Implemented
Source: queryRdbFunctionArguments

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$FUNCTION_NAME` | ScratchBird tracked | queryRdbFunctionArguments | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$ARGUMENT_POSITION` | ScratchBird tracked | queryRdbFunctionArguments | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$MECHANISM` | Always 0 | Constant 0/false | Always. |
| `RDB$FIELD_TYPE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$FIELD_SCALE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$FIELD_LENGTH` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$FIELD_SUB_TYPE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$CHARACTER_SET_ID` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$FIELD_PRECISION` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$CHARACTER_LENGTH` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$PACKAGE_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$ARGUMENT_NAME` | ScratchBird tracked | queryRdbFunctionArguments | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$FIELD_SOURCE` | ScratchBird tracked | queryRdbFunctionArguments | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$DEFAULT_VALUE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DEFAULT_SOURCE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$COLLATION_ID` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$NULL_FLAG` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$ARGUMENT_MECHANISM` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$FIELD_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$RELATION_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$SYSTEM_FLAG` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DESCRIPTION` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$GENERATORS

Table status: Implemented
Source: queryRdbGenerators

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$GENERATOR_NAME` | ScratchBird tracked | queryRdbGenerators | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$GENERATOR_ID` | ScratchBird tracked | queryRdbGenerators | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$SYSTEM_FLAG` | Always 0 | Constant 0/false | Always. |
| `RDB$DESCRIPTION` | Always NULL | Constant NULL | Always. |
| `RDB$SECURITY_CLASS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$OWNER_NAME` | ScratchBird tracked | Constant "SYSDBA" | Always. |
| `RDB$INITIAL_VALUE` | ScratchBird tracked | queryRdbGenerators | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$GENERATOR_INCREMENT` | ScratchBird tracked | queryRdbGenerators | Row emitted; value derived from ScratchBird catalog or constants. |

## RDB$INDEX_SEGMENTS

Table status: Implemented
Source: queryRdbIndexSegments

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$INDEX_NAME` | ScratchBird tracked | queryRdbIndexSegments | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$FIELD_NAME` | ScratchBird tracked | queryRdbIndexSegments | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$FIELD_POSITION` | ScratchBird tracked | queryRdbIndexSegments | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$STATISTICS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$INDICES

Table status: Implemented
Source: queryRdbIndices

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$INDEX_NAME` | ScratchBird tracked | queryRdbIndices | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$RELATION_NAME` | ScratchBird tracked | queryRdbIndices | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$INDEX_ID` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$UNIQUE_FLAG` | ScratchBird tracked | queryRdbIndices | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$DESCRIPTION` | Always NULL | Constant NULL | Always. |
| `RDB$SEGMENT_COUNT` | ScratchBird tracked | queryRdbIndices | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$INDEX_INACTIVE` | Always 0 | Constant 0/false | Always. |
| `RDB$INDEX_TYPE` | Always 0 | Constant 0/false | Always. |
| `RDB$FOREIGN_KEY` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$SYSTEM_FLAG` | Always 0 | Constant 0/false | Always. |
| `RDB$EXPRESSION_BLR` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$EXPRESSION_SOURCE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$STATISTICS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$CONDITION_BLR` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$CONDITION_SOURCE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$KEYWORDS

Table status: Implemented
Source: queryRdbKeywords

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$KEYWORD_NAME` | ScratchBird tracked | queryRdbKeywords | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$KEYWORD_RESERVED` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$PACKAGES

Table status: Implemented
Source: queryRdbPackages

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$PACKAGE_NAME` | ScratchBird tracked | queryRdbPackages | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$PACKAGE_HEADER_SOURCE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$PACKAGE_BODY_SOURCE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$VALID_BODY_FLAG` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$SECURITY_CLASS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$OWNER_NAME` | ScratchBird tracked | Constant "SYSDBA" | Always. |
| `RDB$SYSTEM_FLAG` | Always 0 | Constant 0/false | Always. |
| `RDB$DESCRIPTION` | Always NULL | Constant NULL | Always. |
| `RDB$SQL_SECURITY` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$PAGES

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$PAGE_NUMBER` | Always NULL | Schema-only view required | Never. |
| `RDB$RELATION_ID` | Always NULL | Schema-only view required | Never. |
| `RDB$PAGE_SEQUENCE` | Always NULL | Schema-only view required | Never. |
| `RDB$PAGE_TYPE` | Always NULL | Schema-only view required | Never. |

## RDB$PROCEDURES

Table status: Implemented
Source: queryRdbProcedures

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$PROCEDURE_NAME` | ScratchBird tracked | queryRdbProcedures | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$PROCEDURE_ID` | ScratchBird tracked | queryRdbProcedures | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$PROCEDURE_INPUTS` | ScratchBird tracked | queryRdbProcedures | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$PROCEDURE_OUTPUTS` | ScratchBird tracked | queryRdbProcedures | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$DESCRIPTION` | Always NULL | Constant NULL | Always. |
| `RDB$PROCEDURE_SOURCE` | Always NULL | Constant NULL | Always. |
| `RDB$PROCEDURE_BLR` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$SECURITY_CLASS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$OWNER_NAME` | ScratchBird tracked | Constant "SYSDBA" | Always. |
| `RDB$RUNTIME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$SYSTEM_FLAG` | Always 0 | Constant 0/false | Always. |
| `RDB$PROCEDURE_TYPE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$VALID_BLR` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DEBUG_INFO` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$ENGINE_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$ENTRYPOINT` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$PACKAGE_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$PRIVATE_FLAG` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$SQL_SECURITY` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$PROCEDURE_PARAMETERS

Table status: Implemented
Source: queryRdbProcedureParameters

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$PARAMETER_NAME` | ScratchBird tracked | queryRdbProcedureParameters | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$PROCEDURE_NAME` | ScratchBird tracked | queryRdbProcedureParameters | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$PARAMETER_NUMBER` | ScratchBird tracked | queryRdbProcedureParameters | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$PARAMETER_TYPE` | ScratchBird tracked | queryRdbProcedureParameters | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$FIELD_SOURCE` | ScratchBird tracked | queryRdbProcedureParameters | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$DESCRIPTION` | Always NULL | Constant NULL | Always. |
| `RDB$SYSTEM_FLAG` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DEFAULT_VALUE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DEFAULT_SOURCE` | Always NULL | Constant NULL | Always. |
| `RDB$COLLATION_ID` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$NULL_FLAG` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$PARAMETER_MECHANISM` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$FIELD_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$RELATION_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$PACKAGE_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$PUBLICATIONS

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$PUBLICATION_NAME` | Always NULL | Schema-only view required | Never. |
| `RDB$OWNER_NAME` | Always NULL | Schema-only view required | Never. |
| `RDB$SYSTEM_FLAG` | Always NULL | Schema-only view required | Never. |
| `RDB$ACTIVE_FLAG` | Always NULL | Schema-only view required | Never. |
| `RDB$AUTO_ENABLE` | Always NULL | Schema-only view required | Never. |

## RDB$PUBLICATION_TABLES

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$PUBLICATION_NAME` | Always NULL | Schema-only view required | Never. |
| `RDB$TABLE_NAME` | Always NULL | Schema-only view required | Never. |

## RDB$REF_CONSTRAINTS

Table status: Implemented
Source: queryRdbRefConstraints

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$CONSTRAINT_NAME` | ScratchBird tracked | queryRdbRefConstraints | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$CONST_NAME_UQ` | Always NULL | Constant NULL | Always. |
| `RDB$MATCH_OPTION` | ScratchBird tracked | queryRdbRefConstraints | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$UPDATE_RULE` | ScratchBird tracked | queryRdbRefConstraints | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$DELETE_RULE` | ScratchBird tracked | queryRdbRefConstraints | Row emitted; value derived from ScratchBird catalog or constants. |

## RDB$RELATIONS

Table status: Implemented
Source: queryRdbRelations

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$VIEW_BLR` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$VIEW_SOURCE` | Always NULL | Constant NULL | Always. |
| `RDB$DESCRIPTION` | Always NULL | Constant NULL | Always. |
| `RDB$RELATION_ID` | ScratchBird tracked | queryRdbRelations | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$SYSTEM_FLAG` | Always 0 | Constant 0/false | Always. |
| `RDB$DBKEY_LENGTH` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$FORMAT` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$FIELD_ID` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$RELATION_NAME` | ScratchBird tracked | queryRdbRelations | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$SECURITY_CLASS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$EXTERNAL_FILE` | Always NULL | Constant NULL | Always. |
| `RDB$RUNTIME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$EXTERNAL_DESCRIPTION` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$OWNER_NAME` | ScratchBird tracked | Constant "SYSDBA" | Always. |
| `RDB$DEFAULT_CLASS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$FLAGS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$RELATION_TYPE` | Always 0 | Constant 0/false | Always. |
| `RDB$SQL_SECURITY` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$RELATION_CONSTRAINTS

Table status: Implemented
Source: queryRdbRelationConstraints

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$CONSTRAINT_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$CONSTRAINT_TYPE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$RELATION_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DEFERRABLE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$INITIALLY_DEFERRED` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$INDEX_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$RELATION_FIELDS

Table status: Implemented
Source: queryRdbRelationFields

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$FIELD_NAME` | ScratchBird tracked | queryRdbRelationFields | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$RELATION_NAME` | ScratchBird tracked | queryRdbRelationFields | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$FIELD_SOURCE` | ScratchBird tracked | queryRdbRelationFields | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$QUERY_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$BASE_FIELD` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$EDIT_STRING` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$FIELD_POSITION` | ScratchBird tracked | queryRdbRelationFields | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$QUERY_HEADER` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$UPDATE_FLAG` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$FIELD_ID` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$VIEW_CONTEXT` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DESCRIPTION` | Always NULL | Constant NULL | Always. |
| `RDB$DEFAULT_VALUE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$SYSTEM_FLAG` | Always 0 | Constant 0/false | Always. |
| `RDB$SECURITY_CLASS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$COMPLEX_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$NULL_FLAG` | Always NULL | Constant NULL | Always. |
| `RDB$DEFAULT_SOURCE` | Always NULL | Constant NULL | Always. |
| `RDB$COLLATION_ID` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$GENERATOR_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$IDENTITY_TYPE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$ROLES

Table status: Implemented
Source: queryRdbRoles

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$ROLE_NAME` | ScratchBird tracked | queryRdbRoles | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$OWNER_NAME` | ScratchBird tracked | Constant "SYSDBA" | Always. |
| `RDB$DESCRIPTION` | ScratchBird tracked | Constant "Administrator role" | Always. |
| `RDB$SYSTEM_FLAG` | ScratchBird tracked | Constant 1 | Always. |
| `RDB$SECURITY_CLASS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$SYSTEM_PRIVILEGES` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$SECURITY_CLASSES

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$SECURITY_CLASS` | Always NULL | Schema-only view required | Never. |
| `RDB$ACL` | Always NULL | Schema-only view required | Never. |
| `RDB$DESCRIPTION` | Always NULL | Schema-only view required | Never. |

## RDB$TIME_ZONES

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$TIME_ZONE_ID` | Always NULL | Schema-only view required | Never. |
| `RDB$TIME_ZONE_NAME` | Always NULL | Schema-only view required | Never. |

## RDB$TRANSACTIONS

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$TRANSACTION_ID` | Always NULL | Schema-only view required | Never. |
| `RDB$TRANSACTION_STATE` | Always NULL | Schema-only view required | Never. |
| `RDB$TIMESTAMP` | Always NULL | Schema-only view required | Never. |
| `RDB$TRANSACTION_DESCRIPTION` | Always NULL | Schema-only view required | Never. |

## RDB$TRIGGERS

Table status: Implemented
Source: queryRdbTriggers

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$TRIGGER_NAME` | ScratchBird tracked | queryRdbTriggers | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$RELATION_NAME` | ScratchBird tracked | queryRdbTriggers | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$TRIGGER_SEQUENCE` | ScratchBird tracked | queryRdbTriggers | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$TRIGGER_TYPE` | ScratchBird tracked | queryRdbTriggers | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$TRIGGER_SOURCE` | Always NULL | Constant NULL | Always. |
| `RDB$TRIGGER_BLR` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DESCRIPTION` | Always NULL | Constant NULL | Always. |
| `RDB$TRIGGER_INACTIVE` | ScratchBird tracked | queryRdbTriggers | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$SYSTEM_FLAG` | Always 0 | Constant 0/false | Always. |
| `RDB$FLAGS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$VALID_BLR` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$DEBUG_INFO` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$ENGINE_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$ENTRYPOINT` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$SQL_SECURITY` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## RDB$TRIGGER_MESSAGES

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$TRIGGER_NAME` | Always NULL | Schema-only view required | Never. |
| `RDB$MESSAGE_NUMBER` | Always NULL | Schema-only view required | Never. |
| `RDB$MESSAGE` | Always NULL | Schema-only view required | Never. |

## RDB$TYPES

Table status: Implemented
Source: queryRdbTypes

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$FIELD_NAME` | ScratchBird tracked | queryRdbTypes | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$TYPE` | ScratchBird tracked | queryRdbTypes | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$TYPE_NAME` | ScratchBird tracked | queryRdbTypes | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$DESCRIPTION` | Always NULL | Constant NULL | Always. |
| `RDB$SYSTEM_FLAG` | ScratchBird tracked | Constant 1 | Always. |

## RDB$USER_PRIVILEGES

Table status: Implemented
Source: queryRdbUserPrivileges

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$USER` | ScratchBird tracked | queryRdbUserPrivileges | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$GRANTOR` | ScratchBird tracked | queryRdbUserPrivileges | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$PRIVILEGE` | ScratchBird tracked | queryRdbUserPrivileges | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$GRANT_OPTION` | ScratchBird tracked | queryRdbUserPrivileges | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$RELATION_NAME` | ScratchBird tracked | queryRdbUserPrivileges | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$FIELD_NAME` | ScratchBird tracked | queryRdbUserPrivileges | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$USER_TYPE` | ScratchBird tracked | queryRdbUserPrivileges | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$OBJECT_TYPE` | ScratchBird tracked | queryRdbUserPrivileges | Row emitted; value derived from ScratchBird catalog or constants. |

## RDB$VIEW_RELATIONS

Table status: Implemented
Source: queryRdbViewRelations

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `RDB$VIEW_NAME` | ScratchBird tracked | queryRdbViewRelations | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$RELATION_NAME` | ScratchBird tracked | queryRdbViewRelations | Row emitted; value derived from ScratchBird catalog or constants. |
| `RDB$VIEW_CONTEXT` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$CONTEXT_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$CONTEXT_TYPE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `RDB$PACKAGE_NAME` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## MON$ATTACHMENTS

Table status: Implemented
Source: queryMonAttachments

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `MON$ATTACHMENT_ID` | ScratchBird tracked | queryMonAttachments | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$SERVER_PID` | Always NULL | Constant NULL | Always. |
| `MON$STATE` | ScratchBird tracked | queryMonAttachments | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$ATTACHMENT_NAME` | ScratchBird tracked | queryMonAttachments | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$USER` | ScratchBird tracked | queryMonAttachments | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$ROLE` | ScratchBird tracked | queryMonAttachments | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$REMOTE_PROTOCOL` | ScratchBird tracked | queryMonAttachments | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$REMOTE_ADDRESS` | ScratchBird tracked | queryMonAttachments | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$REMOTE_PID` | Always NULL | Constant NULL | Always. |
| `MON$CHARACTER_SET_ID` | ScratchBird tracked | Constant 4 | Always. |
| `MON$TIMESTAMP` | Always NULL | Constant NULL | Always. |
| `MON$GARBAGE_COLLECTION` | ScratchBird tracked | Constant 1 | Always. |
| `MON$REMOTE_PROCESS` | Always NULL | Constant NULL | Always. |
| `MON$STAT_ID` | Always NULL | Constant NULL | Always. |
| `MON$CLIENT_VERSION` | ScratchBird tracked | Constant "ScratchBird" | Always. |
| `MON$REMOTE_VERSION` | ScratchBird tracked | Constant "ScratchBird" | Always. |
| `MON$REMOTE_HOST` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$REMOTE_OS_USER` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$AUTH_METHOD` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$SYSTEM_FLAG` | Always 0 | Constant 0/false | Always. |
| `MON$IDLE_TIMEOUT` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$IDLE_TIMER` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$STATEMENT_TIMEOUT` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$WIRE_COMPRESSED` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$WIRE_ENCRYPTED` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$WIRE_CRYPT_PLUGIN` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$SESSION_TIMEZONE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$PARALLEL_WORKERS` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## MON$COMPILED_STATEMENTS

Table status: Implemented
Source: queryMonCompiledStatements

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `MON$COMPILED_STATEMENT_ID` | ScratchBird tracked | queryMonCompiledStatements | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$SQL_TEXT` | ScratchBird tracked | queryMonCompiledStatements | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$EXPLAINED_PLAN` | Always NULL | Constant NULL | Always. |
| `MON$OBJECT_NAME` | Always NULL | Constant NULL | Always. |
| `MON$OBJECT_TYPE` | Always NULL | Constant NULL | Always. |
| `MON$PACKAGE_NAME` | Always NULL | Constant NULL | Always. |
| `MON$STAT_ID` | ScratchBird tracked | queryMonCompiledStatements | Row emitted; value derived from ScratchBird catalog or constants. |

## MON$CALL_STACK

Table status: Implemented
Source: queryMonCallStack

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `MON$CALL_ID` | ScratchBird tracked | queryMonCallStack | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$STATEMENT_ID` | Always NULL | Constant NULL | Always. |
| `MON$CALLER_ID` | Always NULL | Constant NULL | Always. |
| `MON$OBJECT_NAME` | Always NULL | Constant NULL | Always. |
| `MON$OBJECT_TYPE` | Always NULL | Constant NULL | Always. |
| `MON$TIMESTAMP` | ScratchBird tracked | queryMonCallStack | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$SOURCE_LINE` | ScratchBird tracked | queryMonCallStack | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$SOURCE_COLUMN` | ScratchBird tracked | queryMonCallStack | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$STAT_ID` | ScratchBird tracked | queryMonCallStack | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$PACKAGE_NAME` | Always NULL | Constant NULL | Always. |
| `MON$COMPILED_STATEMENT_ID` | Always NULL | Constant NULL | Always. |

## MON$CONTEXT_VARIABLES

Table status: Implemented
Source: queryMonContextVariables

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `MON$ATTACHMENT_ID` | ScratchBird tracked | queryMonContextVariables | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$TRANSACTION_ID` | Always NULL | Constant NULL | Always. |
| `MON$VARIABLE_NAME` | Always NULL | Constant NULL | Always. |
| `MON$VARIABLE_VALUE` | Always NULL | Constant NULL | Always. |

## MON$DATABASE

Table status: Implemented
Source: queryMonDatabase

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `MON$DATABASE_NAME` | ScratchBird tracked | queryMonDatabase | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$PAGE_SIZE` | ScratchBird tracked | queryMonDatabase | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$ODS_MAJOR` | ScratchBird tracked | queryMonDatabase | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$ODS_MINOR` | ScratchBird tracked | queryMonDatabase | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$OLDEST_TRANSACTION` | ScratchBird tracked | queryMonDatabase | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$OLDEST_ACTIVE` | ScratchBird tracked | queryMonDatabase | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$OLDEST_SNAPSHOT` | ScratchBird tracked | queryMonDatabase | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$NEXT_TRANSACTION` | ScratchBird tracked | queryMonDatabase | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$PAGE_BUFFERS` | ScratchBird tracked | queryMonDatabase | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$SQL_DIALECT` | ScratchBird tracked | Constant 3 | Always. |
| `MON$SHUTDOWN_MODE` | Always 0 | Constant 0/false | Always. |
| `MON$SWEEP_INTERVAL` | ScratchBird tracked | Constant 20000 | Always. |
| `MON$READ_ONLY` | Always 0 | Constant 0/false | Always. |
| `MON$FORCED_WRITES` | ScratchBird tracked | Constant 1 | Always. |
| `MON$RESERVE_SPACE` | ScratchBird tracked | Constant 1 | Always. |
| `MON$CREATION_DATE` | Always NULL | Constant NULL | Always. |
| `MON$PAGES` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$STAT_ID` | ScratchBird tracked | Constant 1 | Always. |
| `MON$BACKUP_STATE` | Always 0 | Constant 0/false | Always. |
| `MON$CRYPT_PAGE` | Always 0 | Constant 0/false | Always. |
| `MON$OWNER` | ScratchBird tracked | Constant "SYSDBA" | Always. |
| `MON$SEC_DATABASE` | ScratchBird tracked | Constant "Default" | Always. |
| `MON$CRYPT_STATE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$GUID` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$FILE_ID` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$NEXT_ATTACHMENT` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$NEXT_STATEMENT` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$REPLICA_MODE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## MON$IO_STATS

Table status: Implemented
Source: queryMonIoStats

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `MON$STAT_ID` | ScratchBird tracked | queryMonIoStats | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$STAT_GROUP` | Always NULL | Constant NULL | Always. |
| `MON$PAGE_READS` | Always NULL | Constant NULL | Always. |
| `MON$PAGE_WRITES` | Always NULL | Constant NULL | Always. |
| `MON$PAGE_FETCHES` | Always NULL | Constant NULL | Always. |
| `MON$PAGE_MARKS` | Always NULL | Constant NULL | Always. |

## MON$MEMORY_USAGE

Table status: Implemented
Source: queryMonMemoryUsage

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `MON$STAT_ID` | ScratchBird tracked | queryMonMemoryUsage | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$STAT_GROUP` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$MEMORY_USED` | ScratchBird tracked | queryMonMemoryUsage | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$MEMORY_ALLOCATED` | ScratchBird tracked | queryMonMemoryUsage | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$MAX_MEMORY_USED` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `MON$MAX_MEMORY_ALLOCATED` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## MON$RECORD_STATS

Table status: Implemented
Source: queryMonRecordStats

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `MON$STAT_ID` | ScratchBird tracked | queryMonRecordStats | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$STAT_GROUP` | Always 0 | Constant 0/false | Always. |
| `MON$RECORD_SEQ_READS` | ScratchBird tracked | queryMonRecordStats | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$RECORD_IDX_READS` | ScratchBird tracked | queryMonRecordStats | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$RECORD_INSERTS` | ScratchBird tracked | queryMonRecordStats | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$RECORD_UPDATES` | ScratchBird tracked | queryMonRecordStats | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$RECORD_DELETES` | ScratchBird tracked | queryMonRecordStats | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$RECORD_BACKOUTS` | Always 0 | Constant 0/false | Always. |
| `MON$RECORD_PURGES` | Always 0 | Constant 0/false | Always. |
| `MON$RECORD_EXPUNGES` | Always 0 | Constant 0/false | Always. |
| `MON$RECORD_LOCKS` | Always 0 | Constant 0/false | Always. |
| `MON$RECORD_WAITS` | Always 0 | Constant 0/false | Always. |
| `MON$RECORD_CONFLICTS` | Always 0 | Constant 0/false | Always. |
| `MON$BACKVERSION_READS` | Always 0 | Constant 0/false | Always. |
| `MON$FRAGMENT_READS` | Always 0 | Constant 0/false | Always. |
| `MON$RECORD_RPT_READS` | Always 0 | Constant 0/false | Always. |
| `MON$RECORD_IMGC` | Always 0 | Constant 0/false | Always. |

## MON$STATEMENTS

Table status: Implemented
Source: queryMonStatements

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `MON$STATEMENT_ID` | ScratchBird tracked | queryMonStatements | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$ATTACHMENT_ID` | Always NULL | Constant NULL | Always. |
| `MON$TRANSACTION_ID` | Always NULL | Constant NULL | Always. |
| `MON$STATE` | ScratchBird tracked | queryMonStatements | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$TIMESTAMP` | Always NULL | Constant NULL | Always. |
| `MON$SQL_TEXT` | ScratchBird tracked | queryMonStatements | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$STAT_ID` | ScratchBird tracked | queryMonStatements | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$EXPLAINED_PLAN` | Always NULL | Constant NULL | Always. |
| `MON$STATEMENT_TIMEOUT` | Always 0 | Constant 0/false | Always. |
| `MON$STATEMENT_TIMER` | Always NULL | Constant NULL | Always. |
| `MON$COMPILED_STATEMENT_ID` | Always NULL | Constant NULL | Always. |

## MON$TABLE_STATS

Table status: Implemented
Source: queryMonTableStats

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `MON$STAT_ID` | ScratchBird tracked | queryMonTableStats | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$STAT_GROUP` | Always 0 | Constant 0/false | Always. |
| `MON$TABLE_NAME` | ScratchBird tracked | queryMonTableStats | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$RECORD_STAT_ID` | ScratchBird tracked | queryMonTableStats | Row emitted; value derived from ScratchBird catalog or constants. |

## MON$TRANSACTIONS

Table status: Implemented
Source: queryMonTransactions

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `MON$TRANSACTION_ID` | ScratchBird tracked | queryMonTransactions | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$ATTACHMENT_ID` | Always NULL | Constant NULL | Always. |
| `MON$STATE` | ScratchBird tracked | queryMonTransactions | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$TIMESTAMP` | Always NULL | Constant NULL | Always. |
| `MON$TOP_TRANSACTION` | ScratchBird tracked | queryMonTransactions | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$OLDEST_TRANSACTION` | ScratchBird tracked | queryMonTransactions | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$OLDEST_ACTIVE` | ScratchBird tracked | queryMonTransactions | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$ISOLATION_MODE` | ScratchBird tracked | queryMonTransactions | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$LOCK_TIMEOUT` | Always NULL | Constant NULL | Always. |
| `MON$READ_ONLY` | ScratchBird tracked | queryMonTransactions | Row emitted; value derived from ScratchBird catalog or constants. |
| `MON$AUTO_COMMIT` | Always 0 | Constant 0/false | Always. |
| `MON$AUTO_UNDO` | ScratchBird tracked | Constant 1 | Always. |
| `MON$STAT_ID` | ScratchBird tracked | queryMonTransactions | Row emitted; value derived from ScratchBird catalog or constants. |

## SEC$DB_CREATORS

Table status: Implemented
Source: querySecDbCreators

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `SEC$USER` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `SEC$USER_TYPE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## SEC$GLOBAL_AUTH_MAPPING

Table status: Implemented
Source: querySecGlobalAuthMapping

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `SEC$MAP_NAME` | ScratchBird tracked | querySecGlobalAuthMapping | Row emitted; value derived from ScratchBird catalog or constants. |
| `SEC$MAP_USING` | ScratchBird tracked | querySecGlobalAuthMapping | Row emitted; value derived from ScratchBird catalog or constants. |
| `SEC$MAP_PLUGIN` | ScratchBird tracked | querySecGlobalAuthMapping | Row emitted; value derived from ScratchBird catalog or constants. |
| `SEC$MAP_DB` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `SEC$MAP_FROM_TYPE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `SEC$MAP_FROM` | ScratchBird tracked | querySecGlobalAuthMapping | Row emitted; value derived from ScratchBird catalog or constants. |
| `SEC$MAP_TO_TYPE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `SEC$MAP_TO` | ScratchBird tracked | querySecGlobalAuthMapping | Row emitted; value derived from ScratchBird catalog or constants. |
| `SEC$DESCRIPTION` | Always NULL | Column not present in ScratchBird catalog handler | Never. |

## SEC$USERS

Table status: Implemented
Source: querySecUsers

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `SEC$USER_NAME` | ScratchBird tracked | querySecUsers | Row emitted; value derived from ScratchBird catalog or constants. |
| `SEC$FIRST_NAME` | ScratchBird tracked | Constant "System" | Always. |
| `SEC$MIDDLE_NAME` | Always NULL | Constant NULL | Always. |
| `SEC$LAST_NAME` | ScratchBird tracked | Constant "Administrator" | Always. |
| `SEC$ACTIVE` | ScratchBird tracked | Constant true | Always. |
| `SEC$ADMIN` | ScratchBird tracked | Constant true | Always. |
| `SEC$DESCRIPTION` | ScratchBird tracked | Constant "Database administrator" | Always. |
| `SEC$PLUGIN` | ScratchBird tracked | Constant "Srp256" | Always. |

## SEC$USER_ATTRIBUTES

Table status: Implemented
Source: querySecUserAttributes

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `SEC$USER_NAME` | ScratchBird tracked | querySecUserAttributes | Row emitted; value derived from ScratchBird catalog or constants. |
| `SEC$KEY` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `SEC$VALUE` | Always NULL | Column not present in ScratchBird catalog handler | Never. |
| `SEC$PLUGIN` | ScratchBird tracked | querySecUserAttributes | Row emitted; value derived from ScratchBird catalog or constants. |

