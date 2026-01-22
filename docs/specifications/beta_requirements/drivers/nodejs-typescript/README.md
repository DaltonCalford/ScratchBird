# Node.js/TypeScript Driver Specification
**Priority:** P0 (Critical - Beta Required)
**Status:** Draft
**Target Market:** JavaScript - Most-used programming language worldwide
**Use Cases:** Web applications, APIs, serverless, full-stack development, microservices

---


## ScratchBird V2 Scope (Native Protocol Only)

- Protocol: ScratchBird Native Wire Protocol (SBWP) v1.1 only.
- Default port: 3092.
- TLS 1.3 required.
- Emulated protocol drivers (PostgreSQL/MySQL/Firebird/TDS) are out of scope.


## Wrapper Types
- JSONB: `ScratchbirdJsonb`
- RANGE: `ScratchbirdRange<T>`
- GEOMETRY: `ScratchbirdGeometry`

## Overview

JavaScript/TypeScript is the most-used programming language globally. A native Node.js driver with full TypeScript support is critical for ScratchBird adoption in modern web development.

**Key Requirements:**
- Promise-based async API (async/await)
- Full TypeScript type definitions (.d.ts)
- Connection pooling built-in
- Stream support for large result sets
- ESM (ES Modules) and CommonJS compatibility
- Zero native dependencies (pure JavaScript preferred)
- Browser-compatible client (optional, for edge computing)

## Usage examples

```ts
const client = new Client("scratchbird://user:pass@localhost:3092/db");
await client.connect();

await client.query("select 1 as one");

await client.prepare("by_id", "select * from widgets where id = $1");
const rows = await client.execute("by_id", [42]);
```

---

## Specification Documents

### Required Documents

- [x] [SPECIFICATION.md](SPECIFICATION.md) - Detailed technical specification
  - API surface area (connection, query, transaction)
  - Type definitions for all methods
  - Error handling and error types
  - Type mappings (JavaScript/TypeScript ↔ ScratchBird)
  - Connection pooling architecture
  - Stream API design

- [x] [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) - Development roadmap
  - Milestone 1: Basic connection and query (CommonJS)
  - Milestone 2: TypeScript types and ESM support
  - Milestone 3: Connection pooling
  - Milestone 4: Stream API for large results
  - Milestone 5: Prisma/TypeORM adapter layers
  - Resource requirements
  - Timeline estimates

- [x] [TESTING_CRITERIA.md](TESTING_CRITERIA.md) - Test requirements
  - Unit tests for all API methods
  - Integration tests with real database
  - Performance benchmarks vs pg (node-postgres)
  - Memory leak tests (long-running connections)
  - TypeScript compilation tests
  - Browser compatibility tests (if applicable)

- [x] [COMPATIBILITY_MATRIX.md](COMPATIBILITY_MATRIX.md) - Version support
  - Node.js versions (16 LTS, 18 LTS, 20 LTS, 21+)
  - TypeScript versions (4.5+, 5.0+)
  - Package managers (npm, yarn, pnpm, bun)
  - Runtimes (Node.js, Deno, Bun)
  - Operating systems (Linux, macOS, Windows)

- [x] [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md) - Migration from other drivers
  - From pg (node-postgres/PostgreSQL)
  - From mysql2
  - From better-sqlite3
  - From node-firebird
  - Code examples and gotchas

- [x] [API_REFERENCE.md](API_REFERENCE.md) - Complete API documentation
  - Client class
  - Pool class
  - Connection class
  - Query result types
  - Error classes
  - Type converters

### Shared References

- [../DRIVER_BASELINE_SPEC.md](../DRIVER_BASELINE_SPEC.md) - Shared V2 driver requirements.

### Examples Directory

