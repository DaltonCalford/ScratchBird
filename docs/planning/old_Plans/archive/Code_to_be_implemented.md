ScratchBird Source Code Incompleteness Report

  Scan Date: November 28, 2025
  Files Scanned: 310 C++ files (src/ and include/)
  Total Indicators Found: 249+

  ---
  Executive Summary

  | Category                  | Count |
  |---------------------------|-------|
  | TODO                      | 144+  |
  | Future/Later references   | 50+   |
  | INCOMPLETE error messages | 35+   |
  | NOT IMPLEMENTED           | 5     |
  | FIXME                     | 2     |
  | WORKAROUND                | 2     |
  | HACK                      | 1     |

  ---
  Top 10 Files with Most TODOs

  | Rank | File                                 | TODOs |
  |------|--------------------------------------|-------|
  | 1    | src/sblr/executor.cpp                | 24    |
  | 2    | src/core/storage_engine.cpp          | 8     |
  | 3    | src/optimizer/query_planner.cpp      | 8     |
  | 4    | src/core/lsm_compaction_manager.cpp  | 7     |
  | 5    | src/core/charset_loader.cpp          | 7     |
  | 6    | src/parser/semantic_analyzer.cpp     | 6     |
  | 7    | src/optimizer/statistics_manager.cpp | 5     |
  | 8    | src/sblr/bytecode_generator.cpp      | 5     |
  | 9    | src/core/btree.cpp                   | 5     |
  | 10   | src/core/timezone_loader.cpp         | 4     |

  ---
  Detailed Findings by Area

  1. EXECUTOR (24 TODOs) - src/sblr/executor.cpp

  | Line  | Issue                                                                     |
  |-------|---------------------------------------------------------------------------|
  | 1713  | Use constraint name instead of auto-generated                             |
  | 4642  | Deserialize generation_expression and evaluate against current row values |
  | 6938  | Handle DISTINCT for 2-variable functions                                  |
  | 8742  | Parse and store argument expressions                                      |
  | 8761  | Full expression support                                                   |
  | 8969  | Window function argument parsing not available                            |
  | 13221 | Handle nested multi-geometry types                                        |
  | 19592 | Get default_schema_id from database                                       |
  | 19736 | Get from connection context when implemented                              |
  | 19896 | Schema-qualified name handling needed                                     |
  | 19902 | Get actual current schema                                                 |
  | 19928 | Implement lookup for other object types                                   |
  | 20052 | Schema-qualified name handling                                            |
  | 20057 | Get actual current schema                                                 |
  | 20083 | Implement lookup for other object types                                   |
  | 20155 | CASCADE option not implemented in catalog                                 |
  | 20214 | Get from connection context                                               |
  | 20218 | WITH ADMIN OPTION not in bytecode                                         |
  | 20273 | CASCADE option not implemented                                            |
  | 20367 | Implement session user tracking                                           |
  | 20792 | Full SQL LIKE pattern matching                                            |
  | 22064 | Proper TOAST loading                                                      |
  | 22695 | Evaluate default_expr bytecode                                            |
  | 23069 | Evaluate default_expr bytecode                                            |

  2. STATISTICAL AGGREGATE FUNCTIONS (7 TODOs) - src/sblr/executor.cpp:23509-23544

  | Function    | Status                                    |
  |-------------|-------------------------------------------|
  | STDDEV_POP  | TODO - Implement with Welford's algorithm |
  | STDDEV_SAMP | TODO - Implement as aggregate             |
  | VAR_POP     | TODO - Implement with Welford's algorithm |
  | VAR_SAMP    | TODO - Implement as aggregate             |
  | CORR        | TODO - Implement as aggregate             |
  | COVAR_POP   | TODO - Implement as aggregate             |
  | COVAR_SAMP  | TODO - Implement as aggregate             |

  3. QUERY OPTIMIZER (8 TODOs) - src/optimizer/query_planner.cpp

  | Line | Issue                                                 |
  |------|-------------------------------------------------------|
  | 672  | Analyze WHERE clause to check if index column is used |
  | 673  | Check operator compatibility with index type          |
  | 693  | Pass string pool to properly resolve function names   |
  | 897  | Set filter expression from WHERE clause               |
  | 922  | Set index condition and filter from WHERE clause      |
  | 945  | Set index condition and filter from WHERE clause      |
  | 968  | Set index conditions and filter from WHERE clause     |
  | 996  | Set spatial condition and filter from WHERE clause    |
  | 1344 | Get num_pages from catalog                            |
  | 1345 | Get num_tuples from catalog                           |
  | 1373 | Also generate index scan paths                        |

  4. LSM COMPACTION (7 TODOs) - src/core/lsm_compaction_manager.cpp

  | Line | Issue                                            |
  |------|--------------------------------------------------|
  | 178  | Implement key range overlap detection            |
  | 183  | Get actual OIT from TransactionManager           |
  | 202  | Implement key range overlap detection            |
  | 260  | Use proper path generation based on target level |
  | 376  | Implement proper key range overlap detection     |
  | 445  | Use configurable block size                      |
  | 590  | Add proper logging                               |

  5. CHARACTER SET & COLLATION (9 TODOs)

  src/core/charset.cpp:
  | Line | Issue                                             |
  |------|---------------------------------------------------|
  | 472  | Implement full UCA and locale-specific comparison |
  | 769  | Implement proper Unicode case folding             |

  src/core/charset_loader.cpp:
  | Line | Issue                                             |
  |------|---------------------------------------------------|
  | 13   | Implement catalog insertion for pg_charsets       |
  | 49   | Call catalog manager to insert into pg_charsets   |
  | 63   | Implement catalog insertion for pg_collations     |
  | 108  | Call catalog manager to insert into pg_collations |
  | 244  | Query catalog to check if charset exists          |
  | 251  | Query catalog to check if collation exists        |
  | 258  | Query catalog to get charset ID by name           |

  6. PARSER & SEMANTIC ANALYZER (11 TODOs)

  src/parser/parser.cpp:
  | Line | Issue                          |
  |------|--------------------------------|
  | 1893 | Add IN/OUT/INOUT token support |
  | 2191 | Add NOTICE, WARNING tokens     |

  src/parser/semantic_analyzer.cpp:
  | Line | Issue                                                     |
  |------|-----------------------------------------------------------|
  | 168  | Add validation when semantic analyzer is fully integrated |
  | 179  | Add validation when tablespace catalog is integrated      |
  | 260  | Add validation when tablespace catalog is integrated      |
  | 270  | Add validation when tablespace catalog is integrated      |
  | 280  | Add validation when migration logic is implemented        |
  | 392  | Extract actual column info from CTE select list           |
  | 584  | Implement full semantic analysis for MERGE statement      |
  | 722  | Validate qualifier matches available table/alias          |
  | 2129 | Add semantic validation (Phase 3.4.3)                     |
  | 2137 | Add semantic validation (Phase 3.4.3)                     |

  7. STORAGE ENGINE (8 TODOs) - src/core/storage_engine.cpp

  | Line | Issue                                             |
  |------|---------------------------------------------------|
  | 429  | Columnstore row-level integration not implemented |
  | 660  | Get from ConnectionContext::getWaitForLocks()     |
  | 1321 | Get from ConnectionContext::getWaitForLocks()     |
  | 1515 | Columnstore row-level integration not implemented |
  | 1940 | Implement proper tuple deserialization            |
  | 1968 | Implement proper column value extraction          |

  8. B-TREE (5 TODOs) - src/core/btree.cpp

  | Line | Issue                                 |
  |------|---------------------------------------|
  | 314  | TOAST detoasting for index keys       |
  | 590  | Get proc_id from thread-local storage |
  | 1286 | Get proc_id from thread-local storage |
  | 1526 | Get proc_id from thread-local storage |
  | 2294 | Consider implementing parent merge    |

  9. SECURITY (3 TODOs) - src/security/view_security.cpp

  | Line | Issue                                         |
  |------|-----------------------------------------------|
  | 186  | Check actual permissions for effective_user   |
  | 207  | Check column-level permissions                |
  | 240  | Evaluate view's WHERE clause against row_data |

  10. CRYPTOGRAPHIC FUNCTIONS (2 TODOs) - src/sblr/executor.cpp

  | Line  | Issue                                                |
  |-------|------------------------------------------------------|
  | 24460 | Implement ENCODE(data, format) - base64, hex, escape |
  | 24466 | Implement DECODE(text, format)                       |

  ---
  Priority Classification

  HIGH PRIORITY (Blocks Basic Functionality)

  1. LSM Compaction Key Range Detection - Required for proper compaction
  2. Query Planner WHERE Clause Analysis - Core query optimization
  3. TOAST Loading Infrastructure - Required for large values
  4. Semantic Analyzer Integration - Schema validation

  MEDIUM PRIORITY (Blocks Advanced Features)

  1. Statistical Aggregate Functions (STDDEV, VAR, CORR, COVAR)
  2. Character Set/Collation System - I18n support
  3. Window Function Argument Parsing - Analytics queries
  4. Full LIKE Pattern Matching - String operations

  LOW PRIORITY (Enhancement/Polish)

  1. Index Optimization (GiST integration, path generation)
  2. Timezone DST Calculations
  3. Cryptographic Functions (ENCODE/DECODE)
  4. Specialized Optimizations

  ---
  FIXME Items (2 total)

  | File                             | Line | Issue                                     |
  |----------------------------------|------|-------------------------------------------|
  | src/parser/semantic_analyzer.cpp | 1157 | Could be more precise based on field type |

  ---
  WORKAROUND Items (2 total)

  | File                      | Line | Issue                                       |
  |---------------------------|------|---------------------------------------------|
  | src/core/spgist_index.cpp | 432  | Return error (could split parent in future) |
  | src/core/spgist_index.cpp | 478  | Try to just add a new partition             |

  ---
  NOT IMPLEMENTED Items (5 total)

  | File                     | Lines   | Issue                                                           |
  |--------------------------|---------|-----------------------------------------------------------------|
  | src/sblr/index_cache.cpp | 287-289 | GiST index incomplete type issues - memory leak to avoid errors |

  ---
  Recommendations

  1. Create tracking issues for the 24 executor TODOs
  2. Implement statistical aggregates - commonly needed functions
  3. Complete query planner WHERE clause analysis - core functionality
  4. Address LSM compaction key range detection - data integrity
  5. Prioritize TOAST loading - blocks large value support
