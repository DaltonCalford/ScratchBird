-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - TSQUERY
-- Description: Comprehensive TSQUERY (text search query) testing
-- ============================================================================

-- TSQUERY stores text search queries with boolean operators
-- Used to search TSVECTOR documents
-- Supports: AND (&), OR (|), NOT (!), phrase search, proximity

-- Create test database
CREATE DATABASE test_tsquery_db;
USE test_tsquery_db;

-- ============================================================================
-- Section 1: Basic TSQUERY Creation
-- ============================================================================

CREATE TABLE test_tsquery_basic (
    id INT PRIMARY KEY,
    query TSQUERY,
    description VARCHAR(200)
);

-- Simple queries
INSERT INTO test_tsquery_basic VALUES
    (1, 'database'::TSQUERY, 'Single word'),
    (2, 'full & text'::TSQUERY, 'AND operation'),
    (3, 'sql | nosql'::TSQUERY, 'OR operation'),
    (4, '!mysql'::TSQUERY, 'NOT operation'),
    (5, 'search & !stop'::TSQUERY, 'Combined operators');

SELECT id, query, description FROM test_tsquery_basic ORDER BY id;

-- ============================================================================
-- Section 2: Creating TSQUERY with to_tsquery()
-- ============================================================================

CREATE TABLE test_tsquery_creation (
    id INT PRIMARY KEY,
    input_text TEXT,
    query TSQUERY
);

-- to_tsquery() normalizes and parses query syntax
INSERT INTO test_tsquery_creation VALUES
    (1, 'database', to_tsquery('english', 'database')),
    (2, 'database & search', to_tsquery('english', 'database & search')),
    (3, 'sql | postgresql', to_tsquery('english', 'sql | postgresql')),
    (4, '!oracle', to_tsquery('english', '!oracle')),
    (5, '(full & text) | search', to_tsquery('english', '(full & text) | search'));

SELECT id, input_text, query FROM test_tsquery_creation ORDER BY id;

-- ============================================================================
-- Section 3: plainto_tsquery() - Plain Text Queries
-- ============================================================================

CREATE TABLE test_tsquery_plain (
    id INT PRIMARY KEY,
    user_input TEXT,
    query TSQUERY
);

-- plainto_tsquery() converts plain text to TSQUERY (AND between words)
INSERT INTO test_tsquery_plain VALUES
    (1, 'database management', plainto_tsquery('english', 'database management')),
    (2, 'full text search', plainto_tsquery('english', 'full text search')),
    (3, 'postgresql tutorial', plainto_tsquery('english', 'postgresql tutorial'));

-- Automatically ANDs words together
SELECT id, user_input, query FROM test_tsquery_plain ORDER BY id;

-- ============================================================================
-- Section 4: phraseto_tsquery() - Phrase Queries
-- ============================================================================

CREATE TABLE test_tsquery_phrase (
    id INT PRIMARY KEY,
    phrase TEXT,
    query TSQUERY
);

-- phraseto_tsquery() creates phrase queries (words must be adjacent)
INSERT INTO test_tsquery_phrase VALUES
    (1, 'database system', phraseto_tsquery('english', 'database system')),
    (2, 'full text search', phraseto_tsquery('english', 'full text search')),
    (3, 'relational database', phraseto_tsquery('english', 'relational database'));

-- Uses <-> operator for adjacency
SELECT id, phrase, query FROM test_tsquery_phrase ORDER BY id;

-- ============================================================================
-- Section 5: websearch_to_tsquery() - Web Search Syntax
-- ============================================================================

CREATE TABLE test_tsquery_websearch (
    id INT PRIMARY KEY,
    web_query TEXT,
    parsed_query TSQUERY
);

-- websearch_to_tsquery() accepts Google-like syntax
INSERT INTO test_tsquery_websearch VALUES
    (1, 'database', websearch_to_tsquery('english', 'database')),
    (2, 'database OR search', websearch_to_tsquery('english', 'database OR search')),
    (3, 'database -mysql', websearch_to_tsquery('english', 'database -mysql')),
    (4, '"full text search"', websearch_to_tsquery('english', '"full text search"'));

SELECT id, web_query, parsed_query FROM test_tsquery_websearch ORDER BY id;

