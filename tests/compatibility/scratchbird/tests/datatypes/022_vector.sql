-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - VECTOR
-- Description: Comprehensive VECTOR data type testing
-- ============================================================================

-- VECTOR type stores fixed-dimension numerical vectors (embeddings)
-- Used for: AI/ML embeddings, similarity search, semantic search, recommendation systems
-- Supports various distance metrics: L2 (Euclidean), Cosine, Inner Product
-- Optimized with specialized indexes: IVFFLAT, HNSW

-- Create test database
CREATE DATABASE test_vector_db;
USE test_vector_db;

-- ============================================================================
-- Section 1: Basic VECTOR Storage and Creation
-- ============================================================================

CREATE TABLE test_vector_basic (
    id INT PRIMARY KEY,
    embedding VECTOR(3),  -- 3-dimensional vector
    description VARCHAR(200)
);

-- Insert vectors
INSERT INTO test_vector_basic VALUES (1, '[1, 2, 3]', 'Simple 3D vector');
INSERT INTO test_vector_basic VALUES (2, '[0, 0, 0]', 'Zero vector');
INSERT INTO test_vector_basic VALUES (3, '[1, 0, 0]', 'Unit vector X-axis');
INSERT INTO test_vector_basic VALUES (4, '[0, 1, 0]', 'Unit vector Y-axis');
INSERT INTO test_vector_basic VALUES (5, '[0, 0, 1]', 'Unit vector Z-axis');
INSERT INTO test_vector_basic VALUES (6, '[-1, -2, -3]', 'Negative values');
INSERT INTO test_vector_basic VALUES (7, '[0.5, 1.5, 2.5]', 'Decimal values');
INSERT INTO test_vector_basic VALUES (8, '[1e3, 2e-3, 3.14]', 'Scientific notation');

SELECT id, embedding, description FROM test_vector_basic ORDER BY id;

-- ============================================================================
-- Section 2: Higher-Dimensional Vectors
-- ============================================================================

CREATE TABLE test_vector_dimensions (
    id INT PRIMARY KEY,
    vec_128 VECTOR(128),  -- Common for small models
    vec_384 VECTOR(384),  -- Common for sentence transformers
    vec_1536 VECTOR(1536), -- OpenAI ada-002 dimension
    description VARCHAR(200)
);

-- Insert sample high-dimensional vectors (shortened for readability)
INSERT INTO test_vector_dimensions (id, vec_128, description) VALUES
    (1, ARRAY_FILL(1.0, ARRAY[128])::VECTOR(128), '128-dim all ones');

INSERT INTO test_vector_dimensions (id, vec_384, description) VALUES
    (2, ARRAY_FILL(0.5, ARRAY[384])::VECTOR(384), '384-dim all 0.5');

INSERT INTO test_vector_dimensions (id, vec_1536, description) VALUES
    (3, ARRAY_FILL(0.1, ARRAY[1536])::VECTOR(1536), '1536-dim all 0.1');

-- Query dimensions
SELECT id, VECTOR_DIMS(vec_128) AS dims_128 FROM test_vector_dimensions WHERE vec_128 IS NOT NULL;
SELECT id, VECTOR_DIMS(vec_384) AS dims_384 FROM test_vector_dimensions WHERE vec_384 IS NOT NULL;
SELECT id, VECTOR_DIMS(vec_1536) AS dims_1536 FROM test_vector_dimensions WHERE vec_1536 IS NOT NULL;

-- ============================================================================
-- Section 3: L2 Distance (Euclidean Distance)
-- ============================================================================

CREATE TABLE test_vector_l2_distance (
    id INT PRIMARY KEY,
    point VECTOR(3),
    label VARCHAR(100)
);

INSERT INTO test_vector_l2_distance VALUES
    (1, '[0, 0, 0]', 'Origin'),
    (2, '[1, 0, 0]', 'Unit X'),
    (3, '[0, 1, 0]', 'Unit Y'),
    (4, '[0, 0, 1]', 'Unit Z'),
    (5, '[1, 1, 0]', 'Diagonal XY'),
    (6, '[1, 1, 1]', 'Diagonal XYZ'),
    (7, '[3, 4, 0]', '3-4-5 triangle');

