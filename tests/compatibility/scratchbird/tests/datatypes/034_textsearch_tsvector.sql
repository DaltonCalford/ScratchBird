-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - TSVECTOR
-- Description: Comprehensive TSVECTOR (text search vector) testing
-- ============================================================================

-- TSVECTOR stores preprocessed documents for full-text search
-- Contains lexemes (normalized words) with position information
-- Used for: document search, product search, content indexing

-- Create test database
CREATE DATABASE test_tsvector_db;
USE test_tsvector_db;

-- ============================================================================
-- Section 1: Basic TSVECTOR Creation
-- ============================================================================

CREATE TABLE test_tsvector_basic (
    id INT PRIMARY KEY,
    doc_vector TSVECTOR,
    description VARCHAR(200)
);

-- Manual TSVECTOR creation
INSERT INTO test_tsvector_basic VALUES
    (1, 'simple example'::TSVECTOR, 'Two lexemes'),
    (2, 'search engine'::TSVECTOR, 'Two words'),
    (3, 'full text search'::TSVECTOR, 'Three words'),
    (4, 'a the and'::TSVECTOR, 'Common words'),
    (5, '''hello'' ''world'''::TSVECTOR, 'Quoted lexemes');

SELECT id, doc_vector, description FROM test_tsvector_basic ORDER BY id;

-- ============================================================================
-- Section 2: Creating TSVECTOR with to_tsvector()
-- ============================================================================

CREATE TABLE test_tsvector_creation (
    id INT PRIMARY KEY,
    original_text TEXT,
    doc_vector TSVECTOR
);

-- to_tsvector() automatically normalizes and removes stop words
INSERT INTO test_tsvector_creation VALUES
    (1, 'The quick brown fox jumps over the lazy dog',
        to_tsvector('english', 'The quick brown fox jumps over the lazy dog')),
    (2, 'PostgreSQL is a powerful database',
        to_tsvector('english', 'PostgreSQL is a powerful database')),
    (3, 'Full-text search makes finding information easy',
        to_tsvector('english', 'Full-text search makes finding information easy'));

SELECT id, original_text, doc_vector FROM test_tsvector_creation ORDER BY id;

-- ============================================================================
-- Section 3: TSVECTOR with Position Information
-- ============================================================================

CREATE TABLE test_tsvector_positions (
    id INT PRIMARY KEY,
    text_content TEXT,
    doc_vector TSVECTOR
);

-- Position information shows where words appear
INSERT INTO test_tsvector_positions VALUES
    (1, 'search and search again',
        to_tsvector('english', 'search and search again')),
    (2, 'database database systems',
        to_tsvector('english', 'database database systems'));

-- TSVECTOR includes positions: 'word':position
SELECT id, doc_vector FROM test_tsvector_positions ORDER BY id;

-- ============================================================================
-- Section 4: TSVECTOR Functions - length, strip
-- ============================================================================

CREATE TABLE test_tsvector_functions (
    id INT PRIMARY KEY,
    doc_vector TSVECTOR
);

INSERT INTO test_tsvector_functions VALUES
    (1, to_tsvector('english', 'The quick brown fox')),
    (2, to_tsvector('english', 'A simple test document')),
    (3, to_tsvector('english', 'Full text search functionality'));

-- length(): number of lexemes
SELECT id, doc_vector, length(doc_vector) AS num_lexemes
FROM test_tsvector_functions ORDER BY id;

-- strip(): remove position information
SELECT id, doc_vector, strip(doc_vector) AS stripped
FROM test_tsvector_functions ORDER BY id;

-- ============================================================================
-- Section 5: TSVECTOR Concatenation
-- ============================================================================

CREATE TABLE test_tsvector_concat (
    id INT PRIMARY KEY,
    title_vector TSVECTOR,
    body_vector TSVECTOR
);

INSERT INTO test_tsvector_concat VALUES
    (1, to_tsvector('english', 'Database Systems'),
        to_tsvector('english', 'Relational databases store data in tables')),
    (2, to_tsvector('english', 'Full Text Search'),
        to_tsvector('english', 'Search functionality for text documents'));

-- Concatenate TSVECTOR values with ||
SELECT id,
       title_vector || body_vector AS combined_vector
FROM test_tsvector_concat ORDER BY id;

-- ============================================================================
-- Section 6: Different Languages
-- ============================================================================

