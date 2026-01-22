# Python Driver Specification
**Priority:** P0 (Critical - Beta Required)
**Status:** Draft
**Target Market:** >50% of developers worldwide
**Use Cases:** Data science, ML/AI, web applications, scripting

---


## ScratchBird V2 Scope (Native Protocol Only)

- Protocol: ScratchBird Native Wire Protocol (SBWP) v1.1 only.
- Default port: 3092.
- TLS 1.3 required.
- Emulated protocol drivers (PostgreSQL/MySQL/Firebird/TDS) are out of scope.


## Wrapper Types
- JSONB: `scratchbird.types.Jsonb`
- RANGE: `scratchbird.types.Range`
- GEOMETRY: `scratchbird.types.Geometry`

## Overview

Python is the most critical driver to develop, used by over half of developers worldwide. It is the lingua franca of data science, machine learning, and modern web development.

**Key Requirements:**
- PEP 249 DB-API 2.0 compliance
- SQLAlchemy dialect support
- Async/await support (asyncio)
- Type hints (Python 3.7+)
- Support for Pandas DataFrames
- NumPy array integration (for vector operations)
- Jupyter notebook compatibility

## Usage examples

```python
import scratchbird

conn = scratchbird.connect(host="localhost", database="db", user="user", password="pass")
cur = conn.cursor()
cur.execute("select 1 as one")

cur.execute("select * from widgets where id = :id", {"id": 42})
rows = cur.fetchall()
```

---

## Specification Documents

### Required Documents

- [x] [SPECIFICATION.md](SPECIFICATION.md) - Detailed technical specification
  - PEP 249 compliance details
  - API surface area
  - Connection pooling
  - Transaction management
  - Error handling
  - Type mappings (Python ↔ ScratchBird)

- [x] [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) - Development roadmap
  - Milestone 1: Basic DB-API 2.0 connectivity
  - Milestone 2: SQLAlchemy dialect
  - Milestone 3: Async support
  - Milestone 4: Pandas/NumPy integration
  - Resource requirements
  - Timeline estimates

- [x] [TESTING_CRITERIA.md](TESTING_CRITERIA.md) - Test requirements
  - DB-API 2.0 compliance test suite
  - SQLAlchemy test suite compatibility
  - Performance benchmarks vs psycopg2
  - Integration tests with Pandas, Jupyter
  - Vector operation tests with NumPy

- [x] [COMPATIBILITY_MATRIX.md](COMPATIBILITY_MATRIX.md) - Version support
  - Python versions (3.8, 3.9, 3.10, 3.11, 3.12)
  - SQLAlchemy versions (1.4.x, 2.0.x)
  - Pandas versions
  - Operating systems (Linux, macOS, Windows)

- [x] [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md) - Migration from other drivers
  - From psycopg2 (PostgreSQL)
  - From mysql-connector-python
  - From firebirdsql
  - Code examples and gotchas

- [x] [API_REFERENCE.md](API_REFERENCE.md) - Complete API documentation
  - Connection class
  - Cursor class
  - Type objects
  - Module constants
  - Exceptions
  - Connection pool API

### Shared References

- [../DRIVER_BASELINE_SPEC.md](../DRIVER_BASELINE_SPEC.md) - Shared V2 driver requirements.

### Examples Directory

- [ ] **examples/basic_usage.py** - Simple connection and query
- [ ] **examples/sqlalchemy_orm.py** - Using SQLAlchemy ORM
- [ ] **examples/pandas_integration.py** - DataFrame import/export
- [ ] **examples/async_operations.py** - Async/await usage
- [ ] **examples/vector_search.py** - Vector similarity search with NumPy
- [ ] **examples/transaction_management.py** - Transaction examples
- [ ] **examples/connection_pooling.py** - Connection pool usage
- [ ] **examples/jupyter_notebook.ipynb** - Jupyter notebook example

---

## Key Integration Points

### SQLAlchemy Support

**Critical**: SQLAlchemy is "one of the most popular ORMs built for Python"