-- ============================================================================
-- Section 6: TSQUERY Operators - AND, OR, NOT
-- ============================================================================

CREATE TABLE test_tsquery_operators (
    id INT PRIMARY KEY,
    operator_name VARCHAR(50),
    query TSQUERY
);

INSERT INTO test_tsquery_operators VALUES
    (1, 'AND (&)', 'database & search'::TSQUERY),
    (2, 'OR (|)', 'sql | nosql'::TSQUERY),
    (3, 'NOT (!)', '!oracle'::TSQUERY),
    (4, 'Combined', '(database & search) | index'::TSQUERY),
    (5, 'Complex', '(full & text) & (search | index) & !slow'::TSQUERY);

SELECT id, operator_name, query FROM test_tsquery_operators ORDER BY id;

-- ============================================================================
-- Section 7: Phrase Search with <-> Operator
-- ============================================================================

CREATE TABLE test_tsquery_phrase_operators (
    id INT PRIMARY KEY,
    description VARCHAR(200),
    query TSQUERY
);

-- <-> requires words to be adjacent
INSERT INTO test_tsquery_phrase_operators VALUES
    (1, 'Adjacent words', 'database <-> system'::TSQUERY),
    (2, 'Three word phrase', 'full <-> text <-> search'::TSQUERY),
    (3, 'Distance of 2', 'database <2> system'::TSQUERY),
    (4, 'Phrase OR word', '(database <-> management) | system'::TSQUERY);

SELECT id, description, query FROM test_tsquery_phrase_operators ORDER BY id;

-- ============================================================================
-- Section 8: Matching TSVECTOR with TSQUERY - @@ Operator
-- ============================================================================

CREATE TABLE test_tsquery_matching (
    doc_id INT PRIMARY KEY,
    content TEXT,
    doc_vector TSVECTOR
);

INSERT INTO test_tsquery_matching VALUES
    (1, 'PostgreSQL is a powerful relational database system',
        to_tsvector('english', 'PostgreSQL is a powerful relational database system')),
    (2, 'Full-text search allows efficient document searching',
        to_tsvector('english', 'Full-text search allows efficient document searching')),
    (3, 'Database management requires careful planning',
        to_tsvector('english', 'Database management requires careful planning'));

-- Simple match
SELECT doc_id, content FROM test_tsquery_matching
WHERE doc_vector @@ to_tsquery('english', 'database')
ORDER BY doc_id;

-- AND query
SELECT doc_id, content FROM test_tsquery_matching
WHERE doc_vector @@ to_tsquery('english', 'database & system')
ORDER BY doc_id;

-- OR query
SELECT doc_id, content FROM test_tsquery_matching
WHERE doc_vector @@ to_tsquery('english', 'search | management')
ORDER BY doc_id;

-- NOT query
SELECT doc_id, content FROM test_tsquery_matching
WHERE doc_vector @@ to_tsquery('english', 'database & !search')
ORDER BY doc_id;

-- ============================================================================
-- Section 9: TSQUERY Functions - querytree, numnode
-- ============================================================================

CREATE TABLE test_tsquery_functions (
    id INT PRIMARY KEY,
    query TSQUERY
);

INSERT INTO test_tsquery_functions VALUES
    (1, to_tsquery('english', 'database')),
    (2, to_tsquery('english', 'database & search')),
    (3, to_tsquery('english', '(full & text) | search'));

-- querytree(): display query tree
SELECT id, query, querytree(query) AS tree FROM test_tsquery_functions ORDER BY id;

-- numnode(): count nodes in query tree
SELECT id, query, numnode(query) AS node_count FROM test_tsquery_functions ORDER BY id;

-- ============================================================================
-- Section 10: TSQUERY Combination Operators
-- ============================================================================

-- Combine queries with &&, ||, !!
SELECT 'database'::TSQUERY && 'search'::TSQUERY AS and_combined;
SELECT 'sql'::TSQUERY || 'nosql'::TSQUERY AS or_combined;
SELECT !!'database'::TSQUERY AS double_negation;

-- Rewrite queries
CREATE TABLE test_tsquery_rewrite (
    id INT PRIMARY KEY,
    original TSQUERY,
    rewritten TSQUERY
);

