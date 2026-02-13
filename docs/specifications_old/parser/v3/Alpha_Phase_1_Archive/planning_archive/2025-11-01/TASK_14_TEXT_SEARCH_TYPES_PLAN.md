# Task 14: Text Search Types Implementation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Task**: Phase 3, Task 14 - PostgreSQL Full-Text Search (FTS) Types
**Estimated Effort**: 130-200 hours
**Priority**: Phase 3 (Nice to have for complete feature parity)
**Status**: NOT STARTED
**Date**: October 30, 2025

---

## Overview

Implement PostgreSQL-compatible full-text search types (`tsvector`, `tsquery`) with specialized indexing and ranking capabilities. This is **distinct** from the regex-based text search functions completed in Phase 2 Wave 1.

### What Was Already Completed (Phase 2 Wave 1)

✅ **Regex Text Search** (Task 13 - Complete):
- LIKE, ILIKE operators
- REGEXP_MATCHES, REGEXP_REPLACE, REGEXP_SPLIT functions
- 16 text manipulation functions
- Regex operators (~, ~*, !~, !~*)

### What Task 14 Adds

🆕 **Full-Text Search (FTS) Types**:
- `tsvector` - Preprocessed document representation (lexemes + positions)
- `tsquery` - Structured search query representation
- `@@` match operator - Efficient indexed matching
- Text search configurations (language-specific stemming, stop words)
- GIN index integration for fast lookups
- Ranking functions (ts_rank, ts_rank_cd)

### Key Difference

| Feature | Regex (Task 13) | FTS Types (Task 14) |
|---------|----------------|---------------------|
| **Use Case** | Pattern matching | Document search |
| **Index Support** | No specialized index | GIN index optimized |
| **Preprocessing** | None | Stemming, stop words |
| **Performance** | O(n) scan | O(log n) with GIN |
| **Query Language** | Regex patterns | Boolean logic (AND, OR, NOT) |
| **Ranking** | No | Yes (relevance scores) |

---

## PostgreSQL FTS Architecture

### Core Types

#### 1. tsvector - Document Representation

```sql
-- Format: 'lexeme1' 'lexeme2':positions 'lexeme3':positions,...
SELECT 'a fat cat sat on a mat and ate a fat rat'::tsvector;
-- Result: 'a' 'and' 'ate' 'cat' 'fat' 'mat' 'on' 'rat' 'sat'

-- With positions:
SELECT to_tsvector('english', 'a fat cat sat on a mat and ate a fat rat');
-- Result: 'ate':9 'cat':3 'fat':2,11 'mat':7 'rat':12 'sat':4
-- (stop words 'a', 'on', 'and' removed, positions tracked)
```

**Internal Structure**:
- Sorted array of lexemes (normalized words)
- Each lexeme has positions where it appears
- Positions optionally include weight labels (A, B, C, D)
- Compact binary representation

#### 2. tsquery - Search Query Representation

```sql
-- Format: lexeme & lexeme | lexeme & !lexeme
SELECT 'fat & rat'::tsquery;
-- Result: 'fat' & 'rat'

SELECT to_tsquery('english', 'fat & (cat | rat)');
-- Result: 'fat' & ( 'cat' | 'rat' )
```

**Internal Structure**:
- Expression tree with AND/OR/NOT operators
- Proximity operators (<->, <N>)
- Phrase search operators
- Prefix matching (lexeme:*)

#### 3. @@ Match Operator

```sql
-- Check if tsvector matches tsquery
SELECT to_tsvector('fat cats') @@ to_tsquery('cat');
-- Result: true (stemming: cats → cat)

-- Can work on plain text (implicit to_tsvector):
SELECT 'fat cats' @@ to_tsquery('cat');
-- Result: true
```

### Text Search Configuration

Languages have different stemming rules and stop words:

```sql
-- English configuration (default)
SELECT to_tsvector('english', 'running cats');
-- Result: 'cat':2 'run':1  (running→run, cats→cat)

-- Simple configuration (no stemming)
SELECT to_tsvector('simple', 'running cats');
-- Result: 'cats':2 'running':1  (no normalization)
```

---

