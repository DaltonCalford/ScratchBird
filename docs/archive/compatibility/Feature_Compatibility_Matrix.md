# Feature Compatibility Matrix

## SQL Feature Support

| Feature | PostgreSQL | MySQL | MSSQL | Firebird | ScratchBird | Notes |
|---------|------------|-------|-------|----------|-------------|-------|
| **DDL** |
| CREATE TABLE | ✅ | ✅ | ✅ | ✅ | ✅ | Full support |
| ALTER TABLE | ✅ | ✅ | ✅ | ✅ | ✅ | All variants |
| DROP TABLE | ✅ | ✅ | ✅ | ✅ | ✅ | With CASCADE |
| CREATE INDEX | ✅ | ✅ | ✅ | ✅ | ✅ | Multiple types |
| CREATE VIEW | ✅ | ✅ | ✅ | ✅ | ✅ | Updatable views |
| CREATE PROCEDURE | ✅ | ✅ | ✅ | ✅ | ✅ | Language variants |
| CREATE TRIGGER | ✅ | ✅ | ✅ | ✅ | ✅ | Before/After |
| **DML** |
| INSERT | ✅ | ✅ | ✅ | ✅ | ✅ | Full support |
| UPDATE | ✅ | ✅ | ✅ | ✅ | ✅ | With JOIN |
| DELETE | ✅ | ✅ | ✅ | ✅ | ✅ | With JOIN |
| MERGE/UPSERT | ✅ | ⚠️ | ✅ | ✅ | ✅ | ON CONFLICT |
| SELECT | ✅ | ✅ | ✅ | ✅ | ✅ | Full support |
| **Advanced SQL** |
| CTEs | ✅ | ✅ 8.0+ | ✅ | ✅ | ✅ | Recursive |
| Window Functions | ✅ | ✅ 8.0+ | ✅ | ✅ 3.0+ | ✅ | Full set |
| LATERAL JOIN | ✅ | ✅ 8.0.14+ | ✅ APPLY | ❌ | ✅ | Via APPLY |
| JSON Operations | ✅ | ✅ | ✅ | ❌ | ✅ | JSONB support |
| Arrays | ✅ | ❌ | ❌ | ❌ | ✅ | PostgreSQL-style |
| Full Text Search | ✅ | ✅ | ✅ | ❌ | 🔄 | Planned |
| **Transactions** |
| BEGIN/COMMIT | ✅ | ✅ | ✅ | ✅ | ✅ | Standard |
| SAVEPOINT | ✅ | ✅ | ✅ | ✅ | ✅ | Nested |
| Isolation Levels | 4 | 4 | 4 | 3 | 4 | All standard |
| Two-Phase Commit | ✅ | ✅ | ✅ | ✅ | 🔄 | Planned |

Legend: ✅ Full Support | ⚠️ Partial | ❌ Not Supported | 🔄 Planned

## Data Type Compatibility

| Type Category | PostgreSQL | MySQL | MSSQL | Firebird | ScratchBird |
|--------------|------------|-------|-------|----------|-------------|
| **Numeric** |
| TINYINT | smallint | TINYINT | tinyint | SMALLINT | INT8 |
| SMALLINT | smallint | SMALLINT | smallint | SMALLINT | INT16 |
| INTEGER | integer | INT | int | INTEGER | INT32 |
| BIGINT | bigint | BIGINT | bigint | BIGINT | INT64 |
| DECIMAL(p,s) | decimal | DECIMAL | decimal | DECIMAL | DECIMAL |
| REAL | real | FLOAT | real | FLOAT | FLOAT32 |
| DOUBLE | double precision | DOUBLE | float | DOUBLE PRECISION | FLOAT64 |
| **String** |
| CHAR(n) | char | CHAR | char | CHAR | CHAR |
| VARCHAR(n) | varchar | VARCHAR | varchar | VARCHAR | VARCHAR |
| TEXT | text | TEXT | varchar(max) | BLOB SUB_TYPE TEXT | TEXT |
| **Binary** |
| BYTEA | bytea | BLOB | varbinary | BLOB | BLOB |
| **Temporal** |
| DATE | date | DATE | date | DATE | DATE |
| TIME | time | TIME | time | TIME | TIME |
| TIMESTAMP | timestamp | DATETIME | datetime2 | TIMESTAMP | TIMESTAMP |
| INTERVAL | interval | - | - | - | INTERVAL |
| **Special** |
| BOOLEAN | boolean | BOOLEAN | bit | BOOLEAN | BOOLEAN |
| UUID | uuid | - | uniqueidentifier | - | UUID |
| JSON | json/jsonb | JSON | - | - | JSON/JSONB |
| ARRAY | type[] | - | - | - | ARRAY |
| XML | xml | - | xml | - | XML |

