# Specification: Full-Text Index

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage/indexes |
| **Spec Version** | 1.0.0 |
| **Status** | 🟡 Review |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Development Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/fulltext_index.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/fulltext_index.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/tsvector.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/tsquery.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_fulltext_index.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_tsvector_ops.cpp:1`

## Synopsis

Full-text index enables efficient text search using inverted index structures. It supports boolean queries, phrase search, relevance ranking, and stemming in multiple languages. Built on GIN infrastructure with tsvector/tsquery types.

## Scope

### In Scope

- Inverted index structure (GIN-based)
- Text parsing and tokenization
- Stemming and normalization
- Query processing (tsquery)
- Relevance ranking (ts_rank)
- Multiple language dictionaries

### Out of Scope

- N-gram indexing (see index_ngram.md)
- Fuzzy string matching
- Regular expression search
- Document-level security filtering

## Background

Full-text search converts text documents into searchable vectors (tsvectors) containing:
- Lexemes (normalized word forms)
- Positional information
- Weight markers

Queries are parsed into tsquery structures supporting:
- Boolean operators (&, |, !)
- Phrase search (<->)
- Prefix matching (*)
- Weight filtering

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:661
enum class IndexType : uint8_t {
    FULLTEXT = 3,     // Full-text search index (GIN-based)
    // ... other types
};
```

### Text Search Types

```cpp
// Source: scratchbird/core/tsvector.h
struct TsVectorEntry {
    std::string lexeme;           // Normalized word form
    std::vector<uint16_t> positions;  // Word positions in document
    uint8_t weights;              // Weight flags (A=1, B=2, C=4, D=8)
};

struct TsVector {
    std::vector<TsVectorEntry> entries;
    
    // Serialized format:
    // [uint32_t count]
    // For each entry:
    //   [uint8_t lexeme_len] [char lexeme[lexeme_len]]
    //   [uint8_t pos_count] [uint16_t positions[pos_count]]
    //   [uint8_t weights]
};

// Source: scratchbird/core/tsquery.h
enum class TsQueryOperator : uint8_t {
    VALUE = 0,        // Lexeme value
    AND = 1,          // &
    OR = 2,           // |
    NOT = 3,          // !
    PHRASE = 4,       // <->
    FOLLOWED_BY = 5,  // <N>
};

struct TsQueryNode {
    TsQueryOperator op;
    std::string lexeme;           // For VALUE nodes
    uint32_t distance;            // For FOLLOWED_BY (N in <N>)
    std::unique_ptr<TsQueryNode> left;
    std::unique_ptr<TsQueryNode> right;
};
```

### GIN-Based Full-Text Structure

Full-text uses GIN (Generalized Inverted Index) with specialized opclass:

```
GIN Index Structure:
┌─────────────────────────────────────────┐
│ Meta Page                               │
│ (root_entry_page_id, pending pointers)  │
└─────────────────────────────────────────┘
                    │
        ┌───────────┴───────────┐
        ▼                       ▼
┌───────────────┐      ┌─────────────────┐
│ Entry Tree    │      │ Pending List    │
│ (B-tree of    │      │ (unsorted       │
│  lexemes)     │      │  lexeme/TID)    │
└───────┬───────┘      └─────────────────┘
        │
        ▼
┌───────────────┐
│ Posting List  │ (compressed positions + TIDs)
└───────────────┘
```

### Posting List Entry Format

```cpp
// Source: scratchbird/core/gin_tsvector_ops.h
struct FullTextPosting {
    TID tid;                      // Document TID (16 bytes)
    uint16_t weight_vector;       // Weight flags per position
    uint16_t position_count;      // Number of positions
    // Followed by:
    // uint16_t positions[position_count];  // Word positions
};
```

**Position Encoding:**
- Positions are delta-encoded for compression
- Maximum position: 16383 (14 bits)
- 2 bits reserved for weight (A, B, C, D)

### Text Parsing Pipeline

```
Input: Raw text document
  │
  ▼
┌─────────────────┐
│ Parser          │ - Split into tokens (words, numbers)
│ (text search    │ - Handle punctuation
│  configuration) │ - Identify email, URL patterns
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Dictionary      │ - Map tokens to lexemes
│ (ISpell/Snowball│ - Apply stemming
│  + thesaurus)   │ - Stop word removal
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ TsVector        │ - Collect lexemes with positions
│ Builder         │ - Assign weights (optional)
│                 │ - Deduplicate
└─────────────────┘
         │
         ▼
Output: tsvector for indexing
```

## Algorithms

### Algorithm: Document Indexing

```
Input:  document_text, TID, language_config
Output: Status

1. Parse document into tokens:
   tokens = parser.parse(document_text, language_config.parser)

2. For each token:
   a. Normalize via dictionary:
      lexeme = dictionary.normalize(token, language_config.dict)
   
   b. If lexeme is stop word: continue
   
   c. Record position and weight:
      - Position = token index in document
      - Weight = assigned weight (A=title, B=body, etc.)

3. Build tsvector from lexeme-position mappings

4. Insert into GIN index:
   For each (lexeme, positions, weights) in tsvector:
   a. Create posting entry:
      - tid = TID
      - weight_vector = encode_weights(positions, weights)
      - position_count = positions.size()
      - positions = delta_encode(positions)
   
   b. Add to pending list or directly to entry tree

5. Return OK
```