Requirements:
- Custom dialect implementation
- Type mappings for all ScratchBird types
- Compiler for ScratchBird-specific SQL extensions
- Reflection support (introspection)
- Connection pooling integration
- Support for both SQLAlchemy 1.4 and 2.0

Example dialect registration:
```python
# scratchbird.dialects.sqlalchemy
class ScratchBirdDialect(Dialect):
    name = 'scratchbird'
    driver = 'scratchbird'
    # ... implementation
```

### Django ORM Support (Separate - see orms-frameworks/django-orm/)

Integration with Django's database backend system:
- Custom database backend
- Migration support
- Query compiler
- Schema editor

### Pandas Integration

Requirements:
- `read_sql()` compatibility
- `DataFrame.to_sql()` support
- Efficient bulk inserts
- Type preservation

Example:
```python
import pandas as pd
import scratchbird

conn = scratchbird.connect(dsn="scratchbird://localhost/mydb")
df = pd.read_sql("SELECT * FROM users", conn)
df.to_sql("users_copy", conn, if_exists='replace')
```

### Vector Operations (AI/ML)

Requirements:
- Native vector type support
- NumPy array ↔ vector column mapping
- Efficient batch insert of embeddings
- Vector similarity search functions

Example:
```python
import numpy as np
import scratchbird

conn = scratchbird.connect(dsn="scratchbird://localhost/vectordb")
cursor = conn.cursor()

# Insert embedding
embedding = np.random.rand(768)  # 768-dimensional vector
cursor.execute(
    "INSERT INTO documents (id, content, embedding) VALUES (%s, %s, %s)",
    (1, "Sample text", embedding)
)

# Vector similarity search
query_embedding = np.random.rand(768)
cursor.execute(
    "SELECT id, content FROM documents ORDER BY embedding <-> %s LIMIT 10",
    (query_embedding,)
)
results = cursor.fetchall()
```

---

## PEP 249 Compliance Requirements

### Module Interface

```python
# Required module-level attributes
apilevel = '2.0'
threadsafety = 2  # Threads may share the module and connections
paramstyle = 'pyformat'  # or 'qmark', 'numeric', 'named', 'format'

# Required exceptions
Error
Warning
InterfaceError
DatabaseError
    DataError
    OperationalError
    IntegrityError
    InternalError
    ProgrammingError
    NotSupportedError

# Constructor
connect(dsn=None, user=None, password=None, host=None, database=None, ...)
```

### Connection Object

```python
class Connection:
    def close() -> None: ...
    def commit() -> None: ...
    def rollback() -> None: ...
    def cursor() -> Cursor: ...
```

### Cursor Object

```python
class Cursor:
    description: Sequence[Tuple[str, ...]]
    rowcount: int

    def close() -> None: ...
    def execute(operation: str, parameters: Sequence = None) -> None: ...
    def executemany(operation: str, seq_of_parameters: Sequence) -> None: ...
    def fetchone() -> Optional[Sequence]: ...
    def fetchmany(size: int = cursor.arraysize) -> List[Sequence]: ...
    def fetchall() -> List[Sequence]: ...
    def setinputsizes(sizes: Sequence) -> None: ...
    def setoutputsize(size: int, column: int = None) -> None: ...
```

---

## Performance Targets

Benchmark against psycopg2 (PostgreSQL driver):

| Operation | Target Performance |
|-----------|-------------------|
| Connection establishment | Within 10% of psycopg2 |
| Simple SELECT (1 row) | Within 10% of psycopg2 |
| Bulk SELECT (10,000 rows) | Within 15% of psycopg2 |
| Simple INSERT | Within 10% of psycopg2 |
| Bulk INSERT (10,000 rows) | Within 15% of psycopg2 |
| Transaction commit | Within 10% of psycopg2 |
| Vector similarity search | Better than pgvector |

---

## Dependencies

### Runtime Dependencies

