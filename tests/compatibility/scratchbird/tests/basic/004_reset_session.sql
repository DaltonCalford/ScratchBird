-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Basic - RESET Session Variables
-- Description: RESET for parser/search_path and RESET ALL
-- ============================================================================

CREATE DATABASE test_reset_session_db;
USE test_reset_session_db;

SET PARSER TO 'FIREBIRD';
SET search_path TO 'public';

RESET PARSER;
RESET search_path;
RESET ALL;

DROP DATABASE test_reset_session_db;