### Algorithm: Query Processing

```
Input:  query_text, language_config
Output: TsQuery tree

1. Parse query string:
   - Tokenize like documents
   - Identify operators (&, |, !, <->, *, ())

2. Build syntax tree:
   - Apply operator precedence
   - Handle parenthesis nesting

3. For each lexeme in tree:
   a. Normalize using same dictionary as documents
   b. Handle prefix matches (trailing *)

4. Return TsQuery root node
```

### Algorithm: Search Execution

```
Input:  TsQuery, index, current_xid
Output: Matching TIDs with ranks

1. Evaluate query tree:
   Function evaluate(node):
     If node.op == VALUE:
        - Search GIN for lexeme
        - Return posting list (TIDs with positions)
     
     If node.op == AND:
        left = evaluate(node.left)
        right = evaluate(node.right)
        Return intersection(left, right)
     
     If node.op == OR:
        left = evaluate(node.left)
        right = evaluate(node.right)
        Return union(left, right)
     
     If node.op == NOT:
        operand = evaluate(node.left)
        Return all_docs - operand
     
     If node.op == PHRASE:
        left = evaluate(node.left)
        right = evaluate(node.right)
        Return phrase_match(left, right, distance=1)

2. For phrase matching:
   - Check that positions in right follow positions in left
   - Distance must match constraint

3. Filter by MGA visibility

4. Compute relevance ranks (if requested):
   For each matching TID:
   rank = ts_rank(tsvector, tsquery, normalization_flags)

5. Return sorted results
```

### Algorithm: Relevance Ranking (ts_rank)

```
Input:  tsvector, tsquery, normalization_flags
Output: Relevance score [0, 1]

1. Calculate harmonic mean distance:
   - For each query lexeme found in document:
     * Distance = 1 + sum of position gaps
   - H = 1 / sum(1/distance for all matches)

2. Apply length normalization:
   - Typical normalization: 
     norm = 1 / (1 + log(document_length))

3. Apply weight adjustments:
   - Weights: A=1.0, B=0.4, C=0.2, D=0.1
   - Sum weighted occurrence counts

4. Combine:
   rank = H * norm * weight_sum / query_lexeme_count

5. Return rank in [0, 1]
```

### Algorithm: Highlighting (ts_headline)

```
Input:  document_text, tsquery, options
Output: Highlighted excerpt

1. Find matches:
   - Execute query to get matching positions
   - Map positions back to text offsets

2. Select excerpt:
   - Find densest match region
   - Expand to min_words / max_words boundaries
   - Prefer regions with highest rank contribution

3. Highlight matches:
   - Wrap matching terms with <b>...</b>
   - Add ellipses for truncated regions

4. Return formatted excerpt
```

## Text Search Configuration

```cpp
// Source: scratchbird/core/ts_config.h
struct TextSearchConfig {
    std::string name;                 // Config name (e.g., "english")
    ParserType parser;                // Parser to use
    std::vector<DictionaryRef> dicts; // Dictionaries in order
    std::vector<std::string> stop_words;
    
    // Mappings:
    // Token type -> Dictionary list
    std::map<TokenType, std::vector<std::string>> type_dict_map;
};
```

**Built-in Configurations:**
| Config | Parser | Dictionary | Stemmer |
|--------|--------|------------|---------|
| simple | default | none | none |
| english | default | english | snowball english |
| spanish | default | spanish | snowball spanish |
| french | default | french | snowball french |

## Invariants

| Invariant | Description | Verification |
|-----------|-------------|--------------|
| I1 | Same normalization for documents and queries | Consistent dictionary |
| I2 | Position values < 16384 | Insert validation |
| I3 | Posting lists sorted by TID | GIN property |
| I4 | Pending list flushed on threshold | Auto-flush trigger |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `SB_ERR_TS_CONFIG_NOT_FOUND` | Unknown text search config | Use default |
| `SB_ERR_TS_PARSE_ERROR` | Invalid query syntax | Return parse error |
| `SB_ERR_TS_DICT_FAILURE` | Dictionary load failed | Use simple parser |

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `fulltext.pending_limit` | 4096 | Entries before flush |
| `fulltext.max_positions` | 256 | Max positions per lexeme/doc |
| `fulltext.default_config` | 'english' | Default language config |

## Test Coverage

| Test File | Coverage |
|-----------|----------|
| `test_fulltext_index.cpp` | Core full-text operations |
| `test_tsvector_ops.cpp` | TsVector parsing, ranking |
| `test_tsquery_parse.cpp` | Query parsing |

## Related Specifications

- [index_gin.md](./index_gin.md) - GIN infrastructure
- [index_ngram.md](./index_ngram.md) - N-gram text search

## References

- PostgreSQL Full Text Search documentation
- Snowball stemmer library

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Comprehensive specification |