INSERT INTO test_tsquery_rewrite VALUES
    (1, 'postgresql'::TSQUERY, 'postgres'::TSQUERY),
    (2, 'db'::TSQUERY, 'database'::TSQUERY);

-- ts_rewrite: substitute terms
SELECT ts_rewrite('postgresql & db'::TSQUERY,
                  'SELECT original, rewritten FROM test_tsquery_rewrite'::TEXT) AS rewritten_query;

-- ============================================================================
-- Section 11: Real-world Use Case - Document Search
-- ============================================================================

CREATE TABLE test_tsquery_documents (
    doc_id INT PRIMARY KEY,
    title TEXT,
    content TEXT,
    search_vector TSVECTOR
);

CREATE INDEX idx_doc_search ON test_tsquery_documents USING GIN (search_vector);

INSERT INTO test_tsquery_documents VALUES
    (1, 'Introduction to Databases',
        'Databases are systems for storing and retrieving data efficiently',
        to_tsvector('english', 'Introduction to Databases Databases are systems for storing and retrieving data efficiently')),
    (2, 'SQL vs NoSQL',
        'Comparing SQL databases with NoSQL alternatives',
        to_tsvector('english', 'SQL vs NoSQL Comparing SQL databases with NoSQL alternatives')),
    (3, 'Full-Text Search in PostgreSQL',
        'PostgreSQL provides powerful full-text search capabilities',
        to_tsvector('english', 'Full-Text Search in PostgreSQL PostgreSQL provides powerful full-text search capabilities'));

-- Simple search
SELECT doc_id, title FROM test_tsquery_documents
WHERE search_vector @@ to_tsquery('english', 'database')
ORDER BY doc_id;

-- Boolean search
SELECT doc_id, title FROM test_tsquery_documents
WHERE search_vector @@ to_tsquery('english', 'database & !nosql')
ORDER BY doc_id;

-- Phrase search
SELECT doc_id, title FROM test_tsquery_documents
WHERE search_vector @@ phraseto_tsquery('english', 'full text search')
ORDER BY doc_id;

-- User-friendly search
SELECT doc_id, title FROM test_tsquery_documents
WHERE search_vector @@ plainto_tsquery('english', 'postgresql search')
ORDER BY doc_id;

-- ============================================================================
-- Section 12: Real-world Use Case - E-commerce Product Search
-- ============================================================================

CREATE TABLE test_tsquery_products (
    product_id INT PRIMARY KEY,
    name VARCHAR(200),
    description TEXT,
    category VARCHAR(100),
    search_vector TSVECTOR
);

CREATE INDEX idx_product_search ON test_tsquery_products USING GIN (search_vector);

INSERT INTO test_tsquery_products VALUES
    (1, 'Wireless Gaming Mouse',
        'High-precision wireless mouse designed for gaming with RGB lighting',
        'Electronics',
        to_tsvector('english', 'Wireless Gaming Mouse High-precision wireless mouse designed for gaming with RGB lighting Electronics')),
    (2, 'Mechanical Keyboard',
        'RGB mechanical keyboard with blue switches and programmable keys',
        'Electronics',
        to_tsvector('english', 'Mechanical Keyboard RGB mechanical keyboard with blue switches and programmable keys Electronics')),
    (3, 'Ergonomic Office Chair',
        'Comfortable office chair with lumbar support and adjustable height',
        'Furniture',
        to_tsvector('english', 'Ergonomic Office Chair Comfortable office chair with lumbar support and adjustable height Furniture'));

-- Search for wireless products
SELECT product_id, name FROM test_tsquery_products
WHERE search_vector @@ to_tsquery('english', 'wireless')
ORDER BY product_id;

-- Search for gaming AND RGB
SELECT product_id, name FROM test_tsquery_products
WHERE search_vector @@ to_tsquery('english', 'gaming & rgb')
ORDER BY product_id;

-- Search for keyboard OR mouse
SELECT product_id, name FROM test_tsquery_products
WHERE search_vector @@ to_tsquery('english', 'keyboard | mouse')
ORDER BY product_id;

-- Exclude furniture
SELECT product_id, name FROM test_tsquery_products
WHERE search_vector @@ to_tsquery('english', 'ergonomic & !furniture')
ORDER BY product_id;