CREATE TABLE test_tsvector_languages (
    id INT PRIMARY KEY,
    language VARCHAR(20),
    text_content TEXT,
    doc_vector TSVECTOR
);

-- Different language configurations
INSERT INTO test_tsvector_languages VALUES
    (1, 'english', 'The running horses',
        to_tsvector('english', 'The running horses')),
    (2, 'simple', 'The running horses',
        to_tsvector('simple', 'The running horses')),
    (3, 'english', 'databases and searching',
        to_tsvector('english', 'databases and searching'));

-- Note: 'english' stems words (running -> run), 'simple' does not
SELECT id, language, text_content, doc_vector FROM test_tsvector_languages ORDER BY id;

-- ============================================================================
-- Section 7: Real-world Use Case - Document Search
-- ============================================================================

CREATE TABLE test_tsvector_documents (
    doc_id INT PRIMARY KEY,
    title TEXT,
    content TEXT,
    doc_vector TSVECTOR
);

-- Create GIN index for fast text search
CREATE INDEX idx_doc_vector ON test_tsvector_documents USING GIN (doc_vector);

INSERT INTO test_tsvector_documents VALUES
    (1, 'Introduction to Databases',
        'Databases are organized collections of data. They store information in structured formats.',
        to_tsvector('english', 'Introduction to Databases Databases are organized collections of data. They store information in structured formats.')),
    (2, 'Full Text Search Guide',
        'Full-text search allows you to search for words within documents efficiently.',
        to_tsvector('english', 'Full Text Search Guide Full-text search allows you to search for words within documents efficiently.')),
    (3, 'SQL Query Optimization',
        'Optimizing SQL queries improves database performance significantly.',
        to_tsvector('english', 'SQL Query Optimization Optimizing SQL queries improves database performance significantly.'));

-- Search for documents containing specific words
SELECT doc_id, title FROM test_tsvector_documents
WHERE doc_vector @@ to_tsquery('english', 'database')
ORDER BY doc_id;

SELECT doc_id, title FROM test_tsvector_documents
WHERE doc_vector @@ to_tsquery('english', 'search')
ORDER BY doc_id;

-- ============================================================================
-- Section 8: Real-world Use Case - Product Catalog
-- ============================================================================

CREATE TABLE test_tsvector_products (
    product_id INT PRIMARY KEY,
    product_name VARCHAR(200),
    description TEXT,
    category VARCHAR(100),
    search_vector TSVECTOR
);

CREATE INDEX idx_product_search ON test_tsvector_products USING GIN (search_vector);

INSERT INTO test_tsvector_products VALUES
    (1, 'Wireless Mouse', 'Ergonomic wireless mouse with precision tracking', 'Electronics',
        to_tsvector('english', 'Wireless Mouse Ergonomic wireless mouse with precision tracking Electronics')),
    (2, 'Mechanical Keyboard', 'RGB mechanical gaming keyboard with blue switches', 'Electronics',
        to_tsvector('english', 'Mechanical Keyboard RGB mechanical gaming keyboard with blue switches Electronics')),
    (3, 'Office Chair', 'Comfortable ergonomic office chair with lumbar support', 'Furniture',
        to_tsvector('english', 'Office Chair Comfortable ergonomic office chair with lumbar support Furniture'));

-- Search products
SELECT product_id, product_name FROM test_tsvector_products
WHERE search_vector @@ to_tsquery('english', 'wireless')
ORDER BY product_id;

SELECT product_id, product_name FROM test_tsvector_products
WHERE search_vector @@ to_tsquery('english', 'ergonomic')
ORDER BY product_id;

-- ============================================================================
-- Section 9: Weighted TSVECTOR - setweight()
-- ============================================================================

CREATE TABLE test_tsvector_weighted (
    article_id INT PRIMARY KEY,
    title TEXT,
    abstract TEXT,
    body TEXT,
    search_vector TSVECTOR
);

-- Weight lexemes: A (most important) to D (least important)
INSERT INTO test_tsvector_weighted VALUES
    (1, 'Database Systems',
        'Introduction to relational databases',
        'Databases store data in tables with rows and columns',
        setweight(to_tsvector('english', 'Database Systems'), 'A') ||
        setweight(to_tsvector('english', 'Introduction to relational databases'), 'B') ||
        setweight(to_tsvector('english', 'Databases store data in tables with rows and columns'), 'C'));

