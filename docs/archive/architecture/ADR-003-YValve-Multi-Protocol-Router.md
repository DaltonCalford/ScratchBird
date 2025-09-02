# ADR-003: Y-Valve Multi-Protocol Router Design

## Status
Proposed

## Context

ScratchBird needs to support multiple database protocols (PostgreSQL, MySQL, Firebird, TDS/MSSQL) simultaneously while maintaining a single internal representation (BLR). This requires a sophisticated routing and translation layer that goes far beyond traditional database connection handling.

Traditional approaches either:
1. Support only one protocol natively
2. Use separate ports/processes for each protocol
3. Require protocol-specific builds

## Decision

We will implement a Y-Valve Router as the central protocol abstraction layer with the following key design decisions:

### 1. Protocol Detection Strategy

**Decision**: Use pattern matching on initial bytes with confidence scoring
- Detect protocol from first packet without prior knowledge
- Support protocol negotiation (e.g., SSL upgrade)
- Allow explicit protocol specification for ambiguous cases

**Rationale**:
- Enables single-port multi-protocol support
- Reduces configuration complexity
- Supports dynamic protocol discovery

### 2. Parser Architecture

**Decision**: Plugin-based parser architecture with common interface
```c
class IParser {
    virtual BLRResult parseToBLR(const std::string& sql) = 0;
    virtual ResponsePacket handleCommand(const CommandPacket& cmd) = 0;
    virtual DataPacket formatResults(const ResultSet& results) = 0;
};
```

**Rationale**:
- Allows independent parser development
- Enables third-party protocol additions
- Simplifies testing and maintenance
- Supports hot-swapping of parser implementations

### 3. Connection Context Management

**Decision**: Maintain rich per-connection context including:
- Protocol state machine
- Authentication state
- Transaction context
- Client capabilities
- Performance metrics

**Rationale**:
- Enables stateful protocol handling
- Supports connection pooling
- Allows protocol-specific optimizations
- Facilitates debugging and monitoring

### 4. Translation Layer

**Decision**: Two-stage translation process:
1. Protocol SQL → ScratchBird SQL (syntax normalization)
2. ScratchBird SQL → BLR (compilation)

**Rationale**:
- Preserves protocol-specific optimizations
- Enables SQL dialect debugging
- Allows caching at multiple levels
- Simplifies parser implementation

### 5. Protocol Emulation

**Decision**: Implement protocol-specific command emulation
- MySQL `SHOW` commands → System queries
- PostgreSQL `\d` commands → Metadata queries
- MSSQL `sp_*` procedures → Built-in functions

**Rationale**:
- Maintains client compatibility
- Reduces client-side changes
- Supports existing tools and ORMs

## Architectural Implications

### 1. Network Layer Integration

The Network Layer must provide:
```c
struct YValveConnectionHandoff {
    uint64_t connection_id;
    int socket_fd;
    ProtocolType protocol_type;  // Can be UNKNOWN
    struct initial_data;
    struct client_hints;
};
```

### 2. Parser Implementation Requirements

Each parser must:
- Implement the `IParser` interface
- Handle protocol-specific authentication
- Manage protocol state machine
- Provide type mapping
- Support protocol-specific features

### 3. Engine Interface

The engine receives only BLR, making it protocol-agnostic:
- Simplifies engine implementation
- Enables cross-protocol optimization
- Allows unified query planning
- Supports protocol-independent caching

### 4. Performance Considerations

**Caching Strategy**:
- L1: Protocol-specific SQL cache
- L2: ScratchBird SQL cache  
- L3: BLR cache
- L4: Query plan cache

**Fast Path Optimizations**:
- Simple query detection
- Direct BLR generation
- Prepared statement reuse
- Connection pooling per protocol

### 5. Security Model

**Per-Protocol Security**:
- Protocol-specific authentication methods
- SQL injection prevention per dialect
- Rate limiting per protocol
- Audit logging with protocol context

## Consequences

### Positive

1. **True Multi-Protocol Support**: Single instance serves all protocols
2. **Client Compatibility**: Existing applications work unchanged
3. **Unified Management**: Single configuration, monitoring, and administration
4. **Performance**: Shared caching and optimization across protocols
5. **Extensibility**: New protocols can be added as plugins
6. **Testing**: Protocol compliance can be tested independently

### Negative

1. **Complexity**: More complex than single-protocol systems
2. **Memory Overhead**: Per-connection context for multiple protocols
3. **Translation Overhead**: SQL must be translated before execution
4. **Debugging**: Multi-layer translation makes debugging harder
5. **Protocol Fidelity**: Some protocol-specific features may be difficult to emulate

### Neutral

1. **Development Effort**: Requires implementing multiple parsers
2. **Testing Burden**: Must test against multiple protocol test suites
3. **Documentation**: Must document protocol-specific behaviors
4. **Maintenance**: Must track changes in multiple protocols

## Alternatives Considered

### 1. Separate Processes per Protocol
- **Rejected**: Higher resource usage, complex inter-process communication

### 2. Protocol-Specific Builds
- **Rejected**: Deployment complexity, can't serve multiple protocols simultaneously

### 3. Single Protocol with Adapters
- **Rejected**: Poor performance, limited compatibility

### 4. Proxy-Based Approach
- **Rejected**: Additional latency, complex deployment

## Implementation Plan

### Phase 1: Core Y-Valve Framework
1. Protocol detection engine
2. Parser interface definition
3. Connection context manager
4. Basic routing logic

### Phase 2: Native Parser
1. ScratchBird SQL parser
2. BLR generator
3. Type system integration

### Phase 3: PostgreSQL Parser
1. Wire protocol v3 support
2. Extended query protocol
3. COPY support
4. Full compliance testing

### Phase 4: MySQL Parser
1. Client/server protocol
2. Prepared statements
3. Replication protocol (stretch)
4. Full compliance testing

### Phase 5: Additional Protocols
1. Firebird parser
2. TDS parser
3. HTTP/REST API
4. gRPC support

## Metrics for Success

1. **Protocol Compliance**: Pass 95%+ of protocol test suites
2. **Performance**: <5% overhead vs native protocol
3. **Compatibility**: Support major ORMs and tools
4. **Reliability**: 99.99% uptime in production
5. **Scalability**: 10,000+ concurrent connections

## References

- PostgreSQL Frontend/Backend Protocol
- MySQL Client/Server Protocol
- Firebird Wire Protocol Specification  
- TDS Protocol Specification
- Database Wire Protocol Comparison Study

## Decision Records

This ADR relates to:
- ADR-001: MGA Over Traditional MVCC
- ADR-002: UUID-Based Schema System

## Notes

The Y-Valve name comes from Firebird but our implementation is fundamentally different - it's a multi-protocol router rather than just an embedded/server switch. This positions ScratchBird as a universal database that can replace multiple database systems in an organization.