## Implementation Architecture

### Phase 1: Core Type System (40-50 hours)

#### 1.1 tsvector Type (20-25 hours)

**File**: `include/scratchbird/core/tsvector.h`, `src/core/tsvector.cpp`

```cpp
namespace scratchbird::core
{
    // Lexeme with positions
    struct Lexeme
    {
        std::string word;                    // Normalized word
        std::vector<uint16_t> positions;     // Positions (1-based)
        std::vector<char> weights;           // Optional weights (A/B/C/D)
    };

    class TSVector
    {
    public:
        TSVector() = default;

        // Parsing
        static TSVector fromString(const std::string& str);
        std::string toString() const;

        // Construction
        void addLexeme(const std::string& word,
                      const std::vector<uint16_t>& positions = {},
                      const std::vector<char>& weights = {});

        // Access
        size_t size() const { return lexemes_.size(); }
        const std::vector<Lexeme>& lexemes() const { return lexemes_; }

        // Operations
        TSVector concat(const TSVector& other) const;
        bool contains(const std::string& word) const;

        // Serialization
        std::vector<uint8_t> serialize() const;
        static TSVector deserialize(const std::vector<uint8_t>& data);

    private:
        std::vector<Lexeme> lexemes_;  // Sorted by word

        void normalize();  // Sort and deduplicate
    };
}
```

**Estimated Lines**: ~400-500 lines

#### 1.2 tsquery Type (20-25 hours)

**File**: `include/scratchbird/core/tsquery.h`, `src/core/tsquery.cpp`

```cpp
namespace scratchbird::core
{
    enum class TSQueryOp
    {
        AND,      // &
        OR,       // |
        NOT,      // !
        PHRASE,   // <->
        DISTANCE  // <N>
    };

    struct TSQueryNode
    {
        enum class Type { LEXEME, OPERATOR };

        Type type;
        std::string lexeme;        // For LEXEME nodes
        TSQueryOp op;             // For OPERATOR nodes
        std::unique_ptr<TSQueryNode> left;
        std::unique_ptr<TSQueryNode> right;
        uint16_t distance = 1;    // For PHRASE/DISTANCE ops
    };

    class TSQuery
    {
    public:
        TSQuery() = default;

        // Parsing
        static TSQuery fromString(const std::string& str);
        std::string toString() const;

        // Evaluation
        bool matches(const TSVector& vec) const;

        // Serialization
        std::vector<uint8_t> serialize() const;
        static TSQuery deserialize(const std::vector<uint8_t>& data);

    private:
        std::unique_ptr<TSQueryNode> root_;

        bool evaluateNode(const TSQueryNode* node, const TSVector& vec) const;
    };
}
```

**Estimated Lines**: ~500-600 lines

---

### Phase 2: Text Processing (30-40 hours)

#### 2.1 Text Search Configuration (15-20 hours)

**File**: `include/scratchbird/core/ts_config.h`, `src/core/ts_config.cpp`

```cpp
namespace scratchbird::core
{
    class TSConfig
    {
    public:
        TSConfig(const std::string& name);

        // Stemming
        std::string stem(const std::string& word) const;

        // Stop words
        bool isStopWord(const std::string& word) const;

        // Tokenization
        std::vector<std::string> tokenize(const std::string& text) const;

        // Configuration management
        static TSConfig* get(const std::string& name);
        static void registerConfig(const std::string& name, TSConfig* config);

    private:
        std::string name_;
        std::unordered_set<std::string> stop_words_;

        // Stemming implementation (Porter stemmer)
        std::string porterStem(const std::string& word) const;
    };

    // Predefined configurations
    class EnglishConfig : public TSConfig { /* ... */ };
    class SimpleConfig : public TSConfig { /* ... */ };
}
```

**Dependencies**: Porter stemmer algorithm
**Estimated Lines**: ~600-800 lines (including Porter stemmer)

#### 2.2 to_tsvector Function (8-10 hours)

**File**: Part of `src/sblr/executor.cpp`