INSERT INTO test_tsvector_weighted VALUES
    (2, 'Full Text Search',
        'Searching through documents efficiently',
        'Full-text search enables fast querying of large text corpora',
        setweight(to_tsvector('english', 'Full Text Search'), 'A') ||
        setweight(to_tsvector('english', 'Searching through documents efficiently'), 'B') ||
        setweight(to_tsvector('english', 'Full-text search enables fast querying of large text corpora'), 'C'));

SELECT article_id, title, search_vector FROM test_tsvector_weighted ORDER BY article_id;

-- ============================================================================
-- Section 10: Automatic TSVECTOR Updates with Triggers
-- ============================================================================

CREATE TABLE test_tsvector_auto (
    post_id INT PRIMARY KEY,
    title TEXT,
    content TEXT,
    search_vector TSVECTOR
);

-- Trigger to automatically update search_vector
CREATE FUNCTION update_search_vector() RETURNS TRIGGER AS $$
BEGIN
    NEW.search_vector :=
        setweight(to_tsvector('english', COALESCE(NEW.title, '')), 'A') ||
        setweight(to_tsvector('english', COALESCE(NEW.content, '')), 'B');
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER tsvector_update BEFORE INSERT OR UPDATE
ON test_tsvector_auto FOR EACH ROW
EXECUTE FUNCTION update_search_vector();

-- Insert without specifying search_vector (automatically generated)
INSERT INTO test_tsvector_auto (post_id, title, content) VALUES
    (1, 'Getting Started with Databases', 'This post introduces database concepts'),
    (2, 'Advanced SQL Techniques', 'Learn advanced SQL query patterns');

SELECT post_id, title, search_vector FROM test_tsvector_auto ORDER BY post_id;

-- Update triggers automatic re-indexing
UPDATE test_tsvector_auto SET content = 'Updated content about SQL' WHERE post_id = 2;
SELECT post_id, title, search_vector FROM test_tsvector_auto WHERE post_id = 2;

-- ============================================================================
-- Section 11: Generated Columns for TSVECTOR
-- ============================================================================

CREATE TABLE test_tsvector_generated (
    item_id INT PRIMARY KEY,
    name TEXT,
    description TEXT,
    search_vector TSVECTOR GENERATED ALWAYS AS (
        to_tsvector('english', COALESCE(name, '') || ' ' || COALESCE(description, ''))
    ) STORED
);

CREATE INDEX idx_item_search ON test_tsvector_generated USING GIN (search_vector);

INSERT INTO test_tsvector_generated (item_id, name, description) VALUES
    (1, 'Laptop Computer', 'High-performance laptop for professionals'),
    (2, 'USB Cable', 'USB-C charging cable'),
    (3, 'Monitor Stand', 'Adjustable ergonomic monitor stand');

-- search_vector automatically populated
SELECT item_id, name, search_vector FROM test_tsvector_generated ORDER BY item_id;

-- Search works immediately
SELECT item_id, name FROM test_tsvector_generated
WHERE search_vector @@ to_tsquery('english', 'laptop')
ORDER BY item_id;

-- ============================================================================
-- Section 12: TSVECTOR Statistics
-- ============================================================================

CREATE TABLE test_tsvector_stats (
    id INT PRIMARY KEY,
    doc TEXT,
    doc_vector TSVECTOR
);

INSERT INTO test_tsvector_stats VALUES
    (1, 'The quick brown fox jumps over the lazy dog',
        to_tsvector('english', 'The quick brown fox jumps over the lazy dog')),
    (2, 'Database systems store and retrieve data efficiently',
        to_tsvector('english', 'Database systems store and retrieve data efficiently'));

-- Get lexeme count
SELECT id, length(doc_vector) AS lexeme_count FROM test_tsvector_stats ORDER BY id;

-- Extract all lexemes (using unnest)
SELECT id, lexeme FROM test_tsvector_stats, unnest(doc_vector) AS lexeme ORDER BY id, lexeme;

-- ============================================================================
-- Section 13: TSVECTOR Comparison
-- ============================================================================

CREATE TABLE test_tsvector_comparison (
    id INT PRIMARY KEY,
    vec TSVECTOR
);

INSERT INTO test_tsvector_comparison VALUES
    (1, 'dog cat'::TSVECTOR),
    (2, 'cat dog'::TSVECTOR),
    (3, 'dog'::TSVECTOR);

