# Full-Text Search

[Operations Guide README](../../README.md)

## Synopsis

Full-text search enables efficient text searching and ranking.

## Concepts

### Document
A textual unit to search (row, field, combination).

### Query
Search terms with operators.

### Vector (TSVECTOR)
Optimized document representation with word positions.

### Query (TSQUERY)
Normalized search query.

## Basic Usage

### Creating Search Index

```sql
-- Create GIN index for full-text search
CREATE INDEX idx_articles_search ON articles 
    USING GIN (to_tsvector('english', content));

-- Or use generated column
ALTER TABLE articles 
    ADD COLUMN search_vector TSVECTOR 
    GENERATED ALWAYS AS (to_tsvector('english', title || ' ' || content)) STORED;

CREATE INDEX idx_articles_search ON articles USING GIN (search_vector);
```

### Simple Search

```sql
-- Basic search
SELECT * FROM articles 
WHERE search_vector @@ to_tsquery('english', 'database');

-- Multiple terms (AND)
SELECT * FROM articles 
WHERE search_vector @@ to_tsquery('english', 'database & performance');

-- OR search
SELECT * FROM articles 
WHERE search_vector @@ to_tsquery('english', 'database | postgres');

-- Negation
SELECT * FROM articles 
WHERE search_vector @@ to_tsquery('english', 'database & !performance');
```

## Ranking Results

```sql
-- Rank by relevance
SELECT 
    title,
    ts_rank(search_vector, query) as rank
FROM articles,
    to_tsquery('english', 'database optimization') query
WHERE search_vector @@ query
ORDER BY rank DESC;

-- Rank with weights
SELECT 
    title,
    ts_rank_cd('{0.1, 0.2, 0.4, 1.0}', search_vector, query) as rank
FROM articles,
    to_tsquery('english', 'database') query
WHERE search_vector @@ query
ORDER BY rank DESC;
```

## Highlighting Results

```sql
-- Highlight matches
SELECT 
    title,
    ts_headline('english', content, query) as highlight
FROM articles,
    to_tsquery('english', 'database') query
WHERE search_vector @@ query;

-- Custom highlighting
SELECT ts_headline(
    'english',
    content,
    query,
    'StartSel=<mark>, StopSel=</mark>, MaxWords=50, MinWords=10'
) FROM articles, to_tsquery('database') query
WHERE search_vector @@ query;
```

## Advanced Queries

### Phrase Search

```sql
-- Exact phrase
SELECT * FROM articles 
WHERE search_vector @@ to_tsquery('english', 'database <-> optimization');
-- <-> means adjacent words

-- Within N words
SELECT * FROM articles 
WHERE search_vector @@ to_tsquery('english', 'database <5> optimization');
```

### Wildcard Search

```sql
-- Prefix matching
SELECT * FROM articles 
WHERE search_vector @@ to_tsquery('english', 'optim:*');
-- Matches optimize, optimization, optimizing, etc.
```

### Weighted Terms

```sql
-- Different weights for different fields
UPDATE articles SET search_vector = 
    setweight(to_tsvector('english', title), 'A') ||
    setweight(to_tsvector('english', content), 'B');

-- Search with field preference
SELECT * FROM articles 
WHERE search_vector @@ to_tsquery('english', 'database')
ORDER BY ts_rank('{1.0, 0.4, 0.2, 0.1}', search_vector, 
    to_tsquery('english', 'database')) DESC;
```

## Languages

```sql
-- Use language-specific processing
CREATE INDEX idx_german ON articles 
    USING GIN (to_tsvector('german', content));

-- Available languages
SELECT cfgname FROM pg_ts_config;
-- english, german, french, spanish, etc.
```

## Maintenance

```sql
-- Update statistics
ANALYZE articles;

-- For large updates, batch processing
UPDATE articles 
SET search_vector = to_tsvector('english', content)
WHERE id BETWEEN 1 AND 10000;
```

## Examples

```sql
-- Blog search
SELECT 
    id,
    title,
    ts_rank(search_vector, plainto_tsquery('english', 'search terms')) as rank,
    ts_headline('english', content, plainto_tsquery('english', 'search terms')) as snippet
FROM blog_posts
WHERE search_vector @@ plainto_tsquery('english', 'search terms')
ORDER BY rank DESC
LIMIT 20;

-- Product search with filtering
SELECT 
    p.name,
    ts_rank(p.search_vector, query) as rank
FROM products p,
    to_tsquery('english', 'laptop & performance') query
WHERE p.search_vector @@ query
    AND p.category = 'electronics'
    AND p.price BETWEEN 500 AND 2000
ORDER BY rank DESC;
```

## See Also

- [GIN Indexes](../../../language_reference/syntax_guide/ddl/table_and_constraints/04_create_index.md)
- [Text Search Types](../../type_system/03_character_and_text_types.md)