```cpp
// Opcode: EXT_TO_TSVECTOR
// Arguments: config_name (optional), text
// Returns: tsvector

TSVector to_tsvector(const std::string& config_name, const std::string& text)
{
    TSConfig* config = TSConfig::get(config_name);
    TSVector result;

    std::vector<std::string> tokens = config->tokenize(text);
    uint16_t position = 1;

    for (const auto& token : tokens)
    {
        if (config->isStopWord(token))
            continue;

        std::string lexeme = config->stem(token);
        result.addLexeme(lexeme, {position});
        position++;
    }

    return result;
}
```

**Estimated Lines**: ~150-200 lines (executor handler)

#### 2.3 to_tsquery Function (7-10 hours)

**File**: Part of `src/sblr/executor.cpp`

```cpp
// Opcode: EXT_TO_TSQUERY
// Arguments: config_name (optional), query_text
// Returns: tsquery

TSQuery to_tsquery(const std::string& config_name, const std::string& query_text)
{
    TSConfig* config = TSConfig::get(config_name);

    // Parse query string: "fat & (cat | rat)"
    // Apply stemming to lexemes
    // Build expression tree

    return TSQuery::fromString(stemmed_query);
}
```

**Estimated Lines**: ~200-250 lines (parser + executor)

---

### Phase 3: Operators and Functions (20-30 hours)

#### 3.1 @@ Match Operator (10-12 hours)

**File**: `src/sblr/executor.cpp`

```cpp
// Opcode: EXT_TSMATCH
// Arguments: tsvector, tsquery
// Returns: boolean

bool ts_match(const TSVector& vec, const TSQuery& query)
{
    return query.matches(vec);
}

// Also support: text @@ tsquery (implicit to_tsvector)
bool ts_match_text(const std::string& text, const TSQuery& query)
{
    TSVector vec = to_tsvector("english", text);
    return query.matches(vec);
}
```

**Estimated Lines**: ~100-150 lines

#### 3.2 ts_rank Function (10-15 hours)

**File**: `src/sblr/executor.cpp`

```cpp
// Opcode: EXT_TS_RANK
// Arguments: tsvector, tsquery
// Returns: float (relevance score)

float ts_rank(const TSVector& vec, const TSQuery& query)
{
    // TF-IDF style ranking
    // - Count matching lexemes
    // - Weight by position proximity
    // - Normalize by document length

    float score = 0.0f;
    // Implementation...
    return score;
}
```

**Estimated Lines**: ~200-250 lines (ranking algorithm)

---

### Phase 4: GIN Index Integration (25-35 hours)

#### 4.1 GIN Index Operator Class (15-20 hours)

**File**: `src/core/gin_index.cpp` (enhancement)

```cpp
// Add tsvector support to existing GIN index

class GINTSVectorOps : public GINOperatorClass
{
public:
    // Extract keys from tsvector (one key per lexeme)
    std::vector<Value> extractKeys(const Value& tsvector) override
    {
        TSVector vec = tsvector.getTSVector();
        std::vector<Value> keys;

        for (const auto& lexeme : vec.lexemes())
        {
            keys.push_back(Value::makeText(lexeme.word));
        }

        return keys;
    }

    // Check if query matches posting list
    bool consistent(const std::vector<bool>& check, const TSQuery& query) override
    {
        // Evaluate query boolean logic with available keys
        // ...
    }
};
```

**Estimated Lines**: ~300-400 lines

#### 4.2 Query Planning Integration (10-15 hours)

**File**: `src/optimizer/query_planner.cpp` (enhancement)

```cpp
// Generate GIN index scan for @@ operator

if (op == BinaryOp::TSMATCH)
{
    // Check if left operand has GIN index
    // Generate GIN scan path
    // Cost estimation based on query selectivity
}
```

**Estimated Lines**: ~200-250 lines

---

### Phase 5: SQL Integration (15-25 hours)

#### 5.1 Parser Support (8-12 hours)

**File**: `src/parser/parser.cpp`, `src/parser/lexer.cpp`

```cpp
// Add tokens
TokenType::KW_TSVECTOR
TokenType::KW_TSQUERY
TokenType::OP_TSMATCH  // @@

// Parse type specifications
// COLUMN_NAME tsvector
// COLUMN_NAME tsquery

// Parse @@ operator
// tsvector_col @@ 'search query'::tsquery
```