-- Equality (order doesn't matter for lexemes)
SELECT id FROM test_tsvector_comparison WHERE vec = 'cat dog'::TSVECTOR ORDER BY id;

-- Inequality
SELECT id FROM test_tsvector_comparison WHERE vec != 'dog'::TSVECTOR ORDER BY id;

-- ============================================================================
-- Section 14: TSVECTOR Concatenation Strategies
-- ============================================================================

CREATE TABLE test_tsvector_concat_strategies (
    id INT PRIMARY KEY,
    field1 TEXT,
    field2 TEXT,
    field3 TEXT
);

INSERT INTO test_tsvector_concat_strategies VALUES
    (1, 'Database', 'Management', 'Systems'),
    (2, 'Full', 'Text', 'Search');

-- Different concatenation approaches
SELECT id,
       to_tsvector('english', field1 || ' ' || field2 || ' ' || field3) AS concat_first,
       to_tsvector('english', field1) || to_tsvector('english', field2) || to_tsvector('english', field3) AS concat_vectors
FROM test_tsvector_concat_strategies ORDER BY id;

-- With weights
SELECT id,
       setweight(to_tsvector('english', field1), 'A') ||
       setweight(to_tsvector('english', field2), 'B') ||
       setweight(to_tsvector('english', field3), 'C') AS weighted_concat
FROM test_tsvector_concat_strategies ORDER BY id;

-- ============================================================================
-- Section 15: NULL Handling
-- ============================================================================

CREATE TABLE test_tsvector_nulls (
    id INT PRIMARY KEY,
    text_content TEXT,
    doc_vector TSVECTOR
);

INSERT INTO test_tsvector_nulls VALUES
    (1, 'Some content', to_tsvector('english', 'Some content')),
    (2, NULL, NULL),
    (3, '', to_tsvector('english', ''));

SELECT id, text_content, doc_vector, doc_vector IS NULL AS is_null
FROM test_tsvector_nulls ORDER BY id;

-- COALESCE to handle NULLs
SELECT id, to_tsvector('english', COALESCE(text_content, '')) AS safe_vector
FROM test_tsvector_nulls ORDER BY id;

-- ============================================================================
-- Section 16: Real-world Use Case - Blog Search
-- ============================================================================

CREATE TABLE test_tsvector_blog (
    post_id INT PRIMARY KEY,
    title TEXT,
    author VARCHAR(100),
    content TEXT,
    tags TEXT[],
    published_date DATE,
    search_vector TSVECTOR
);

CREATE INDEX idx_blog_search ON test_tsvector_blog USING GIN (search_vector);

INSERT INTO test_tsvector_blog VALUES
    (1, 'Introduction to SQL', 'Alice Johnson',
        'SQL is a powerful language for managing databases',
        ARRAY['sql', 'database', 'tutorial'],
        '2024-01-15',
        setweight(to_tsvector('english', 'Introduction to SQL'), 'A') ||
        setweight(to_tsvector('english', 'Alice Johnson'), 'B') ||
        setweight(to_tsvector('english', 'SQL is a powerful language for managing databases'), 'C') ||
        setweight(to_tsvector('english', 'sql database tutorial'), 'D')),
    (2, 'Advanced Database Design', 'Bob Smith',
        'Learn about normalization and database schema design',
        ARRAY['database', 'design', 'normalization'],
        '2024-01-20',
        setweight(to_tsvector('english', 'Advanced Database Design'), 'A') ||
        setweight(to_tsvector('english', 'Bob Smith'), 'B') ||
        setweight(to_tsvector('english', 'Learn about normalization and database schema design'), 'C') ||
        setweight(to_tsvector('english', 'database design normalization'), 'D'));

-- Search blog posts
SELECT post_id, title, author FROM test_tsvector_blog
WHERE search_vector @@ to_tsquery('english', 'database')
ORDER BY post_id;

SELECT post_id, title FROM test_tsvector_blog
WHERE search_vector @@ to_tsquery('english', 'SQL')
ORDER BY post_id;

-- ============================================================================
-- Section 17: Real-world Use Case - Job Listings
-- ============================================================================

CREATE TABLE test_tsvector_jobs (
    job_id INT PRIMARY KEY,
    title VARCHAR(200),
    company VARCHAR(200),
    description TEXT,
    requirements TEXT,
    location VARCHAR(100),
    search_vector TSVECTOR
);

CREATE INDEX idx_job_search ON test_tsvector_jobs USING GIN (search_vector);

INSERT INTO test_tsvector_jobs VALUES
    (1, 'Senior Database Administrator', 'TechCorp Inc',
        'Manage and optimize database systems',
        'Experience with PostgreSQL, MySQL, performance tuning',
        'New York, NY',
        setweight(to_tsvector('english', 'Senior Database Administrator'), 'A') ||
        setweight(to_tsvector('english', 'TechCorp Inc'), 'B') ||
        setweight(to_tsvector('english', 'Manage and optimize database systems'), 'C') ||
        setweight(to_tsvector('english', 'Experience with PostgreSQL, MySQL, performance tuning'), 'D')),
    (2, 'Full Stack Developer', 'StartupCo',
        'Build web applications using modern frameworks',
        'JavaScript, React, Node.js, database experience',
        'San Francisco, CA',
        setweight(to_tsvector('english', 'Full Stack Developer'), 'A') ||
        setweight(to_tsvector('english', 'StartupCo'), 'B') ||
        setweight(to_tsvector('english', 'Build web applications using modern frameworks'), 'C') ||
        setweight(to_tsvector('english', 'JavaScript, React, Node.js, database experience'), 'D'));

-- Search for jobs
SELECT job_id, title, company FROM test_tsvector_jobs
WHERE search_vector @@ to_tsquery('english', 'database')
ORDER BY job_id;

SELECT job_id, title, company FROM test_tsvector_jobs
WHERE search_vector @@ to_tsquery('english', 'PostgreSQL')
ORDER BY job_id;

-- ============================================================================
-- Section 18: TSVECTOR Size and Storage
-- ============================================================================

CREATE TABLE test_tsvector_size (
    id INT PRIMARY KEY,
    short_text TEXT,
    long_text TEXT,
    short_vector TSVECTOR,
    long_vector TSVECTOR
);

INSERT INTO test_tsvector_size VALUES
    (1, 'Short document',
        'This is a much longer document with many more words that will create a larger TSVECTOR when processed',
        to_tsvector('english', 'Short document'),
        to_tsvector('english', 'This is a much longer document with many more words that will create a larger TSVECTOR when processed'));

-- Check storage size
SELECT id,
       pg_column_size(short_vector) AS short_size_bytes,
       pg_column_size(long_vector) AS long_size_bytes
FROM test_tsvector_size;

-- ============================================================================
-- Section 19: Best Practices
-- ============================================================================

CREATE TABLE test_tsvector_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_tsvector_best_practices VALUES
    (1, 'Use TSVECTOR for full-text search on documents'),
    (2, 'Create GIN indexes on TSVECTOR columns for fast search'),
    (3, 'Use to_tsvector() to convert text to TSVECTOR'),
    (4, 'Use setweight() to prioritize important fields (A > B > C > D)'),
    (5, 'Combine title, description, and tags into single TSVECTOR'),
    (6, 'Use triggers or generated columns for automatic updates'),
    (7, 'Choose appropriate language configuration (english, simple, etc)'),
    (8, 'Use @@ operator to match TSVECTOR against TSQUERY'),
    (9, 'Strip position info with strip() if positions not needed'),
    (10, 'Concatenate TSVECTOR with || operator'),
    (11, 'TSVECTOR ideal for: document search, product catalogs, blogs'),
    (12, 'length() returns number of unique lexemes'),
    (13, 'Lexemes are normalized (stemmed, lowercased)'),
    (14, 'Use COALESCE to handle NULL text fields'),
    (15, 'Consider index size vs query performance trade-offs');

SELECT id, guideline FROM test_tsvector_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_tsvector_best_practices;
DROP TABLE test_tsvector_size;
DROP TABLE test_tsvector_jobs;
DROP TABLE test_tsvector_blog;
DROP TABLE test_tsvector_nulls;
DROP TABLE test_tsvector_concat_strategies;
DROP TABLE test_tsvector_comparison;
DROP TABLE test_tsvector_stats;
DROP TABLE test_tsvector_generated;
DROP TABLE test_tsvector_auto;
DROP FUNCTION update_search_vector();
DROP TABLE test_tsvector_weighted;
DROP TABLE test_tsvector_products;
DROP TABLE test_tsvector_documents;
DROP TABLE test_tsvector_languages;
DROP TABLE test_tsvector_concat;
DROP TABLE test_tsvector_functions;
DROP TABLE test_tsvector_positions;
DROP TABLE test_tsvector_creation;
DROP TABLE test_tsvector_basic;

DROP DATABASE test_tsvector_db;

-- End of TSVECTOR data type tests