## Function Compatibility

| Function Type | PostgreSQL | MySQL | MSSQL | Firebird | ScratchBird |
|--------------|------------|-------|-------|----------|-------------|
| **String Functions** |
| Concatenation | \|\| | CONCAT() | + | \|\| | All styles |
| Substring | substring() | SUBSTRING() | SUBSTRING() | SUBSTRING() | All styles |
| Length | length() | LENGTH() | LEN() | CHAR_LENGTH() | All styles |
| Upper/Lower | upper/lower | UPPER/LOWER | UPPER/LOWER | UPPER/LOWER | ✅ |
| Trim | trim() | TRIM() | TRIM() | TRIM() | ✅ |
| **Date Functions** |
| Current Time | now() | NOW() | GETDATE() | CURRENT_TIMESTAMP | All styles |
| Extract | extract() | EXTRACT() | DATEPART() | EXTRACT() | All styles |
| Date Add | + interval | DATE_ADD() | DATEADD() | DATEADD() | All styles |
| Date Format | to_char() | DATE_FORMAT() | FORMAT() | - | All styles |
| **Aggregate Functions** |
| STRING_AGG | string_agg() | GROUP_CONCAT() | STRING_AGG() | LIST() | All styles |
| Array Agg | array_agg() | JSON_ARRAYAGG() | - | - | array_agg() |
| **JSON Functions** |
| Extract | ->>, #> | ->, ->> | JSON_VALUE() | - | All styles |
| Build | json_build | JSON_OBJECT() | FOR JSON | - | All styles |

## Protocol Support

| Protocol Feature | PostgreSQL | MySQL | MSSQL | Firebird | Notes |
|-----------------|------------|-------|-------|----------|-------|
| **Wire Protocol** |
| Version | v3.0 | v10 | TDS 7.4 | v13 | Latest stable |
| Default Port | 5432 | 3306 | 1433 | 3050 | Configurable |
| SSL/TLS | ✅ | ✅ | ✅ | ✅ | Required |
| **Authentication** |
| Password | ✅ | ✅ | ✅ | ✅ | Basic |
| MD5 | ✅ | ✅ | - | - | Legacy |
| SCRAM-SHA-256 | ✅ | - | - | - | Secure |
| Kerberos | ✅ | ✅ | ✅ | ✅ | Enterprise |
| Certificate | ✅ | ✅ | ✅ | ✅ | X.509 |
| **Features** |
| Prepared Statements | ✅ | ✅ | ✅ | ✅ | Cached |
| Cursors | ✅ | ✅ | ✅ | ✅ | Scrollable |
| Batch Operations | ✅ | ✅ | ✅ | ✅ | Optimized |
| Async Operations | ✅ | ✅ | ✅ | ✅ | Non-blocking |

## Client Compatibility

