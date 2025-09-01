# Reference Materials for ScratchBird

## ✅ Completed References

These references have been gathered and documented:

### Technical Specifications
- **Wire Protocols**: PostgreSQL, MySQL, Firebird, TDS - `/references/wire_protocols/`
- **Data Types**: Complete type system for all databases - `/references/data_types/`
- **Page Layouts**: All 27 page types specified - `/workspace/references/archive/technical_specifications/PAGE_LAYOUTS_AND_STRUCTURES.md`
- **BLR Specification**: Complete instruction set - `/references/technical_specifications/BLR_SPECIFICATION.md`
- **SQL Grammar**: Full BNF/EBNF - `/references/technical_specifications/SQL_GRAMMAR_BNF.md`
- **MGA Implementation**: Based on Firebird with enhancements - `/references/technical_specifications/MGA_IMPLEMENTATION.md`
- **C API**: Complete specification - `/references/technical_specifications/C_API_SPECIFICATION.md`
- **Y-Valve Architecture**: Multi-protocol router - `/references/technical_specifications/Y_VALVE_ARCHITECTURE.md`

### Architecture Documents
- **ADR-001**: MGA Over Traditional MVCC - `/docs/architecture/ADR-001-MGA-Over-Traditional-MVCC.md`
- **ADR-002**: UUID-Based Schema System - `/docs/architecture/ADR-002-UUID-Based-Schema.md`
- **ADR-003**: Y-Valve Multi-Protocol Router - `/docs/architecture/ADR-003-YValve-Multi-Protocol-Router.md`

## 📚 Additional References Needed (Future)

### Post-Alpha References

These can be gathered as needed during implementation:

#### Query Optimization
- Cost model parameters from PostgreSQL
- Join algorithm implementations
- Statistics collection strategies
- Adaptive query execution papers

#### Advanced Indexing
- GIN index implementation (PostgreSQL)
- R-tree algorithms for spatial data
- LSM tree implementations
- Bitmap index compression techniques

#### Distributed Systems (Post-Beta)
- Raft consensus protocol
- Vector clocks implementation
- Consistent hashing algorithms
- CAP theorem implications

#### Security (Beta Phase)
- SCRAM-SHA-256 specification (RFC 7677)
- TLS 1.3 specification (RFC 8446)
- OAuth 2.0 for databases (RFC 6749)

#### Performance Benchmarks
- TPC-C specification
- TPC-H queries
- YCSB workloads
- Sysbench patterns

## 🔗 External Resources

### Standards Documents
- SQL:2016 standard (ISO/IEC 9075)
- JDBC 4.3 specification
- ODBC 3.8 specification
- XDR specification (RFC 4506)

### Implementation References
- Firebird source (for MGA details)
- PostgreSQL source (for query optimization)
- MySQL source (for replication)
- SQLite source (for embedded patterns)

### Academic Papers
- "Multi-Version Concurrency Control" - Reed (1978)
- "The Transaction Concept" - Gray (1981)
- "ARIES: A Transaction Recovery Method" - Mohan et al. (1992)
- "The Log-Structured Merge-Tree" - O'Neil et al. (1996)

## 📋 Reference Gathering Strategy

1. **Just-In-Time**: Gather references as phases require them
2. **Extract Key Concepts**: Don't copy full source, extract algorithms
3. **Document Sources**: Always cite original sources
4. **Version Specific**: Note which version of external systems

## 🎯 Current Focus

For Alpha implementation, all necessary references are complete:
- ✅ Page structures
- ✅ Transaction management (MGA)
- ✅ C API specification
- ✅ SQL grammar
- ✅ Wire protocols
- ✅ Type system

No additional references needed until Beta phase.

---

**Note**: The `/references/SOURCE_CODE_STRATEGY.md` document outlines our approach to using external source code (extract concepts, not copy code).