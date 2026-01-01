-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - XML
-- Description: Comprehensive XML data type testing
-- ============================================================================

-- XML type stores well-formed XML documents and fragments
-- Supports XPath queries, XSLT transformations, and XML validation
-- Used for document storage, configuration files, and data interchange

-- Create test database
CREATE DATABASE test_xml_db;
USE test_xml_db;

-- ============================================================================
-- Section 1: Basic XML Storage
-- ============================================================================

CREATE TABLE test_xml_basic (
    id INT PRIMARY KEY,
    xml_data XML,
    description VARCHAR(200)
);

-- Simple XML elements
INSERT INTO test_xml_basic VALUES (1, '<root>Simple content</root>', 'Simple element');
INSERT INTO test_xml_basic VALUES (2, '<person><name>John</name><age>30</age></person>', 'Nested elements');
INSERT INTO test_xml_basic VALUES (3, '<item id="123" type="product"/>', 'Element with attributes');
INSERT INTO test_xml_basic VALUES (4, '<list><item>A</item><item>B</item><item>C</item></list>', 'Multiple child elements');
INSERT INTO test_xml_basic VALUES (5, '<root xmlns="http://example.com/ns">Namespaced</root>', 'With namespace');
INSERT INTO test_xml_basic VALUES (6, '<root><![CDATA[Special <characters> & symbols]]></root>', 'CDATA section');
INSERT INTO test_xml_basic VALUES (7, '<empty/>', 'Empty element');
INSERT INTO test_xml_basic VALUES (8, '<root><!-- This is a comment --><data>Content</data></root>', 'With comment');

SELECT id, xml_data, description FROM test_xml_basic ORDER BY id;

-- ============================================================================
-- Section 2: XML Well-formedness Validation
-- ============================================================================

CREATE TABLE test_xml_validation (
    id INT PRIMARY KEY,
    xml_string TEXT,
    description VARCHAR(200)
);

-- Valid XML
INSERT INTO test_xml_validation VALUES (1, '<root>Valid</root>', 'Well-formed');
INSERT INTO test_xml_validation VALUES (2, '<a><b><c>Nested</c></b></a>', 'Properly nested');
INSERT INTO test_xml_validation VALUES (3, '<element attr="value">Text</element>', 'With attribute');

-- These would fail validation if inserted as XML type:
INSERT INTO test_xml_validation VALUES (4, '<root>Unclosed', 'Missing closing tag - invalid');
INSERT INTO test_xml_validation VALUES (5, '<a><b></a></b>', 'Improperly nested - invalid');
INSERT INTO test_xml_validation VALUES (6, '<element attribute=value>', 'Unquoted attribute - invalid');

-- Test validation function
SELECT id, description, XMLPARSE(CONTENT xml_string) AS parsed
FROM test_xml_validation WHERE id <= 3 ORDER BY id;

-- ============================================================================
-- Section 3: XPath Queries - Element Extraction
-- ============================================================================

CREATE TABLE test_xml_xpath_elements (
    id INT PRIMARY KEY,
    document XML
);

INSERT INTO test_xml_xpath_elements VALUES
    (1, '<book><title>Database Systems</title><author>Smith</author><year>2023</year></book>'),
    (2, '<book><title>XML Processing</title><author>Jones</author><year>2022</year></book>'),
    (3, '<library><book><title>First Book</title></book><book><title>Second Book</title></book></library>');

-- Extract single element value
SELECT id, XPATH('//title/text()', document) AS title FROM test_xml_xpath_elements WHERE id = 1;
SELECT id, XPATH('//author/text()', document) AS author FROM test_xml_xpath_elements WHERE id = 1;
SELECT id, XPATH('//year/text()', document) AS year FROM test_xml_xpath_elements WHERE id = 2;

-- Extract multiple elements
SELECT id, XPATH('//title/text()', document) AS titles FROM test_xml_xpath_elements WHERE id = 3;

-- ============================================================================
-- Section 4: XPath Queries - Attribute Extraction
-- ============================================================================

CREATE TABLE test_xml_xpath_attributes (
    id INT PRIMARY KEY,
    product XML
);

INSERT INTO test_xml_xpath_attributes VALUES
    (1, '<product id="101" category="electronics" price="599.99"><name>Laptop</name></product>'),
    (2, '<product id="102" category="books" price="29.99"><name>XML Guide</name></product>'),
    (3, '<item sku="ABC123" warehouse="WH1" quantity="50"><description>Widget</description></item>');