-- Calculate L2 distance from origin
SELECT id, label, point <-> '[0, 0, 0]' AS distance_from_origin
FROM test_vector_l2_distance ORDER BY distance_from_origin;

-- Calculate L2 distance between two specific points
SELECT
    a.id AS id1,
    a.label AS label1,
    b.id AS id2,
    b.label AS label2,
    a.point <-> b.point AS l2_distance
FROM test_vector_l2_distance a, test_vector_l2_distance b
WHERE a.id = 1 AND b.id IN (2, 3, 4, 6) ORDER BY b.id;

-- Find nearest neighbors (k-NN search)
SELECT id, label, point <-> '[1, 1, 1]' AS distance
FROM test_vector_l2_distance
ORDER BY distance LIMIT 3;

-- ============================================================================
-- Section 4: Cosine Distance (1 - Cosine Similarity)
-- ============================================================================

CREATE TABLE test_vector_cosine_distance (
    id INT PRIMARY KEY,
    embedding VECTOR(3),
    description VARCHAR(200)
);

INSERT INTO test_vector_cosine_distance VALUES
    (1, '[1, 0, 0]', 'Direction: X-axis'),
    (2, '[2, 0, 0]', 'Direction: X-axis (scaled)'),
    (3, '[0, 1, 0]', 'Direction: Y-axis'),
    (4, '[1, 1, 0]', 'Direction: 45° in XY plane'),
    (5, '[1, 1, 1]', 'Direction: Diagonal'),
    (6, '[-1, 0, 0]', 'Direction: Negative X-axis');

-- Cosine distance (1 - cosine similarity)
-- Cosine distance is 0 for identical directions, 2 for opposite directions
SELECT id, description, embedding <=> '[1, 0, 0]' AS cosine_distance
FROM test_vector_cosine_distance ORDER BY cosine_distance;

-- Note: Vectors with same direction but different magnitudes have distance 0
SELECT
    a.description AS desc1,
    b.description AS desc2,
    a.embedding <=> b.embedding AS cosine_distance
FROM test_vector_cosine_distance a, test_vector_cosine_distance b
WHERE a.id = 1 AND b.id = 2;  -- Same direction, different magnitude

-- Find most similar (smallest cosine distance)
SELECT id, description, embedding <=> '[1, 1, 1]' AS similarity
FROM test_vector_cosine_distance
ORDER BY similarity LIMIT 3;

-- ============================================================================
-- Section 5: Inner Product Distance (Negative Inner Product)
-- ============================================================================

CREATE TABLE test_vector_inner_product (
    id INT PRIMARY KEY,
    vec VECTOR(3),
    description VARCHAR(200)
);

INSERT INTO test_vector_inner_product VALUES
    (1, '[1, 0, 0]', 'Unit X'),
    (2, '[2, 0, 0]', 'Scaled X'),
    (3, '[0, 1, 0]', 'Unit Y'),
    (4, '[1, 1, 0]', 'Diagonal XY'),
    (5, '[-1, 0, 0]', 'Opposite X');

-- Inner product distance (negative dot product)
-- Lower values mean higher similarity (more aligned, same direction)
SELECT id, description, vec <#> '[1, 0, 0]' AS inner_product_distance
FROM test_vector_inner_product ORDER BY inner_product_distance;

-- Compare: same direction different magnitude
SELECT
    a.description AS desc1,
    b.description AS desc2,
    a.vec <#> b.vec AS inner_product_distance
FROM test_vector_inner_product a, test_vector_inner_product b
WHERE a.id = 1 AND b.id = 2;

-- ============================================================================
-- Section 6: Vector Operations
-- ============================================================================

CREATE TABLE test_vector_operations (
    id INT PRIMARY KEY,
    vec1 VECTOR(3),
    vec2 VECTOR(3)
);