- Python >= 3.8
- (Optional) NumPy >= 1.20 (for vector operations)
- (Optional) Pandas >= 1.3 (for DataFrame integration)

### Build Dependencies

- setuptools or build
- C compiler (for binary extensions, if needed)
- Cython (if using for performance)

### Optional Dependencies

- SQLAlchemy >= 1.4 (for ORM support)
- asyncio (stdlib, for async support)
- typing_extensions (for older Python versions)

---

## Installation

```bash
# PyPI installation (future)
pip install scratchbird

# Development installation
git clone https://github.com/yourusername/scratchbird-python.git
cd scratchbird-python
pip install -e ".[dev]"
```

---

## Documentation Requirements

### User Documentation

- [ ] Quick start guide
- [ ] Installation instructions
- [ ] Connection string format
- [ ] Type mapping reference
- [ ] Transaction management guide
- [ ] Connection pooling guide
- [ ] Error handling best practices
- [ ] Performance tuning tips

### API Documentation

- [ ] Full API reference (Sphinx)
- [ ] Docstrings for all public APIs
- [ ] Type hints for all functions
- [ ] Usage examples in docstrings

### Integration Guides

- [ ] SQLAlchemy integration guide
- [ ] Django integration guide
- [ ] Flask integration guide
- [ ] FastAPI integration guide
- [ ] Pandas integration guide
- [ ] Jupyter notebook guide
- [ ] LangChain integration guide

---

## Release Criteria

### Functional Completeness

- [ ] 100% PEP 249 DB-API 2.0 compliance
- [ ] SQLAlchemy dialect implemented and tested
- [ ] All core data types supported
- [ ] Transaction management working
- [ ] Connection pooling working
- [ ] Async support implemented (if committed)

### Quality Metrics

- [ ] >95% test coverage
- [ ] All PEP 249 compliance tests passing
- [ ] SQLAlchemy test suite passing
- [ ] Performance benchmarks met
- [ ] Memory leak tests passing
- [ ] Thread safety tests passing

### Documentation

- [ ] API reference complete
- [ ] User guide complete
- [ ] 10+ code examples
- [ ] Migration guide from psycopg2/mysql-connector
- [ ] Troubleshooting guide

### Packaging

- [ ] PyPI package published
- [ ] Conda package (optional)
- [ ] Source distributions
- [ ] Wheel distributions (Linux, macOS, Windows)
- [ ] Verified pip installation on all platforms

---

## Community and Support

### Documentation Locations

- PyPI: https://pypi.org/project/scratchbird/
- Read the Docs: https://scratchbird-python.readthedocs.io/
- GitHub: https://github.com/yourusername/scratchbird-python
- Examples: https://github.com/yourusername/scratchbird-python/tree/main/examples

### Issue Tracking

- GitHub Issues: https://github.com/yourusername/scratchbird-python/issues
- Response time target: <3 days
- Bug fix SLA: Critical <7 days, High <14 days

### Community Channels

- Discord: ScratchBird #python-driver
- Stack Overflow: Tag `scratchbird-python`
- Mailing list: scratchbird-python@googlegroups.com

---

## References

1. **PEP 249**: Python Database API Specification v2.0
   https://peps.python.org/pep-0249/

2. **SQLAlchemy Documentation**
   https://docs.sqlalchemy.org/

3. **psycopg2 Driver** (reference implementation)
   https://www.psycopg.org/

4. **Stack Overflow Developer Survey 2024**
   Python usage >50% of developers

---

## Next Steps

1. **Create detailed SPECIFICATION.md** with complete API design
2. **Develop IMPLEMENTATION_PLAN.md** with milestones and timeline
3. **Write TESTING_CRITERIA.md** with comprehensive test requirements
4. **Begin prototype** with basic DB-API 2.0 connection and cursor
5. **Set up CI/CD** for automated testing against ScratchBird instances

---

**Document Version:** 1.0 (Template)
**Last Updated:** 2026-01-03
**Status:** Draft
**Assigned To:** TBD