| Client Type | PostgreSQL | MySQL | MSSQL | Firebird | Status |
|------------|------------|-------|-------|----------|--------|
| **Command Line** |
| psql | ✅ | - | - | - | Full |
| mysql | - | ✅ | - | - | Full |
| sqlcmd | - | - | ✅ | - | Full |
| isql | - | - | - | ✅ | Full |
| **GUI Tools** |
| pgAdmin | ✅ | - | - | - | Full |
| phpMyAdmin | - | ✅ | - | - | Full |
| SSMS | - | - | ✅ | - | Partial |
| FlameRobin | - | - | - | ✅ | Full |
| DBeaver | ✅ | ✅ | ✅ | ✅ | Full |
| **Libraries** |
| libpq (C) | ✅ | - | - | - | Full |
| MySQL Connector/C | - | ✅ | - | - | Full |
| ODBC | ✅ | ✅ | ✅ | ✅ | Full |
| JDBC | ✅ | ✅ | ✅ | ✅ | Full |
| **ORMs** |
| SQLAlchemy | ✅ | ✅ | ✅ | ✅ | Full |
| Hibernate | ✅ | ✅ | ✅ | ✅ | Full |
| Django ORM | ✅ | ✅ | ⚠️ | ⚠️ | Full PG/MySQL |
| Entity Framework | ✅ | ✅ | ✅ | - | Full |
| ActiveRecord | ✅ | ✅ | ✅ | - | Full |

## Application Compatibility

| Application | Database | Compatibility | Notes |
|-------------|----------|--------------|-------|
| WordPress | MySQL | ✅ Full | Primary test case |
| Drupal | MySQL/PostgreSQL | ✅ Full | Multi-DB support |
| Joomla | MySQL | ✅ Full | Standard CMS |
| **Developer Tools** |
| GitLab | PostgreSQL | ✅ Full | Complex schema |
| Redmine | MySQL/PostgreSQL | ✅ Full | Project management |
| SonarQube | PostgreSQL | ✅ Full | Code analysis |
| **E-Commerce** |
| Magento | MySQL | ✅ Full | E-commerce |
| PrestaShop | MySQL | ✅ Full | Online store |
| OpenCart | MySQL | ✅ Full | Shopping cart |
| **Enterprise** |
| Odoo | PostgreSQL | ⚠️ Partial | Complex features |
| SuiteCRM | MySQL | ✅ Full | CRM system |
| Nextcloud | MySQL/PostgreSQL | ✅ Full | File sharing |

## Performance Targets

| Metric | PostgreSQL | MySQL | MSSQL | Firebird | ScratchBird Target |
|--------|------------|-------|-------|----------|-------------------|
| Simple SELECT (ms) | 0.05 | 0.06 | 0.07 | 0.04 | < 0.05 |
| Index Lookup (ms) | 0.02 | 0.03 | 0.03 | 0.02 | < 0.02 |
| Join 1K x 1K (ms) | 5 | 7 | 6 | 4 | < 5 |
| Insert Row (ms) | 0.1 | 0.15 | 0.12 | 0.08 | < 0.1 |
| TPC-C (tpmC) | 500K | 400K | 450K | 300K | > 400K |
| TPC-H SF=1 (sec) | 10 | 15 | 12 | 20 | < 15 |

## Limitations and Differences

### Known Limitations
1. **PostgreSQL Features Not Supported Initially:**
   - LISTEN/NOTIFY
   - Large Objects (lo_*)
   - Table inheritance
   - Exclusion constraints

2. **MySQL Features Not Supported Initially:**
   - HANDLER statements
   - GET_LOCK/RELEASE_LOCK
   - LOAD DATA INFILE (security)

3. **MSSQL Features Not Supported Initially:**
   - CLR integration
   - Service Broker
   - FileStream
   - Temporal tables

4. **Firebird Features Always Supported:**
   - EXECUTE BLOCK
   - EXECUTE STATEMENT
   - Generators/Sequences
   - External tables

### Behavioral Differences
1. **NULL Handling:**
   - MySQL: NULL-safe equality (<=>) supported
   - PostgreSQL: IS DISTINCT FROM supported
   - ScratchBird: Both supported

2. **Case Sensitivity:**
   - MySQL: Case-insensitive by default
   - PostgreSQL: Case-sensitive
   - ScratchBird: Configurable per connection

3. **Transaction Isolation:**
   - MySQL: REPEATABLE READ default
   - PostgreSQL: READ COMMITTED default
   - ScratchBird: Configurable per database