**Estimated Lines**: ~150-200 lines

#### 5.2 Bytecode Generation (4-6 hours)

**File**: `src/sblr/bytecode_generator.cpp`

```cpp
// Generate opcodes for FTS operations
// EXT_TO_TSVECTOR
// EXT_TO_TSQUERY
// EXT_TSMATCH
// EXT_TS_RANK
```

**Estimated Lines**: ~100-150 lines

#### 5.3 Type System Integration (3-7 hours)

**File**: `include/scratchbird/core/types.h`, `src/core/types.cpp`

```cpp
enum class DataType
{
    // ...existing types...
    TSVECTOR = 40,
    TSQUERY = 41
};

class Value
{
public:
    static Value makeTSVector(const TSVector& vec);
    static Value makeTSQuery(const TSQuery& query);

    TSVector getTSVector() const;
    TSQuery getTSQuery() const;
};
```

**Estimated Lines**: ~150-200 lines

---

### Phase 6: Testing (15-20 hours)

#### 6.1 Unit Tests

**File**: `tests/unit/test_tsvector.cpp` (~300 lines)
- tsvector parsing
- Lexeme operations
- Serialization

**File**: `tests/unit/test_tsquery.cpp` (~300 lines)
- tsquery parsing
- Boolean evaluation
- Query optimization

**File**: `tests/unit/test_ts_config.cpp` (~250 lines)
- Stemming correctness
- Stop word filtering
- Multi-language support

#### 6.2 Integration Tests

**File**: `tests/integration/test_full_text_search.cpp` (~400 lines)
- to_tsvector/to_tsquery functions
- @@ operator
- ts_rank function
- GIN index usage
- Multi-document search

**Total Test Lines**: ~1,250 lines

---

## Implementation Summary

| Component | Estimated Hours | Estimated Lines |
|-----------|----------------|-----------------|
| **tsvector type** | 20-25h | 400-500 |
| **tsquery type** | 20-25h | 500-600 |
| **Text configuration** | 15-20h | 600-800 |
| **to_tsvector function** | 8-10h | 150-200 |
| **to_tsquery function** | 7-10h | 200-250 |
| **@@ operator** | 10-12h | 100-150 |
| **ts_rank function** | 10-15h | 200-250 |
| **GIN integration** | 25-35h | 500-650 |
| **SQL integration** | 15-25h | 400-550 |
| **Testing** | 15-20h | 1,250 |
| **Total** | **145-197h** | **~4,300-4,950 lines** |

---

## Dependencies

### External Libraries (Optional)

1. **Snowball Stemmer** (libstemmer) - Better than Porter stemmer
   - Multi-language support
   - Battle-tested algorithms
   - ~15 languages out of the box

2. **ICU** (International Components for Unicode) - Already used for collation
   - Word segmentation (for CJK languages)
   - Case folding
   - Normalization

### Internal Dependencies

1. **GIN Index** - ✅ Already implemented (Phase 2, Wave 1)
2. **Text Functions** - ✅ Already implemented (Task 13)
3. **Value System** - ✅ Extensible for new types

---

## Usage Examples

### Basic Full-Text Search

```sql
-- Create table with tsvector column
CREATE TABLE documents (
    id INTEGER PRIMARY KEY,
    title TEXT,
    body TEXT,
    search_vector tsvector
);

-- Create GIN index for fast searching
CREATE INDEX idx_search ON documents USING GIN(search_vector);

-- Insert documents with preprocessed tsvector
INSERT INTO documents (id, title, body, search_vector)
VALUES (
    1,
    'PostgreSQL Tutorial',
    'Learn PostgreSQL full-text search with examples',
    to_tsvector('english', 'PostgreSQL Tutorial Learn PostgreSQL full-text search with examples')
);

-- Search with ranking
SELECT
    id,
    title,
    ts_rank(search_vector, query) AS rank
FROM
    documents,
    to_tsquery('english', 'postgresql & search') AS query
WHERE
    search_vector @@ query
ORDER BY
    rank DESC;
```

### Advanced Features