-- ============================================================================
-- Section 13: Search with Ranking - ts_rank
-- ============================================================================

CREATE TABLE test_tsquery_ranking (
    article_id INT PRIMARY KEY,
    title TEXT,
    content TEXT,
    search_vector TSVECTOR
);

INSERT INTO test_tsquery_ranking VALUES
    (1, 'Database Fundamentals',
        'Introduction to database concepts and database design principles',
        to_tsvector('english', 'Database Fundamentals Introduction to database concepts and database design principles')),
    (2, 'Advanced Database Topics',
        'Database performance tuning and database optimization techniques',
        to_tsvector('english', 'Advanced Database Topics Database performance tuning and database optimization techniques')),
    (3, 'SQL Tutorial',
        'Learn SQL queries and database management with practical examples',
        to_tsvector('english', 'SQL Tutorial Learn SQL queries and database management with practical examples'));

-- Search and rank results
SELECT article_id, title,
       ts_rank(search_vector, to_tsquery('english', 'database')) AS rank
FROM test_tsquery_ranking
WHERE search_vector @@ to_tsquery('english', 'database')
ORDER BY rank DESC;

-- ts_rank_cd (with cover density)
SELECT article_id, title,
       ts_rank_cd(search_vector, to_tsquery('english', 'database')) AS rank_cd
FROM test_tsquery_ranking
WHERE search_vector @@ to_tsquery('english', 'database')
ORDER BY rank_cd DESC;

-- ============================================================================
-- Section 14: Highlighting Search Results - ts_headline
-- ============================================================================

CREATE TABLE test_tsquery_highlighting (
    post_id INT PRIMARY KEY,
    content TEXT,
    search_vector TSVECTOR
);

INSERT INTO test_tsquery_highlighting VALUES
    (1, 'PostgreSQL is a powerful open-source relational database management system with advanced features',
        to_tsvector('english', 'PostgreSQL is a powerful open-source relational database management system with advanced features')),
    (2, 'Full-text search capabilities in PostgreSQL allow for sophisticated document searching and ranking',
        to_tsvector('english', 'Full-text search capabilities in PostgreSQL allow for sophisticated document searching and ranking'));

-- Highlight matching terms
SELECT post_id,
       ts_headline('english', content, to_tsquery('english', 'database'))
FROM test_tsquery_highlighting
WHERE search_vector @@ to_tsquery('english', 'database')
ORDER BY post_id;

SELECT post_id,
       ts_headline('english', content, to_tsquery('english', 'search'))
FROM test_tsquery_highlighting
WHERE search_vector @@ to_tsquery('english', 'search')
ORDER BY post_id;

-- ============================================================================
-- Section 15: Complex Query Examples
-- ============================================================================

CREATE TABLE test_tsquery_complex (
    id INT PRIMARY KEY,
    query_description VARCHAR(200),
    query TSQUERY
);

INSERT INTO test_tsquery_complex VALUES
    (1, 'Exact phrase', phraseto_tsquery('english', 'database management system')),
    (2, 'Multiple ORs', to_tsquery('english', 'postgresql | mysql | oracle | mongodb')),
    (3, 'Nested boolean', to_tsquery('english', '(database & (sql | nosql)) & !deprecated')),
    (4, 'Phrase with alternatives', to_tsquery('english', '(database <-> system) | (data <-> management)')),
    (5, 'Complex exclusion', to_tsquery('english', '(search & index) & !(slow | outdated)'));

SELECT id, query_description, query FROM test_tsquery_complex ORDER BY id;

-- ============================================================================
-- Section 16: TSQUERY with Different Languages
-- ============================================================================

CREATE TABLE test_tsquery_languages (
    id INT PRIMARY KEY,
    language VARCHAR(20),
    text TEXT,
    query TSQUERY
);

INSERT INTO test_tsquery_languages VALUES
    (1, 'english', 'running databases', to_tsquery('english', 'run & database')),
    (2, 'simple', 'running databases', to_tsquery('simple', 'running & databases'));

-- English stems 'running' to 'run', simple doesn't
SELECT id, language, text, query FROM test_tsquery_languages ORDER BY id;

-- Test matching
SELECT id, language,
       to_tsvector(language, text) @@ query AS matches
FROM test_tsquery_languages ORDER BY id;