- [ ] **examples/basic_query.js** - Simple connection and query (CommonJS)
- [ ] **examples/basic_query.mjs** - ESM version
- [ ] **examples/typescript_example.ts** - TypeScript usage with types
- [ ] **examples/connection_pool.js** - Connection pool usage
- [ ] **examples/transactions.js** - Transaction management
- [ ] **examples/prepared_statements.js** - Parameterized queries
- [ ] **examples/streaming.js** - Stream large result sets
- [ ] **examples/prisma_integration/** - Prisma ORM example
- [ ] **examples/typeorm_integration/** - TypeORM example
- [ ] **examples/express_api/** - Express.js REST API example
- [ ] **examples/nextjs_app/** - Next.js application example

---

## Key Integration Points

### Prisma Support

**Critical**: Prisma is the "type-safe Node.js ORM" with rapid adoption.

Requirements:
- Prisma database connector implementation
- Custom Prisma schema generator
- Migration support
- Introspection support
- Query engine adapter

Example Prisma schema:
```prisma
datasource db {
  provider = "scratchbird"
  url      = env("DATABASE_URL")
}

generator client {
  provider = "prisma-client-js"
}

model User {
  id        Int      @id @default(autoincrement())
  email     String   @unique
  name      String?
  posts     Post[]
  createdAt DateTime @default(now())
}

model Post {
  id        Int      @id @default(autoincrement())
  title     String
  content   String?
  published Boolean  @default(false)
  author    User     @relation(fields: [authorId], references: [id])
  authorId  Int
}
```

### TypeORM Support (Separate - see orms-frameworks/typeorm/)

Integration with TypeORM's DataSource system:
- Custom driver implementation
- Schema synchronization
- Migration support
- Query builder

### Basic Driver API

Requirements:
- Simple, intuitive API similar to node-postgres
- Full TypeScript type safety
- Promise-based (async/await)

Example usage:
```typescript
import { Client, Pool } from 'scratchbird';

// Simple client usage
const client = new Client({
  host: 'localhost',
  port: 3092,
  database: 'mydb',
  user: 'myuser',
  password: 'mypassword'
});

await client.connect();

// Parameterized query
const result = await client.query(
  'SELECT * FROM users WHERE email = $1',
  ['user@example.com']
);

console.log(result.rows);
await client.end();

// Connection pool usage (recommended for production)
const pool = new Pool({
  host: 'localhost',
  port: 3092,
  database: 'mydb',
  user: 'myuser',
  password: 'mypassword',
  max: 20, // max connections
  idleTimeoutMillis: 30000,
  connectionTimeoutMillis: 2000,
});

// Pool automatically manages connections
const { rows } = await pool.query('SELECT NOW()');
console.log(rows[0]);

// Transactions
const client = await pool.connect();
try {
  await client.query('BEGIN');
  await client.query('INSERT INTO users(name) VALUES($1)', ['Alice']);
  await client.query('INSERT INTO users(name) VALUES($1)', ['Bob']);
  await client.query('COMMIT');
} catch (e) {
  await client.query('ROLLBACK');
  throw e;
} finally {
  client.release();
}

await pool.end();
```

### TypeScript Type Safety

Requirements:
- Full .d.ts type definitions
- Generic result types
- Type inference for queries
- Strict null checking support

Example types:
```typescript
import { Client, QueryResult, QueryResultRow } from 'scratchbird';

interface User {
  id: number;
  email: string;
  name: string | null;
  created_at: Date;
}

const client = new Client(config);
await client.connect();

// Type-safe query result
const result: QueryResult<User> = await client.query<User>(
  'SELECT * FROM users WHERE id = $1',
  [123]
);

const user: User | undefined = result.rows[0];
if (user) {
  console.log(user.email); // TypeScript knows this is a string
  console.log(user.name?.toUpperCase()); // Handles null safely
}
```

### Stream Support for Large Results

Requirements:
- Cursor-based streaming
- Backpressure handling
- Memory-efficient for millions of rows

Example:
```typescript
import { Pool } from 'scratchbird';
import { pipeline } from 'stream/promises';
import { Transform } from 'stream';

const pool = new Pool(config);

// Stream query results
const queryStream = pool.query(
  new Query('SELECT * FROM large_table')
);

// Process rows as stream
await pipeline(
  queryStream,
  new Transform({
    objectMode: true,
    transform(row, encoding, callback) {
      // Process each row
      console.log(row);
      callback();
    }
  })
);
```

---

## API Design

### Client Class

```typescript
class Client {
  constructor(config: ClientConfig);

  connect(): Promise<void>;
  end(): Promise<void>;

  query<T extends QueryResultRow = any>(
    queryText: string,
    values?: any[]
  ): Promise<QueryResult<T>>;

  query<T extends QueryResultRow = any>(
    config: QueryConfig
  ): Promise<QueryResult<T>>;

  // Event emitters
  on(event: 'error', listener: (err: Error) => void): this;
  on(event: 'notice', listener: (notice: Notice) => void): this;
  on(event: 'notification', listener: (message: Notification) => void): this;
}

interface ClientConfig {
  host?: string;
  port?: number;
  database?: string;
  user?: string;
  password?: string;
  ssl?: boolean | TLSConnectionOptions;
  connectionString?: string; // e.g., "scratchbird://user:pass@host:3092/db"
  connectionTimeoutMillis?: number;
  keepAlive?: boolean;
  statement_timeout?: number;
  query_timeout?: number;
}
```

### Pool Class

```typescript
class Pool {
  constructor(config: PoolConfig);

  connect(): Promise<PoolClient>;
  end(): Promise<void>;

  query<T extends QueryResultRow = any>(
    queryText: string,
    values?: any[]
  ): Promise<QueryResult<T>>;

  // Pool statistics
  get totalCount(): number;
  get idleCount(): number;
  get waitingCount(): number;

  // Events
  on(event: 'error', listener: (err: Error, client: PoolClient) => void): this;
  on(event: 'connect', listener: (client: PoolClient) => void): this;
  on(event: 'acquire', listener: (client: PoolClient) => void): this;
  on(event: 'remove', listener: (client: PoolClient) => void): this;
}

interface PoolConfig extends ClientConfig {
  max?: number; // max pool size (default: 10)
  min?: number; // min pool size (default: 0)
  idleTimeoutMillis?: number; // close idle connections after (default: 10000)
  connectionTimeoutMillis?: number; // wait time for connection (default: 0 = no timeout)
  allowExitOnIdle?: boolean; // allow process exit when all connections idle
}
```

### Query Result Types

```typescript
interface QueryResult<R extends QueryResultRow = any> {
  rows: R[];
  rowCount: number;
  command: string; // 'SELECT', 'INSERT', 'UPDATE', etc.
  oid: number;
  fields: FieldDef[];
}

interface QueryResultRow {
  [column: string]: any;
}

interface FieldDef {
  name: string;
  tableID: number;
  columnID: number;
  dataTypeID: number;
  dataTypeSize: number;
  dataTypeModifier: number;
  format: string;
}

interface QueryConfig {
  text: string;
  values?: any[];
  rowMode?: 'array'; // return rows as arrays instead of objects
  types?: CustomTypeParsers;
}
```

### Error Classes

```typescript
class DatabaseError extends Error {
  length: number;
  severity: string;
  code: string;
  detail?: string;
  hint?: string;
  position?: string;
  internalPosition?: string;
  internalQuery?: string;
  where?: string;
  schema?: string;
  table?: string;
  column?: string;
  dataType?: string;
  constraint?: string;
  file?: string;
  line?: string;
  routine?: string;
}
```

---

## Performance Targets

Benchmark against pg (node-postgres) driver:

| Operation | Target Performance |
|-----------|-------------------|
| Connection establishment | Within 10% of pg |
| Simple SELECT (1 row) | Within 10% of pg |
| Bulk SELECT (10,000 rows) | Within 15% of pg |
| Simple INSERT | Within 10% of pg |
| Bulk INSERT (10,000 rows) | Within 15% of pg |
| Parameterized query | Within 10% of pg |
| Connection pool (100 queries) | Within 10% of pg |
| Memory usage (1M rows) | Within 20% of pg |

---

## Dependencies

### Runtime Dependencies

Minimal dependencies preferred (ideally zero for core driver):
- SBWP native protocol only (no pg-protocol dependency).
- (Optional) Type conversion libraries

### Development Dependencies

- TypeScript >= 4.5
- @types/node
- Jest or Mocha (testing)
- ESLint + Prettier (code quality)
- typedoc (API documentation)

### Peer Dependencies

- Node.js >= 16.0.0

---

## Installation

```bash
# NPM installation (future)
npm install scratchbird

# Yarn
yarn add scratchbird

# pnpm
pnpm add scratchbird

# Bun
bun add scratchbird

# Development installation
git clone https://github.com/yourusername/scratchbird-node.git
cd scratchbird-node
npm install
npm run build
npm test
```

---

## Package.json Structure

```json
{
  "name": "scratchbird",
  "version": "1.0.0",
  "description": "ScratchBird database driver for Node.js with full TypeScript support",
  "main": "./dist/index.js",
  "module": "./dist/index.mjs",
  "types": "./dist/index.d.ts",
  "exports": {
    ".": {
      "import": "./dist/index.mjs",
      "require": "./dist/index.js",
      "types": "./dist/index.d.ts"
    }
  },
  "engines": {
    "node": ">=16.0.0"
  },
  "keywords": [
    "database",
    "scratchbird",
    "sql",
    "driver",
    "typescript",
    "postgres-compatible"
  ],
  "license": "MIT",
  "repository": {
    "type": "git",
    "url": "https://github.com/yourusername/scratchbird-node.git"
  },
  "bugs": {
    "url": "https://github.com/yourusername/scratchbird-node/issues"
  },
  "scripts": {
    "build": "tsc && tsc -p tsconfig.esm.json",
    "test": "jest",
    "test:coverage": "jest --coverage",
    "lint": "eslint src/**/*.ts",
    "format": "prettier --write src/**/*.ts",
    "docs": "typedoc src/index.ts"
  }
}
```

---

## Documentation Requirements

### User Documentation

- [ ] Quick start guide (5-minute tutorial)
- [ ] Installation instructions (npm, yarn, pnpm)
- [ ] Connection string format
- [ ] Connection pooling best practices
- [ ] Transaction management guide
- [ ] Error handling guide
- [ ] Type safety guide (TypeScript)
- [ ] Performance tuning tips
- [ ] Deployment guide (production)

### API Documentation

- [ ] Full API reference (TypeDoc generated)
- [ ] JSDoc comments for all public APIs
- [ ] TypeScript type documentation
- [ ] Usage examples in JSDoc

### Integration Guides

- [ ] Prisma integration guide
- [ ] TypeORM integration guide
- [ ] Sequelize integration guide (if P1)
- [ ] Express.js integration guide
- [ ] Next.js integration guide
- [ ] NestJS integration guide
- [ ] Fastify integration guide
- [ ] Serverless deployment (AWS Lambda, Vercel, Cloudflare Workers)

---

## Release Criteria

### Functional Completeness

- [ ] Basic connection and query
- [ ] Parameterized queries (SQL injection prevention)
- [ ] Transaction management
- [ ] Connection pooling
- [ ] Stream API for large results
- [ ] All core data types supported
- [ ] Error handling complete
- [ ] ESM and CommonJS support

### Quality Metrics

- [ ] >95% test coverage
- [ ] All integration tests passing
- [ ] Performance benchmarks met (within 10-15% of pg)
- [ ] Memory leak tests passing
- [ ] TypeScript strict mode compilation
- [ ] Zero security vulnerabilities (npm audit)
- [ ] Compatibility with Node.js 16, 18, 20 LTS

### Documentation

- [ ] API reference complete (TypeDoc)
- [ ] User guide complete
- [ ] 15+ code examples
- [ ] Migration guide from pg/mysql2
- [ ] Troubleshooting guide
- [ ] TypeScript usage guide

### Packaging

- [ ] NPM package published
- [ ] Source code on GitHub
- [ ] Automated CI/CD (GitHub Actions)
- [ ] Semantic versioning
- [ ] Changelog maintained
- [ ] Security policy documented

---

## Community and Support

### Package Locations

- NPM: https://www.npmjs.com/package/scratchbird
- GitHub: https://github.com/yourusername/scratchbird-node
- Documentation: https://scratchbird-node.readthedocs.io/
- Examples: https://github.com/yourusername/scratchbird-node/tree/main/examples

### Issue Tracking

- GitHub Issues: https://github.com/yourusername/scratchbird-node/issues
- Response time target: <3 days
- Bug fix SLA: Critical <7 days, High <14 days
- Security issues: Email security@scratchbird.dev

### Community Channels

- Discord: ScratchBird #nodejs-driver
- Stack Overflow: Tag `scratchbird-node` or `scratchbird`
- Twitter: @ScratchBirdDB

---

## References

1. **node-postgres (pg)** - Reference implementation
   https://node-postgres.com/

2. **Prisma Documentation**
   https://www.prisma.io/docs

3. **TypeScript Handbook**
   https://www.typescriptlang.org/docs/handbook/

4. **Node.js Database Driver Best Practices**
   https://nodejs.org/en/docs/guides/

5. **PostgreSQL Wire Protocol 3.0** (if compatible)
   https://www.postgresql.org/docs/current/protocol.html

---

## Next Steps

1. **Create detailed SPECIFICATION.md** with complete API design
2. **Develop IMPLEMENTATION_PLAN.md** with milestones and timeline
3. **Write TESTING_CRITERIA.md** with comprehensive test requirements
4. **Begin prototype** with basic connection and query
5. **Set up CI/CD** for automated testing and NPM publishing
6. **Create Prisma adapter** (critical for adoption)

---

**Document Version:** 1.0 (Template)
**Last Updated:** 2026-01-03
**Status:** Draft
**Assigned To:** TBD