INSERT INTO test_vector_operations VALUES
    (1, '[1, 2, 3]', '[4, 5, 6]'),
    (2, '[1, 0, 0]', '[0, 1, 0]'),
    (3, '[2, 2, 2]', '[1, 1, 1]');

-- Vector addition
SELECT id, vec1 + vec2 AS sum FROM test_vector_operations ORDER BY id;

-- Vector subtraction
SELECT id, vec1 - vec2 AS difference FROM test_vector_operations ORDER BY id;

-- Scalar multiplication
SELECT id, vec1 * 2 AS scaled FROM test_vector_operations ORDER BY id;

-- Vector norm (L2 norm / magnitude)
SELECT id, VECTOR_NORM(vec1) AS magnitude FROM test_vector_operations ORDER BY id;

-- Dot product
SELECT id, vec1 <#> vec2 AS dot_product FROM test_vector_operations ORDER BY id;

-- ============================================================================
-- Section 7: Vector Indexing - IVFFLAT
-- ============================================================================

CREATE TABLE test_vector_ivfflat (
    id INT PRIMARY KEY,
    embedding VECTOR(128)
);

-- IVFFLAT: Inverted File Flat index
-- Good for: Medium-sized datasets (10K-1M vectors)
-- Trade-off: Faster search, some recall loss
CREATE INDEX idx_embedding_ivfflat ON test_vector_ivfflat
USING IVFFLAT (embedding VECTOR_L2_OPS)
WITH (lists = 100);  -- Number of clusters

-- Insert sample data (simplified for test)
INSERT INTO test_vector_ivfflat
SELECT i, ARRAY_FILL(random(), ARRAY[128])::VECTOR(128)
FROM generate_series(1, 1000) i;

-- Query using index (approximate nearest neighbor)
SELECT id FROM test_vector_ivfflat
ORDER BY embedding <-> ARRAY_FILL(0.5, ARRAY[128])::VECTOR(128)
LIMIT 10;

-- ============================================================================
-- Section 8: Vector Indexing - HNSW
-- ============================================================================

CREATE TABLE test_vector_hnsw (
    id INT PRIMARY KEY,
    embedding VECTOR(128)
);

-- HNSW: Hierarchical Navigable Small World
-- Good for: Large datasets (100K-10M+ vectors)
-- Trade-off: Better recall than IVFFLAT, more memory
CREATE INDEX idx_embedding_hnsw ON test_vector_hnsw
USING HNSW (embedding VECTOR_L2_OPS)
WITH (m = 16, ef_construction = 64);
-- m: number of connections per layer
-- ef_construction: controls index build quality/speed

-- Insert sample data
INSERT INTO test_vector_hnsw
SELECT i, ARRAY_FILL(random(), ARRAY[128])::VECTOR(128)
FROM generate_series(1, 1000) i;

-- Query using HNSW index
SELECT id FROM test_vector_hnsw
ORDER BY embedding <-> ARRAY_FILL(0.5, ARRAY[128])::VECTOR(128)
LIMIT 10;

-- ============================================================================
-- Section 9: Different Distance Metrics with Indexes
-- ============================================================================

CREATE TABLE test_vector_distance_metrics (
    id INT PRIMARY KEY,
    embedding VECTOR(384)
);

-- Index for L2 distance
CREATE INDEX idx_vec_l2 ON test_vector_distance_metrics
USING HNSW (embedding VECTOR_L2_OPS);

-- Index for cosine distance
CREATE INDEX idx_vec_cosine ON test_vector_distance_metrics
USING HNSW (embedding VECTOR_COSINE_OPS);

-- Index for inner product
CREATE INDEX idx_vec_ip ON test_vector_distance_metrics
USING HNSW (embedding VECTOR_IP_OPS);

-- Insert sample embeddings
INSERT INTO test_vector_distance_metrics
SELECT i, ARRAY_FILL(random(), ARRAY[384])::VECTOR(384)
FROM generate_series(1, 500) i;