-- ============================================================================
-- Section 17: NULL Handling
-- ============================================================================

CREATE TABLE test_tsquery_nulls (
    id INT PRIMARY KEY,
    query TSQUERY
);

INSERT INTO test_tsquery_nulls VALUES
    (1, 'database'::TSQUERY),
    (2, NULL),
    (3, 'search'::TSQUERY);

SELECT id, query, query IS NULL AS is_null FROM test_tsquery_nulls ORDER BY id;

-- ============================================================================
-- Section 18: TSQUERY Negation and Edge Cases
-- ============================================================================

CREATE TABLE test_tsquery_edge_cases (
    id INT PRIMARY KEY,
    description VARCHAR(200),
    query TSQUERY
);

INSERT INTO test_tsquery_edge_cases VALUES
    (1, 'Single negation', '!stop'::TSQUERY),
    (2, 'Double negation', '!!database'::TSQUERY),
    (3, 'Empty after normalization', to_tsquery('english', 'the a an')),
    (4, 'Parentheses grouping', '(a | b) & (c | d)'::TSQUERY);

SELECT id, description, query FROM test_tsquery_edge_cases ORDER BY id;

-- ============================================================================
-- Section 19: Performance Comparison
-- ============================================================================

CREATE TABLE test_tsquery_performance (
    id INT PRIMARY KEY,
    note TEXT
);

INSERT INTO test_tsquery_performance VALUES
    (1, 'to_tsquery() for boolean queries with operators'),
    (2, 'plainto_tsquery() for simple user input (ANDs words)'),
    (3, 'phraseto_tsquery() for exact phrase matching'),
    (4, 'websearch_to_tsquery() for Google-like syntax'),
    (5, 'GIN indexes make text search very fast'),
    (6, 'Use @@ operator to match TSQUERY against TSVECTOR'),
    (7, 'ts_rank() for relevance scoring'),
    (8, 'ts_headline() for result highlighting'),
    (9, 'Complex queries may be slower than simple ones'),
    (10, 'Index selectivity affects query performance');

SELECT id, note FROM test_tsquery_performance ORDER BY id;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_tsquery_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_tsquery_best_practices VALUES
    (1, 'Use TSQUERY for text search queries'),
    (2, 'Use to_tsquery() when query syntax is controlled'),
    (3, 'Use plainto_tsquery() for untrusted user input'),
    (4, 'Use websearch_to_tsquery() for web search boxes'),
    (5, 'Use phraseto_tsquery() for exact phrase searches'),
    (6, 'AND (&) narrows results, OR (|) expands results'),
    (7, 'NOT (!) excludes documents containing terms'),
    (8, '<-> operator for adjacent words (phrase search)'),
    (9, 'Combine with ts_rank() for relevance sorting'),
    (10, 'Use ts_headline() to highlight matching terms'),
    (11, 'TSQUERY normalizes and stems like TSVECTOR'),
    (12, 'Match language config between TSQUERY and TSVECTOR'),
    (13, 'Complex queries: use parentheses for grouping'),
    (14, 'querytree() to debug complex query structure'),
    (15, 'TSQUERY ideal for: search boxes, filters, document queries');

SELECT id, guideline FROM test_tsquery_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_tsquery_best_practices;
DROP TABLE test_tsquery_performance;
DROP TABLE test_tsquery_edge_cases;
DROP TABLE test_tsquery_nulls;
DROP TABLE test_tsquery_languages;
DROP TABLE test_tsquery_complex;
DROP TABLE test_tsquery_highlighting;
DROP TABLE test_tsquery_ranking;
DROP TABLE test_tsquery_products;
DROP TABLE test_tsquery_documents;
DROP TABLE test_tsquery_rewrite;
DROP TABLE test_tsquery_functions;
DROP TABLE test_tsquery_matching;
DROP TABLE test_tsquery_phrase_operators;
DROP TABLE test_tsquery_operators;
DROP TABLE test_tsquery_websearch;
DROP TABLE test_tsquery_phrase;
DROP TABLE test_tsquery_plain;
DROP TABLE test_tsquery_creation;
DROP TABLE test_tsquery_basic;

DROP DATABASE test_tsquery_db;

-- End of TSQUERY data type tests