-- Extract attributes
SELECT id, XPATH('/product/@id', product) AS product_id FROM test_xml_xpath_attributes WHERE id = 1;
SELECT id, XPATH('/product/@category', product) AS category FROM test_xml_xpath_attributes WHERE id = 1;
SELECT id, XPATH('/product/@price', product) AS price FROM test_xml_xpath_attributes WHERE id = 2;

-- Extract multiple attributes
SELECT id,
       XPATH('/product/@id', product) AS id_attr,
       XPATH('/product/@category', product) AS category_attr
FROM test_xml_xpath_attributes WHERE id <= 2 ORDER BY id;

-- ============================================================================
-- Section 5: XPath Predicates and Filtering
-- ============================================================================

CREATE TABLE test_xml_xpath_predicates (
    id INT PRIMARY KEY,
    catalog XML
);

INSERT INTO test_xml_xpath_predicates VALUES
    (1, '<catalog>
           <product id="1" price="100">Item A</product>
           <product id="2" price="200">Item B</product>
           <product id="3" price="50">Item C</product>
         </catalog>'),
    (2, '<catalog>
           <product id="10" price="150">Item X</product>
           <product id="20" price="75">Item Y</product>
         </catalog>');

-- Filter by attribute value
SELECT id, XPATH('//product[@id="1"]/text()', catalog) AS product FROM test_xml_xpath_predicates WHERE id = 1;

-- Filter by attribute comparison
SELECT id, XPATH('//product[@price>100]/text()', catalog) AS expensive_products FROM test_xml_xpath_predicates WHERE id = 1;

-- Position-based selection
SELECT id, XPATH('//product[1]/text()', catalog) AS first_product FROM test_xml_xpath_predicates ORDER BY id;
SELECT id, XPATH('//product[last()]/text()', catalog) AS last_product FROM test_xml_xpath_predicates ORDER BY id;

-- ============================================================================
-- Section 6: XML Construction Functions
-- ============================================================================

-- XMLELEMENT: Create XML element
SELECT XMLELEMENT(NAME user, 'Alice') AS simple_element;
SELECT XMLELEMENT(NAME person,
                  XMLELEMENT(NAME name, 'John'),
                  XMLELEMENT(NAME age, 30)) AS nested_elements;

-- XMLATTRIBUTES: Add attributes to elements
SELECT XMLELEMENT(NAME product,
                  XMLATTRIBUTES(123 AS id, 'electronics' AS category),
                  'Laptop') AS element_with_attrs;

-- XMLFOREST: Create multiple elements from columns
CREATE TABLE test_xml_construction (
    id INT PRIMARY KEY,
    first_name VARCHAR(50),
    last_name VARCHAR(50),
    age INT
);

INSERT INTO test_xml_construction VALUES
    (1, 'Alice', 'Johnson', 28),
    (2, 'Bob', 'Smith', 35);

SELECT id,
       XMLELEMENT(NAME person,
                  XMLATTRIBUTES(id AS person_id),
                  XMLFOREST(first_name, last_name, age)) AS person_xml
FROM test_xml_construction ORDER BY id;

-- XMLCONCAT: Concatenate XML values
SELECT XMLCONCAT(XMLELEMENT(NAME first, 'A'),
                 XMLELEMENT(NAME second, 'B')) AS concatenated;

-- ============================================================================
-- Section 7: XML Aggregation
-- ============================================================================

CREATE TABLE test_xml_aggregation (
    id INT,
    category VARCHAR(50),
    item_name VARCHAR(100)
);

INSERT INTO test_xml_aggregation VALUES
    (1, 'fruits', 'apple'),
    (2, 'fruits', 'banana'),
    (3, 'vegetables', 'carrot'),
    (4, 'vegetables', 'lettuce');

-- XMLAGG: Aggregate rows into XML
SELECT category,
       XMLAGG(XMLELEMENT(NAME item, item_name)) AS items_xml
FROM test_xml_aggregation
GROUP BY category ORDER BY category;

-- Create complete XML document from table
SELECT XMLELEMENT(NAME catalog,
                  XMLAGG(XMLELEMENT(NAME product,
                                    XMLATTRIBUTES(id AS product_id),
                                    item_name))) AS catalog_xml
FROM test_xml_aggregation;

-- ============================================================================
-- Section 8: XML to Table Conversion
-- ============================================================================

CREATE TABLE test_xml_to_table (
    id INT PRIMARY KEY,
    products_xml XML
);

INSERT INTO test_xml_to_table VALUES
    (1, '<products>
           <product><id>101</id><name>Laptop</name><price>999.99</price></product>
           <product><id>102</id><name>Mouse</name><price>29.99</price></product>
           <product><id>103</id><name>Keyboard</name><price>79.99</price></product>
         </products>');

-- Extract to rows using XPATH
SELECT
    XPATH('//product/id/text()', products_xml) AS product_ids,
    XPATH('//product/name/text()', products_xml) AS product_names,
    XPATH('//product/price/text()', products_xml) AS prices
FROM test_xml_to_table WHERE id = 1;

-- XMLTABLE: Convert XML to relational table
SELECT product.*
FROM test_xml_to_table,
     XMLTABLE('/products/product'
              PASSING products_xml
              COLUMNS id INT PATH 'id',
                      name VARCHAR(100) PATH 'name',
                      price DECIMAL(10,2) PATH 'price') AS product
WHERE test_xml_to_table.id = 1;

-- ============================================================================
-- Section 9: XML Namespaces
-- ============================================================================

CREATE TABLE test_xml_namespaces (
    id INT PRIMARY KEY,
    doc XML
);

INSERT INTO test_xml_namespaces VALUES
    (1, '<root xmlns="http://example.com/default">
           <element>Content</element>
         </root>'),
    (2, '<doc xmlns:custom="http://example.com/custom">
           <custom:item>Value</custom:item>
         </doc>'),
    (3, '<root xmlns:a="http://a.com" xmlns:b="http://b.com">
           <a:element>A content</a:element>
           <b:element>B content</b:element>
         </root>');

-- Query with default namespace
SELECT id, XPATH('//*[local-name()="element"]/text()',
                 doc,
                 ARRAY[ARRAY['x', 'http://example.com/default']]) AS content
FROM test_xml_namespaces WHERE id = 1;

-- Query with custom namespace
SELECT id, XPATH('//c:item/text()',
                 doc,
                 ARRAY[ARRAY['c', 'http://example.com/custom']]) AS custom_content
FROM test_xml_namespaces WHERE id = 2;

-- ============================================================================
-- Section 10: XML Modification
-- ============================================================================

CREATE TABLE test_xml_modification (
    id INT PRIMARY KEY,
    data XML
);

INSERT INTO test_xml_modification VALUES
    (1, '<person><name>Alice</name><age>28</age></person>');

-- XMLCONCAT to add elements
UPDATE test_xml_modification
SET data = XMLCONCAT(data, XMLELEMENT(NAME email, 'alice@example.com'))
WHERE id = 1;

SELECT data FROM test_xml_modification WHERE id = 1;

-- Replace entire document
UPDATE test_xml_modification
SET data = '<person><name>Alice Updated</name><age>29</age><city>NYC</city></person>'
WHERE id = 1;

SELECT data FROM test_xml_modification WHERE id = 1;

-- ============================================================================
-- Section 11: XML Document Structure - Real-world Example
-- ============================================================================

CREATE TABLE test_xml_documents (
    id INT PRIMARY KEY,
    doc_type VARCHAR(50),
    content XML,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Invoice document
INSERT INTO test_xml_documents (id, doc_type, content) VALUES
    (1, 'invoice', '<invoice id="INV-001" date="2024-01-15">
                      <customer>
                        <name>Acme Corp</name>
                        <address>123 Main St, NYC</address>
                      </customer>
                      <items>
                        <item sku="ABC123" quantity="10" price="99.99"/>
                        <item sku="DEF456" quantity="5" price="149.99"/>
                      </items>
                      <total>1749.85</total>
                    </invoice>');

-- Configuration document
INSERT INTO test_xml_documents (id, doc_type, content) VALUES
    (2, 'config', '<configuration version="1.0">
                     <database>
                       <host>localhost</host>
                       <port>5432</port>
                       <name>mydb</name>
                     </database>
                     <cache enabled="true">
                       <ttl>3600</ttl>
                       <maxSize>1000</maxSize>
                     </cache>
                   </configuration>');

-- Query invoice details
SELECT id,
       XPATH('//invoice/@id', content) AS invoice_id,
       XPATH('//customer/name/text()', content) AS customer_name,
       XPATH('//total/text()', content) AS total
FROM test_xml_documents WHERE doc_type = 'invoice';

-- Query configuration
SELECT id,
       XPATH('//database/host/text()', content) AS db_host,
       XPATH('//database/port/text()', content) AS db_port,
       XPATH('//cache/@enabled', content) AS cache_enabled
FROM test_xml_documents WHERE doc_type = 'config';

-- ============================================================================
-- Section 12: RSS/Atom Feed Example
-- ============================================================================

CREATE TABLE test_xml_feeds (
    id INT PRIMARY KEY,
    feed_type VARCHAR(20),
    feed_data XML
);

INSERT INTO test_xml_feeds VALUES
    (1, 'rss', '<rss version="2.0">
                  <channel>
                    <title>Tech News</title>
                    <link>http://example.com</link>
                    <item>
                      <title>Breaking: New Database Released</title>
                      <link>http://example.com/article1</link>
                      <pubDate>2024-01-15T10:00:00Z</pubDate>
                    </item>
                    <item>
                      <title>Tutorial: Advanced SQL</title>
                      <link>http://example.com/article2</link>
                      <pubDate>2024-01-14T15:30:00Z</pubDate>
                    </item>
                  </channel>
                </rss>');

-- Extract feed information
SELECT id,
       XPATH('//channel/title/text()', feed_data) AS feed_title,
       XPATH('count(//item)', feed_data) AS item_count
FROM test_xml_feeds WHERE id = 1;

-- Extract all article titles
SELECT id, XPATH('//item/title/text()', feed_data) AS article_titles
FROM test_xml_feeds WHERE id = 1;

-- ============================================================================
-- Section 13: SVG Document Storage
-- ============================================================================

CREATE TABLE test_xml_svg (
    id INT PRIMARY KEY,
    image_name VARCHAR(100),
    svg_data XML
);

INSERT INTO test_xml_svg VALUES
    (1, 'circle', '<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
                     <circle cx="50" cy="50" r="40" fill="blue"/>
                   </svg>'),
    (2, 'rectangle', '<svg xmlns="http://www.w3.org/2000/svg" width="200" height="100">
                        <rect x="10" y="10" width="180" height="80" fill="red"/>
                      </svg>');

-- Query SVG attributes
SELECT id, image_name,
       XPATH('//*[local-name()="svg"]/@width',
             svg_data,
             ARRAY[ARRAY['svg', 'http://www.w3.org/2000/svg']]) AS width,
       XPATH('//*[local-name()="svg"]/@height',
             svg_data,
             ARRAY[ARRAY['svg', 'http://www.w3.org/2000/svg']]) AS height
FROM test_xml_svg ORDER BY id;

-- ============================================================================
-- Section 14: XML Comparison
-- ============================================================================

CREATE TABLE test_xml_comparison (
    id INT PRIMARY KEY,
    xml_value XML
);

INSERT INTO test_xml_comparison VALUES
    (1, '<root><a>1</a><b>2</b></root>'),
    (2, '<root><b>2</b><a>1</a></root>'),  -- Different order
    (3, '<root>  <a>1</a>  <b>2</b>  </root>'),  -- Different whitespace
    (4, '<root><a>1</a><b>2</b></root>');  -- Identical to row 1

-- XML equality (structure-based, ignores formatting differences)
SELECT id FROM test_xml_comparison
WHERE xml_value = '<root><a>1</a><b>2</b></root>' ORDER BY id;

-- Note: XML comparison is based on document structure, not textual representation
-- Whitespace and attribute order may not affect equality depending on implementation

-- ============================================================================
-- Section 15: XML Indexing
-- ============================================================================

CREATE TABLE test_xml_indexing (
    id INT PRIMARY KEY,
    metadata XML
);

-- Index on extracted XML path (for faster queries)
CREATE INDEX idx_xml_id ON test_xml_indexing ((XPATH('//id/text()', metadata)::TEXT));

INSERT INTO test_xml_indexing VALUES
    (1, '<record><id>100</id><type>user</type></record>'),
    (2, '<record><id>200</id><type>product</type></record>'),
    (3, '<record><id>300</id><type>order</type></record>');

-- Query using indexed path
SELECT id, metadata FROM test_xml_indexing
WHERE XPATH('//id/text()', metadata)::TEXT = '100';

-- ============================================================================
-- Section 16: NULL Handling
-- ============================================================================

CREATE TABLE test_xml_nulls (
    id INT PRIMARY KEY,
    xml_data XML
);

INSERT INTO test_xml_nulls VALUES
    (1, '<root>Content</root>'),
    (2, NULL),
    (3, '<empty/>');

SELECT id, xml_data, xml_data IS NULL AS is_null FROM test_xml_nulls ORDER BY id;

-- XPath on NULL returns NULL
SELECT id, XPATH('//root/text()', xml_data) AS extracted FROM test_xml_nulls ORDER BY id;

-- ============================================================================
-- Section 17: XML Functions - XMLEXISTS, XMLCAST
-- ============================================================================

CREATE TABLE test_xml_functions (
    id INT PRIMARY KEY,
    document XML
);

INSERT INTO test_xml_functions VALUES
    (1, '<product><name>Laptop</name><price>999.99</price></product>'),
    (2, '<product><name>Mouse</name></product>'),
    (3, '<order><id>123</id><total>499.99</total></order>');

-- XMLEXISTS: Check if XPath matches
SELECT id FROM test_xml_functions
WHERE XMLEXISTS('//price' PASSING document) ORDER BY id;

SELECT id FROM test_xml_functions
WHERE XMLEXISTS('//total' PASSING document) ORDER BY id;

-- XMLCAST: Cast XML to other types
SELECT id,
       XMLCAST(XPATH('//price/text()', document) AS DECIMAL(10,2)) AS price_numeric
FROM test_xml_functions
WHERE XMLEXISTS('//price' PASSING document) ORDER BY id;

-- ============================================================================
-- Section 18: XML Schema Types (Illustrative)
-- ============================================================================

CREATE TABLE test_xml_schema_types (
    id INT PRIMARY KEY,
    typed_data XML
);

-- In a full implementation, XML Schema validation would ensure types
INSERT INTO test_xml_schema_types VALUES
    (1, '<data><int>42</int><string>text</string><bool>true</bool></data>'),
    (2, '<data><date>2024-01-15</date><time>10:30:00</time></data>');

-- Extract and cast to appropriate SQL types
SELECT id,
       CAST(XPATH('//int/text()', typed_data) AS INT) AS int_value,
       XPATH('//string/text()', typed_data) AS string_value
FROM test_xml_schema_types WHERE id = 1;

-- ============================================================================
-- Section 19: Real-world Use Case - API Response Storage
-- ============================================================================

CREATE TABLE test_xml_api_responses (
    id INT PRIMARY KEY,
    endpoint VARCHAR(200),
    response XML,
    status_code INT,
    fetched_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_xml_api_responses (id, endpoint, response, status_code) VALUES
    (1, '/api/user/123', '<response>
                            <user id="123">
                              <name>Alice Johnson</name>
                              <email>alice@example.com</email>
                              <created>2024-01-01</created>
                            </user>
                          </response>', 200),
    (2, '/api/products', '<response>
                            <products>
                              <product id="1"><name>Item A</name><price>99.99</price></product>
                              <product id="2"><name>Item B</name><price>149.99</price></product>
                            </products>
                          </response>', 200);

-- Query API responses
SELECT id, endpoint,
       XPATH('//user/@id', response) AS user_id,
       XPATH('//user/name/text()', response) AS user_name
FROM test_xml_api_responses WHERE endpoint = '/api/user/123';

SELECT id, XPATH('count(//product)', response) AS product_count
FROM test_xml_api_responses WHERE endpoint = '/api/products';

-- ============================================================================
-- Section 20: XML Best Practices
-- ============================================================================

CREATE TABLE test_xml_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_xml_best_practices VALUES
    (1, 'Store only well-formed XML; validate on insert'),
    (2, 'Use XPath for querying specific elements and attributes'),
    (3, 'Consider indexing frequently-queried XPath expressions'),
    (4, 'Use XMLTABLE to convert XML to relational format for complex queries'),
    (5, 'For large XML documents, consider storing in external files'),
    (6, 'XML is ideal for: configuration files, document storage, data interchange'),
    (7, 'Use namespaces to avoid element name conflicts'),
    (8, 'XMLAGG for aggregating relational data into XML documents'),
    (9, 'Consider JSON/JSONB for flexible semi-structured data (often faster)'),
    (10, 'Use CDATA sections for text containing special XML characters');

SELECT id, guideline FROM test_xml_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_xml_best_practices;
DROP TABLE test_xml_api_responses;
DROP TABLE test_xml_schema_types;
DROP TABLE test_xml_functions;
DROP TABLE test_xml_nulls;
DROP TABLE test_xml_indexing;
DROP TABLE test_xml_comparison;
DROP TABLE test_xml_svg;
DROP TABLE test_xml_feeds;
DROP TABLE test_xml_documents;
DROP TABLE test_xml_modification;
DROP TABLE test_xml_namespaces;
DROP TABLE test_xml_to_table;
DROP TABLE test_xml_aggregation;
DROP TABLE test_xml_construction;
DROP TABLE test_xml_xpath_predicates;
DROP TABLE test_xml_xpath_attributes;
DROP TABLE test_xml_xpath_elements;
DROP TABLE test_xml_validation;
DROP TABLE test_xml_basic;

DROP DATABASE test_xml_db;

-- End of XML data type tests