-- Query with different metrics
SELECT id FROM test_vector_distance_metrics
ORDER BY embedding <-> ARRAY_FILL(0.5, ARRAY[384])::VECTOR(384)
LIMIT 5;  -- L2 distance

SELECT id FROM test_vector_distance_metrics
ORDER BY embedding <=> ARRAY_FILL(0.5, ARRAY[384])::VECTOR(384)
LIMIT 5;  -- Cosine distance

SELECT id FROM test_vector_distance_metrics
ORDER BY embedding <#> ARRAY_FILL(0.5, ARRAY[384])::VECTOR(384)
LIMIT 5;  -- Inner product

-- ============================================================================
-- Section 10: Real-world Use Case - Document Embeddings
-- ============================================================================

CREATE TABLE test_vector_documents (
    id INT PRIMARY KEY,
    title VARCHAR(200),
    content TEXT,
    embedding VECTOR(384),  -- From sentence-transformer model
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_doc_embedding ON test_vector_documents
USING HNSW (embedding VECTOR_COSINE_OPS);

-- Insert sample documents (embeddings would come from ML model)
INSERT INTO test_vector_documents (id, title, content, embedding) VALUES
    (1, 'Introduction to Databases', 'Databases store and organize data...', ARRAY_FILL(0.1, ARRAY[384])::VECTOR(384)),
    (2, 'SQL Query Basics', 'SQL is used to query databases...', ARRAY_FILL(0.15, ARRAY[384])::VECTOR(384)),
    (3, 'Python Programming', 'Python is a versatile language...', ARRAY_FILL(0.8, ARRAY[384])::VECTOR(384)),
    (4, 'Machine Learning Intro', 'ML algorithms learn from data...', ARRAY_FILL(0.85, ARRAY[384])::VECTOR(384));

-- Semantic search: find documents similar to query embedding
-- (In practice, query would be embedded by same model)
SELECT id, title,
       embedding <=> ARRAY_FILL(0.12, ARRAY[384])::VECTOR(384) AS similarity_score
FROM test_vector_documents
ORDER BY similarity_score
LIMIT 3;

-- ============================================================================
-- Section 11: Real-world Use Case - Product Recommendations
-- ============================================================================

CREATE TABLE test_vector_products (
    id INT PRIMARY KEY,
    product_name VARCHAR(200),
    category VARCHAR(100),
    embedding VECTOR(128),  -- Product feature embedding
    price DECIMAL(10, 2)
);

CREATE INDEX idx_product_embedding ON test_vector_products
USING HNSW (embedding VECTOR_L2_OPS);

INSERT INTO test_vector_products (id, product_name, category, embedding, price) VALUES
    (1, 'Laptop Pro 15', 'Electronics', ARRAY_FILL(0.2, ARRAY[128])::VECTOR(128), 1299.99),
    (2, 'Wireless Mouse', 'Electronics', ARRAY_FILL(0.25, ARRAY[128])::VECTOR(128), 29.99),
    (3, 'Desk Lamp', 'Home', ARRAY_FILL(0.7, ARRAY[128])::VECTOR(128), 49.99),
    (4, 'Office Chair', 'Furniture', ARRAY_FILL(0.9, ARRAY[128])::VECTOR(128), 199.99),
    (5, 'USB-C Hub', 'Electronics', ARRAY_FILL(0.22, ARRAY[128])::VECTOR(128), 39.99);

-- Find similar products (recommendations)
-- Given user viewed product 1 (Laptop), find similar products
SELECT id, product_name, category,
       embedding <-> (SELECT embedding FROM test_vector_products WHERE id = 1) AS similarity
FROM test_vector_products
WHERE id != 1  -- Exclude the source product
ORDER BY similarity
LIMIT 3;

-- ============================================================================
-- Section 12: Real-world Use Case - Image Search
-- ============================================================================

CREATE TABLE test_vector_images (
    id INT PRIMARY KEY,
    image_url VARCHAR(500),
    alt_text VARCHAR(200),
    embedding VECTOR(512),  -- From image model like CLIP
    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_image_embedding ON test_vector_images
USING HNSW (embedding VECTOR_COSINE_OPS);

INSERT INTO test_vector_images (id, image_url, alt_text, embedding) VALUES
    (1, '/images/cat1.jpg', 'Orange cat sleeping', ARRAY_FILL(0.3, ARRAY[512])::VECTOR(512)),
    (2, '/images/cat2.jpg', 'Black cat playing', ARRAY_FILL(0.32, ARRAY[512])::VECTOR(512)),
    (3, '/images/dog1.jpg', 'Golden retriever running', ARRAY_FILL(0.6, ARRAY[512])::VECTOR(512)),
    (4, '/images/dog2.jpg', 'Puppy sleeping', ARRAY_FILL(0.65, ARRAY[512])::VECTOR(512)),
    (5, '/images/bird1.jpg', 'Blue bird on branch', ARRAY_FILL(0.9, ARRAY[512])::VECTOR(512));

-- Find visually similar images
SELECT id, alt_text,
       embedding <=> (SELECT embedding FROM test_vector_images WHERE id = 1) AS visual_similarity
FROM test_vector_images
WHERE id != 1
ORDER BY visual_similarity
LIMIT 3;

-- ============================================================================
-- Section 13: Real-world Use Case - User/Item Embeddings
-- ============================================================================

CREATE TABLE test_vector_user_embeddings (
    user_id INT PRIMARY KEY,
    username VARCHAR(100),
    preference_embedding VECTOR(64),  -- User preference vector
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_vector_item_embeddings (
    item_id INT PRIMARY KEY,
    item_name VARCHAR(200),
    item_embedding VECTOR(64),
    category VARCHAR(100)
);

CREATE INDEX idx_user_pref ON test_vector_user_embeddings
USING HNSW (preference_embedding VECTOR_COSINE_OPS);

CREATE INDEX idx_item_emb ON test_vector_item_embeddings
USING HNSW (item_embedding VECTOR_COSINE_OPS);

-- Insert users
INSERT INTO test_vector_user_embeddings (user_id, username, preference_embedding) VALUES
    (1, 'alice', ARRAY_FILL(0.1, ARRAY[64])::VECTOR(64)),
    (2, 'bob', ARRAY_FILL(0.5, ARRAY[64])::VECTOR(64)),
    (3, 'charlie', ARRAY_FILL(0.9, ARRAY[64])::VECTOR(64));

-- Insert items
INSERT INTO test_vector_item_embeddings (item_id, item_name, item_embedding, category) VALUES
    (1, 'Sci-Fi Movie', ARRAY_FILL(0.12, ARRAY[64])::VECTOR(64), 'Entertainment'),
    (2, 'Mystery Novel', ARRAY_FILL(0.15, ARRAY[64])::VECTOR(64), 'Books'),
    (3, 'Cooking Show', ARRAY_FILL(0.85, ARRAY[64])::VECTOR(64), 'Entertainment'),
    (4, 'Tech Podcast', ARRAY_FILL(0.48, ARRAY[64])::VECTOR(64), 'Audio');

-- Recommend items for a user based on embedding similarity
SELECT i.item_id, i.item_name, i.category,
       i.item_embedding <=> u.preference_embedding AS match_score
FROM test_vector_user_embeddings u
CROSS JOIN test_vector_item_embeddings i
WHERE u.user_id = 1
ORDER BY match_score
LIMIT 3;

-- ============================================================================
-- Section 14: Vector Normalization
-- ============================================================================

CREATE TABLE test_vector_normalization (
    id INT PRIMARY KEY,
    original VECTOR(3),
    normalized VECTOR(3)
);

-- Vectors of different magnitudes
INSERT INTO test_vector_normalization (id, original) VALUES
    (1, '[3, 4, 0]'),
    (2, '[1, 1, 1]'),
    (3, '[10, 20, 30]');

-- Normalize vectors to unit length
UPDATE test_vector_normalization
SET normalized = original / VECTOR_NORM(original);

SELECT id,
       original,
       normalized,
       VECTOR_NORM(original) AS original_norm,
       VECTOR_NORM(normalized) AS normalized_norm
FROM test_vector_normalization ORDER BY id;

-- Use case: Normalized vectors for cosine similarity via dot product
-- For unit vectors: cosine_similarity = dot_product

-- ============================================================================
-- Section 15: Filtering with Vector Search
-- ============================================================================

CREATE TABLE test_vector_filtered_search (
    id INT PRIMARY KEY,
    product_name VARCHAR(200),
    category VARCHAR(100),
    price DECIMAL(10, 2),
    in_stock BOOLEAN,
    embedding VECTOR(128)
);

CREATE INDEX idx_filtered_vec ON test_vector_filtered_search
USING HNSW (embedding VECTOR_L2_OPS);

INSERT INTO test_vector_filtered_search VALUES
    (1, 'Laptop A', 'Electronics', 999.99, true, ARRAY_FILL(0.1, ARRAY[128])::VECTOR(128)),
    (2, 'Laptop B', 'Electronics', 1299.99, false, ARRAY_FILL(0.12, ARRAY[128])::VECTOR(128)),
    (3, 'Monitor', 'Electronics', 399.99, true, ARRAY_FILL(0.15, ARRAY[128])::VECTOR(128)),
    (4, 'Desk', 'Furniture', 299.99, true, ARRAY_FILL(0.8, ARRAY[128])::VECTOR(128)),
    (5, 'Chair', 'Furniture', 199.99, true, ARRAY_FILL(0.82, ARRAY[128])::VECTOR(128));

-- Vector search with filters (hybrid search)
-- Find similar products that are in stock and under $500
SELECT id, product_name, price,
       embedding <-> ARRAY_FILL(0.11, ARRAY[128])::VECTOR(128) AS similarity
FROM test_vector_filtered_search
WHERE in_stock = true AND price < 500
ORDER BY similarity
LIMIT 3;

-- ============================================================================
-- Section 16: Batch Vector Similarity
-- ============================================================================

CREATE TABLE test_vector_batch_similarity (
    id INT PRIMARY KEY,
    embedding VECTOR(3),
    label VARCHAR(100)
);

INSERT INTO test_vector_batch_similarity VALUES
    (1, '[1, 0, 0]', 'Point A'),
    (2, '[0, 1, 0]', 'Point B'),
    (3, '[0, 0, 1]', 'Point C'),
    (4, '[1, 1, 1]', 'Point D');

-- Compute pairwise distances
SELECT
    a.id AS id1,
    a.label AS label1,
    b.id AS id2,
    b.label AS label2,
    a.embedding <-> b.embedding AS l2_distance
FROM test_vector_batch_similarity a
CROSS JOIN test_vector_batch_similarity b
WHERE a.id < b.id  -- Avoid duplicates and self-comparison
ORDER BY a.id, b.id;

-- ============================================================================
-- Section 17: NULL Handling
-- ============================================================================

CREATE TABLE test_vector_nulls (
    id INT PRIMARY KEY,
    embedding VECTOR(3)
);

INSERT INTO test_vector_nulls VALUES
    (1, '[1, 2, 3]'),
    (2, NULL),
    (3, '[4, 5, 6]');

SELECT id, embedding, embedding IS NULL AS is_null FROM test_vector_nulls ORDER BY id;

-- Distance with NULL returns NULL
SELECT id, embedding <-> '[1, 1, 1]' AS distance FROM test_vector_nulls ORDER BY id;

-- ============================================================================
-- Section 18: Vector Dimension Mismatch Handling
-- ============================================================================

CREATE TABLE test_vector_dim_mismatch (
    id INT PRIMARY KEY,
    vec3 VECTOR(3),
    vec5 VECTOR(5)
);

INSERT INTO test_vector_dim_mismatch VALUES
    (1, '[1, 2, 3]', '[1, 2, 3, 4, 5]');

-- These operations would fail (dimension mismatch):
-- SELECT vec3 <-> '[1, 2, 3, 4]' FROM test_vector_dim_mismatch;  -- ERROR
-- SELECT vec3 + vec5 FROM test_vector_dim_mismatch;  -- ERROR

-- Must match dimensions exactly
SELECT id, vec3 <-> '[1, 1, 1]' AS dist3 FROM test_vector_dim_mismatch;
SELECT id, vec5 <-> '[1, 1, 1, 1, 1]' AS dist5 FROM test_vector_dim_mismatch;

-- ============================================================================
-- Section 19: Performance Characteristics
-- ============================================================================

CREATE TABLE test_vector_performance (
    id INT PRIMARY KEY,
    notes TEXT
);

INSERT INTO test_vector_performance VALUES
    (1, 'Exact k-NN (no index): O(n) - scans all vectors'),
    (2, 'IVFFLAT index: ~O(sqrt(n)) - clusters data, searches subset'),
    (3, 'HNSW index: ~O(log(n)) - graph-based, best for large datasets'),
    (4, 'L2 distance: fastest computation (sum of squares)'),
    (5, 'Cosine distance: requires normalization (slightly slower)'),
    (6, 'Index build time: HNSW > IVFFLAT > no index'),
    (7, 'Index memory: HNSW > IVFFLAT > no index'),
    (8, 'Query recall: Exact > HNSW > IVFFLAT'),
    (9, 'Best for 1K-10K vectors: Exact search or IVFFLAT'),
    (10, 'Best for 100K+ vectors: HNSW with tuned parameters');

SELECT id, notes FROM test_vector_performance ORDER BY id;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_vector_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_vector_best_practices VALUES
    (1, 'Choose dimension based on your ML model (128, 384, 512, 1536 common)'),
    (2, 'Normalize vectors for cosine similarity (makes it equivalent to dot product)'),
    (3, 'Use HNSW for large datasets (100K+ vectors)'),
    (4, 'Use IVFFLAT for medium datasets (10K-1M vectors)'),
    (5, 'Tune index parameters based on recall/speed requirements'),
    (6, 'Use L2 distance for spatial data, cosine for semantic similarity'),
    (7, 'Combine vector search with filters for hybrid search'),
    (8, 'Store embeddings alongside original data for explainability'),
    (9, 'Batch insert vectors for better index building performance'),
    (10, 'Monitor index build time and query latency in production'),
    (11, 'Use VECTOR for: semantic search, recommendations, image search, anomaly detection'),
    (12, 'Consider dimensionality reduction (PCA) for very high dimensions'),
    (13, 'Keep embeddings versioned if ML model changes'),
    (14, 'Use partial indexes for filtering subsets'),
    (15, 'VECTOR is ideal for AI/ML applications with embedding-based search');

SELECT id, guideline FROM test_vector_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_vector_best_practices;
DROP TABLE test_vector_performance;
DROP TABLE test_vector_dim_mismatch;
DROP TABLE test_vector_nulls;
DROP TABLE test_vector_batch_similarity;
DROP TABLE test_vector_filtered_search;
DROP TABLE test_vector_normalization;
DROP TABLE test_vector_item_embeddings;
DROP TABLE test_vector_user_embeddings;
DROP TABLE test_vector_images;
DROP TABLE test_vector_products;
DROP TABLE test_vector_documents;
DROP TABLE test_vector_distance_metrics;
DROP TABLE test_vector_hnsw;
DROP TABLE test_vector_ivfflat;
DROP TABLE test_vector_operations;
DROP TABLE test_vector_inner_product;
DROP TABLE test_vector_cosine_distance;
DROP TABLE test_vector_l2_distance;
DROP TABLE test_vector_dimensions;
DROP TABLE test_vector_basic;

DROP DATABASE test_vector_db;

-- End of VECTOR data type tests