```sql
-- Phrase search (words must be adjacent)
SELECT * FROM documents
WHERE search_vector @@ 'full <-> text'::tsquery;

-- Proximity search (words within N positions)
SELECT * FROM documents
WHERE search_vector @@ 'full <2> search'::tsquery;

-- Prefix matching
SELECT * FROM documents
WHERE search_vector @@ 'post:*'::tsquery;  -- Matches postgres, postgresql, etc.

-- Weighted search (prefer title matches)
UPDATE documents
SET search_vector =
    setweight(to_tsvector('english', title), 'A') ||
    setweight(to_tsvector('english', body), 'B');
```

---

## PostgreSQL Compatibility

### Supported Features

- ✅ tsvector and tsquery types
- ✅ to_tsvector() function
- ✅ to_tsquery() function
- ✅ @@ match operator
- ✅ ts_rank() function
- ✅ Text search configurations
- ✅ GIN index support
- ✅ Boolean operators (AND, OR, NOT)
- ✅ Phrase search (<->)
- ✅ Prefix matching (:*)

### Deferred to Future Phases

- ⏳ ts_rank_cd() (cover density ranking)
- ⏳ tsquery rewriting
- ⏳ Thesaurus support
- ⏳ Custom dictionaries
- ⏳ plainto_tsquery() (simpler query syntax)
- ⏳ phraseto_tsquery() (phrase query helper)
- ⏳ websearch_to_tsquery() (web search syntax)
- ⏳ Highlighting (ts_headline)

---

## Performance Considerations

### GIN Index Performance

- **Index Size**: ~2-3x document size (depends on vocabulary)
- **Search Speed**: O(log n) for AND queries, O(k*log n) for OR queries
- **Insert Speed**: O(m*log n) where m = unique lexemes per document

### Memory Usage

- **tsvector**: ~10-20 bytes per unique lexeme + positions
- **tsquery**: ~50-100 bytes per query tree
- **Configuration**: ~1-5 MB per language (stop words + stemming tables)

### Optimization Strategies

1. **Partial Indexes**: Index only recent/relevant documents
2. **GIN Fast Update**: Batch updates to reduce index churn
3. **Lazy Lexeme Expansion**: Don't expand query until needed
4. **Cache Stemmed Results**: Avoid re-stemming common words

---

## Risks and Mitigations

### Risk 1: Porter Stemmer Complexity

**Risk**: Implementing Porter stemmer from scratch is error-prone
**Mitigation**: Use Snowball libstemmer (well-tested, multi-language)
**Fallback**: Simple suffix stripping for MVP

### Risk 2: GIN Index Changes

**Risk**: Modifying GIN index might break existing functionality
**Mitigation**: Add operator class abstraction, don't modify core GIN
**Testing**: Extensive GIN tests for arrays and tsvector

### Risk 3: Performance at Scale

**Risk**: FTS might be slow on large document sets
**Mitigation**: Benchmark early, optimize GIN lookups, add caching
**Acceptance Criteria**: Match PostgreSQL performance within 2x

---

## Acceptance Criteria

1. ✅ tsvector and tsquery types work correctly
2. ✅ to_tsvector() correctly stems and filters stop words
3. ✅ to_tsquery() parses complex Boolean queries
4. ✅ @@ operator returns correct match results
5. ✅ ts_rank() returns reasonable relevance scores
6. ✅ GIN index speeds up searches (>10x vs sequential scan)
7. ✅ English configuration works correctly
8. ✅ At least 1,000 unit/integration tests pass
9. ✅ Performance within 2x of PostgreSQL

---

## Recommendation

**Defer to Phase 3** - This is a "nice to have" feature:
- Phase 2 already has regex text search (Task 13)
- Full-text search is specialized (not required for most apps)
- 145-197 hours is substantial effort
- Can be added later without breaking changes

**If Implementing**: Start with MVP:
1. Basic tsvector/tsquery types (no positions)
2. Simple stemming (suffix stripping)
3. @@ operator (no GIN optimization initially)
4. Add GIN and ranking later

**MVP Effort**: ~60-80 hours (vs 145-197 full implementation)
