[Back to Drivers](Driver-Comparison.md) | [Back to Home](../Home.md)

# Node.js / TypeScript Driver Guide

**Status:** Alpha documentation
**Last Updated:** 2026-01-20

---

## Overview

Node.js applications can connect to ScratchBird through multiple protocols:

| Protocol | Port | Library | Best For |
|----------|------|---------|----------|
| PostgreSQL | 5432 | pg, postgres.js | Most Node.js apps |
| MySQL | 3306 | mysql2, mysql | MySQL migrations |
| Firebird | 3050 | node-firebird | Firebird migrations |
| Native | 3092 | (future) scratchbird | Full feature access |

**Recommendation:** Use **pg** (node-postgres) or **postgres.js** via the PostgreSQL protocol for the best ecosystem compatibility.

---

## Quick Start

### Installation

```bash
# Recommended: pg (node-postgres)
npm install pg
npm install --save-dev @types/pg  # For TypeScript

# Or postgres.js (modern, zero dependencies)
npm install postgres

# Or mysql2 (MySQL protocol)
npm install mysql2

# Or Prisma ORM
npm install prisma @prisma/client
```

### Basic Connection (JavaScript)

```javascript
const { Client } = require('pg');

const client = new Client({
  host: 'localhost',
  port: 5432,
  database: 'scratchbird',
  user: 'app_user',
  password: 'secret'
});

async function main() {
  await client.connect();

  const result = await client.query('SELECT version()');
  console.log(result.rows[0]);

  await client.end();
}

main().catch(console.error);
```

### Basic Connection (TypeScript)

```typescript
import { Client } from 'pg';

const client = new Client({
  host: 'localhost',
  port: 5432,
  database: 'scratchbird',
  user: 'app_user',
  password: 'secret'
});

async function main(): Promise<void> {
  await client.connect();

  const result = await client.query('SELECT version()');
  console.log(result.rows[0]);

  await client.end();
}

main().catch(console.error);
```

---

## Connection Methods

### pg (node-postgres)

The most popular PostgreSQL client for Node.js.

```typescript
import { Client, Pool } from 'pg';

// Single client connection
const client = new Client({
  host: 'localhost',
  port: 5432,
  database: 'scratchbird',
  user: 'app_user',
  password: 'secret'
});

// Connection string
const client2 = new Client({
  connectionString: 'postgresql://app_user:secret@localhost:5432/scratchbird'
});

// With SSL
const client3 = new Client({
  host: 'localhost',
  port: 5432,
  database: 'scratchbird',
  user: 'app_user',
  password: 'secret',
  ssl: {
    rejectUnauthorized: false  // For self-signed certs
  }
});

// Environment variables (automatic)
// Set PGHOST, PGPORT, PGDATABASE, PGUSER, PGPASSWORD
const client4 = new Client();  // Uses env vars
```

### postgres.js (Modern Alternative)

Zero-dependency, fast PostgreSQL client.

```typescript
import postgres from 'postgres';

// Basic connection
const sql = postgres({
  host: 'localhost',
  port: 5432,
  database: 'scratchbird',
  username: 'app_user',
  password: 'secret'
});

// Connection string
const sql2 = postgres('postgresql://app_user:secret@localhost:5432/scratchbird');

// With options
const sql3 = postgres({
  host: 'localhost',
  port: 5432,
  database: 'scratchbird',
  username: 'app_user',
  password: 'secret',
  max: 10,           // Connection pool size
  idle_timeout: 20,  // Seconds before closing idle connections
  connect_timeout: 10
});
```

### mysql2

```typescript
import mysql from 'mysql2/promise';

// Create connection
const conn = await mysql.createConnection({
  host: 'localhost',
  port: 3306,
  database: 'scratchbird',
  user: 'app_user',
  password: 'secret'
});

// Execute query
const [rows] = await conn.execute('SELECT * FROM users');
console.log(rows);

await conn.end();
```

### node-firebird

```javascript
const Firebird = require('node-firebird');

const options = {
  host: 'localhost',
  port: 3050,
  database: 'scratchbird',
  user: 'SYSDBA',
  password: 'masterkey'
};

Firebird.attach(options, (err, db) => {
  if (err) throw err;

  db.query('SELECT * FROM users', (err, result) => {
    console.log(result);
    db.detach();
  });
});
```

---

## CRUD Operations

### Create (INSERT)

```typescript
import { Pool } from 'pg';

const pool = new Pool({
  connectionString: 'postgresql://localhost:5432/scratchbird'
});

// Single insert with parameters
const result = await pool.query(
  'INSERT INTO users (username, email) VALUES ($1, $2) RETURNING id',
  ['john_doe', 'john@example.com']
);
console.log('Inserted user ID:', result.rows[0].id);

// Insert with RETURNING
const { rows } = await pool.query(`
  INSERT INTO users (username, email, created_at)
  VALUES ($1, $2, NOW())
  RETURNING id, username, created_at
`, ['jane_doe', 'jane@example.com']);
console.log('Created:', rows[0]);

// Batch insert
const users = [
  ['user1', 'user1@example.com'],
  ['user2', 'user2@example.com'],
  ['user3', 'user3@example.com']
];

for (const [username, email] of users) {
  await pool.query(
    'INSERT INTO users (username, email) VALUES ($1, $2)',
    [username, email]
  );
}
```

### Read (SELECT)

```typescript
import { Pool, QueryResult } from 'pg';

const pool = new Pool({
  connectionString: 'postgresql://localhost:5432/scratchbird'
});

// Basic query
const result = await pool.query('SELECT * FROM users');
console.log(result.rows);

// Query with parameters
const { rows } = await pool.query(
  'SELECT * FROM users WHERE id = $1',
  [1]
);
console.log(rows[0]);

// Query with multiple parameters
const users = await pool.query(
  'SELECT * FROM users WHERE status = $1 AND role = $2 ORDER BY $3 LIMIT $4',
  ['active', 'admin', 'created_at', 10]
);

// TypeScript with typed results
interface User {
  id: number;
  username: string;
  email: string;
  created_at: Date;
}

const typedResult = await pool.query<User>(
  'SELECT id, username, email, created_at FROM users'
);
typedResult.rows.forEach(user => {
  console.log(user.username, user.email);
});
```

### Update

```typescript
import { Pool } from 'pg';

const pool = new Pool({
  connectionString: 'postgresql://localhost:5432/scratchbird'
});

// Simple update
const result = await pool.query(
  'UPDATE users SET email = $1 WHERE id = $2',
  ['newemail@example.com', 1]
);
console.log('Rows updated:', result.rowCount);

// Update with RETURNING
const { rows } = await pool.query(`
  UPDATE users
  SET last_login = NOW(), login_count = login_count + 1
  WHERE id = $1
  RETURNING id, last_login, login_count
`, [1]);
console.log('Updated user:', rows[0]);

// Conditional update
const updateResult = await pool.query(`
  UPDATE products
  SET price = price * 1.10
  WHERE category = $1 AND updated_at < $2
`, ['electronics', '2024-01-01']);
console.log(`Updated ${updateResult.rowCount} products`);
```

### Delete

```typescript
import { Pool } from 'pg';

const pool = new Pool({
  connectionString: 'postgresql://localhost:5432/scratchbird'
});

// Simple delete
const result = await pool.query(
  'DELETE FROM sessions WHERE user_id = $1',
  [1]
);
console.log('Deleted:', result.rowCount);

// Delete with RETURNING
const { rows } = await pool.query(`
  DELETE FROM audit_logs
  WHERE created_at < NOW() - INTERVAL '1 year'
  RETURNING id, action
`);
console.log(`Deleted ${rows.length} old logs`);

// Delete with subquery
await pool.query(`
  DELETE FROM order_items
  WHERE order_id IN (
    SELECT id FROM orders WHERE status = 'cancelled'
  )
`);
```

---

## Connection Pooling

### pg Pool

```typescript
import { Pool, PoolConfig } from 'pg';

const poolConfig: PoolConfig = {
  host: 'localhost',
  port: 5432,
  database: 'scratchbird',
  user: 'app_user',
  password: 'secret',
  max: 20,                    // Maximum connections
  min: 5,                     // Minimum connections
  idleTimeoutMillis: 30000,   // Close idle connections after 30s
  connectionTimeoutMillis: 5000,  // Timeout on new connections
  maxUses: 7500               // Close connection after N uses
};

const pool = new Pool(poolConfig);

// Use pool directly (recommended)
const result = await pool.query('SELECT * FROM users');

// Or acquire client for multiple queries
const client = await pool.connect();
try {
  await client.query('BEGIN');
  await client.query('INSERT INTO users (username) VALUES ($1)', ['test']);
  await client.query('COMMIT');
} catch (err) {
  await client.query('ROLLBACK');
  throw err;
} finally {
  client.release();  // Return to pool
}

// Pool events
pool.on('connect', (client) => {
  console.log('New client connected');
});

pool.on('error', (err, client) => {
  console.error('Pool error:', err);
});

// Shutdown
await pool.end();
```

### postgres.js Connection Pool

```typescript
import postgres from 'postgres';

const sql = postgres({
  host: 'localhost',
  port: 5432,
  database: 'scratchbird',
  username: 'app_user',
  password: 'secret',
  max: 10,              // Pool size
  idle_timeout: 20,     // Seconds
  connect_timeout: 10,
  max_lifetime: 60 * 30 // 30 minutes
});

// Queries automatically use pooled connections
const users = await sql`SELECT * FROM users`;

// Graceful shutdown
await sql.end();
```

### mysql2 Pool

```typescript
import mysql from 'mysql2/promise';

const pool = mysql.createPool({
  host: 'localhost',
  port: 3306,
  database: 'scratchbird',
  user: 'app_user',
  password: 'secret',
  waitForConnections: true,
  connectionLimit: 10,
  queueLimit: 0
});

// Use pool directly
const [rows] = await pool.execute('SELECT * FROM users');

// Or get connection
const conn = await pool.getConnection();
try {
  await conn.beginTransaction();
  await conn.execute('INSERT INTO users (username) VALUES (?)', ['test']);
  await conn.commit();
} catch (err) {
  await conn.rollback();
  throw err;
} finally {
  conn.release();
}

await pool.end();
```

---

## Transactions

### Manual Transactions

```typescript
import { Pool } from 'pg';

const pool = new Pool({
  connectionString: 'postgresql://localhost:5432/scratchbird'
});

const client = await pool.connect();

try {
  await client.query('BEGIN');

  // Transfer funds
  await client.query(
    'UPDATE accounts SET balance = balance - $1 WHERE id = $2',
    [100, 1]
  );
  await client.query(
    'UPDATE accounts SET balance = balance + $1 WHERE id = $2',
    [100, 2]
  );

  // Record transfer
  await client.query(
    'INSERT INTO transfers (from_id, to_id, amount) VALUES ($1, $2, $3)',
    [1, 2, 100]
  );

  await client.query('COMMIT');
  console.log('Transfer successful');

} catch (err) {
  await client.query('ROLLBACK');
  console.error('Transfer failed:', err);
  throw err;

} finally {
  client.release();
}
```

### Transaction Helper Function

```typescript
import { Pool, PoolClient } from 'pg';

const pool = new Pool({
  connectionString: 'postgresql://localhost:5432/scratchbird'
});

async function withTransaction<T>(
  callback: (client: PoolClient) => Promise<T>
): Promise<T> {
  const client = await pool.connect();
  try {
    await client.query('BEGIN');
    const result = await callback(client);
    await client.query('COMMIT');
    return result;
  } catch (err) {
    await client.query('ROLLBACK');
    throw err;
  } finally {
    client.release();
  }
}

// Usage
const result = await withTransaction(async (client) => {
  const { rows: [user] } = await client.query(
    'INSERT INTO users (username) VALUES ($1) RETURNING *',
    ['newuser']
  );

  await client.query(
    'INSERT INTO profiles (user_id) VALUES ($1)',
    [user.id]
  );

  return user;
});
```

### postgres.js Transactions

```typescript
import postgres from 'postgres';

const sql = postgres('postgresql://localhost:5432/scratchbird');

// Transaction block
const [user] = await sql.begin(async (sql) => {
  const [user] = await sql`
    INSERT INTO users (username, email)
    VALUES ('newuser', 'new@example.com')
    RETURNING *
  `;

  await sql`
    INSERT INTO profiles (user_id, bio)
    VALUES (${user.id}, 'New user profile')
  `;

  return [user];
});

// Savepoints
await sql.begin(async (sql) => {
  await sql`INSERT INTO orders (customer_id) VALUES (1)`;

  await sql.savepoint(async (sql) => {
    await sql`INSERT INTO order_items (order_id, product_id) VALUES (1, 999)`;
  }).catch(() => {
    console.log('Item insert failed, order preserved');
  });
});
```

---

## Prepared Statements

### pg Prepared Statements

```typescript
import { Pool } from 'pg';

const pool = new Pool({
  connectionString: 'postgresql://localhost:5432/scratchbird'
});

// Named prepared statement
const result = await pool.query({
  name: 'get-user-by-id',
  text: 'SELECT * FROM users WHERE id = $1',
  values: [1]
});

// Reuse (automatically cached)
for (let id = 1; id <= 100; id++) {
  const { rows } = await pool.query({
    name: 'get-user-by-id',
    text: 'SELECT * FROM users WHERE id = $1',
    values: [id]
  });
}
```

### postgres.js Tagged Templates

```typescript
import postgres from 'postgres';

const sql = postgres('postgresql://localhost:5432/scratchbird');

// Tagged template literals (automatically parameterized)
const username = 'john';
const users = await sql`SELECT * FROM users WHERE username = ${username}`;

// Multiple parameters
const minAge = 18;
const status = 'active';
const adults = await sql`
  SELECT * FROM users
  WHERE age >= ${minAge}
  AND status = ${status}
`;

// Dynamic columns (use sql.unsafe carefully)
const column = 'email';
const results = await sql`
  SELECT id, ${sql(column)} FROM users
`;
```

---

## Error Handling

```typescript
import { Pool, DatabaseError } from 'pg';

const pool = new Pool({
  connectionString: 'postgresql://localhost:5432/scratchbird'
});

try {
  await pool.query(
    'INSERT INTO users (username, email) VALUES ($1, $2)',
    ['duplicate', 'dup@example.com']
  );
} catch (err) {
  if (err instanceof DatabaseError) {
    switch (err.code) {
      case '23505':  // unique_violation
        console.error('Duplicate entry:', err.detail);
        break;
      case '23503':  // foreign_key_violation
        console.error('Foreign key violation:', err.constraint);
        break;
      case '23502':  // not_null_violation
        console.error('NULL value not allowed:', err.column);
        break;
      case '23514':  // check_violation
        console.error('Check constraint failed:', err.constraint);
        break;
      case '42P01':  // undefined_table
        console.error('Table does not exist:', err.table);
        break;
      default:
        console.error('Database error:', err.code, err.message);
    }
  } else {
    console.error('Unexpected error:', err);
  }
}
```

---

## TypeScript Types

### Define Types

```typescript
// types/database.ts
export interface User {
  id: number;
  username: string;
  email: string;
  password_hash: string;
  created_at: Date;
  updated_at: Date;
}

export interface Product {
  id: number;
  name: string;
  price: number;
  category: string;
  stock: number;
}

export interface Order {
  id: number;
  user_id: number;
  total: number;
  status: 'pending' | 'processing' | 'shipped' | 'delivered';
  created_at: Date;
}
```

### Use with pg

```typescript
import { Pool, QueryResult } from 'pg';
import { User, Product } from './types/database';

const pool = new Pool({
  connectionString: 'postgresql://localhost:5432/scratchbird'
});

// Typed query
async function getUserById(id: number): Promise<User | null> {
  const result = await pool.query<User>(
    'SELECT * FROM users WHERE id = $1',
    [id]
  );
  return result.rows[0] || null;
}

// Typed insert
async function createUser(
  username: string,
  email: string
): Promise<User> {
  const result = await pool.query<User>(
    `INSERT INTO users (username, email)
     VALUES ($1, $2)
     RETURNING *`,
    [username, email]
  );
  return result.rows[0];
}

// Generic query helper
async function query<T>(
  text: string,
  params?: unknown[]
): Promise<T[]> {
  const result = await pool.query<T>(text, params);
  return result.rows;
}

// Usage
const users = await query<User>('SELECT * FROM users WHERE active = $1', [true]);
```

### Use with postgres.js

```typescript
import postgres from 'postgres';

interface User {
  id: number;
  username: string;
  email: string;
}

const sql = postgres('postgresql://localhost:5432/scratchbird');

// Typed queries
const users = await sql<User[]>`SELECT * FROM users`;

// Single row
const [user] = await sql<[User?]>`
  SELECT * FROM users WHERE id = ${1}
`;

// Insert with return
const [newUser] = await sql<[User]>`
  INSERT INTO users (username, email)
  VALUES (${'newuser'}, ${'new@example.com'})
  RETURNING *
`;
```

---

## Framework Integration

### Express.js

```typescript
import express, { Request, Response, NextFunction } from 'express';
import { Pool } from 'pg';

const app = express();
app.use(express.json());

const pool = new Pool({
  connectionString: process.env.DATABASE_URL
});

// Middleware to attach db to request
app.use((req: Request, res: Response, next: NextFunction) => {
  req.db = pool;
  next();
});

// Routes
app.get('/users', async (req: Request, res: Response) => {
  const { rows } = await req.db.query('SELECT id, username, email FROM users');
  res.json(rows);
});

app.get('/users/:id', async (req: Request, res: Response) => {
  const { rows } = await req.db.query(
    'SELECT * FROM users WHERE id = $1',
    [req.params.id]
  );
  if (rows.length === 0) {
    return res.status(404).json({ error: 'User not found' });
  }
  res.json(rows[0]);
});

app.post('/users', async (req: Request, res: Response) => {
  const { username, email } = req.body;
  const { rows } = await req.db.query(
    'INSERT INTO users (username, email) VALUES ($1, $2) RETURNING *',
    [username, email]
  );
  res.status(201).json(rows[0]);
});

// Error handler
app.use((err: Error, req: Request, res: Response, next: NextFunction) => {
  console.error(err);
  res.status(500).json({ error: 'Internal server error' });
});

app.listen(3000);
```

### Fastify

```typescript
import Fastify from 'fastify';
import { Pool } from 'pg';

const fastify = Fastify({ logger: true });

const pool = new Pool({
  connectionString: process.env.DATABASE_URL
});

// Decorate fastify with db
fastify.decorate('db', pool);

// Routes
fastify.get('/users', async (request, reply) => {
  const { rows } = await fastify.db.query('SELECT * FROM users');
  return rows;
});

fastify.get<{ Params: { id: string } }>(
  '/users/:id',
  async (request, reply) => {
    const { rows } = await fastify.db.query(
      'SELECT * FROM users WHERE id = $1',
      [request.params.id]
    );
    if (rows.length === 0) {
      reply.code(404);
      return { error: 'Not found' };
    }
    return rows[0];
  }
);

// Graceful shutdown
fastify.addHook('onClose', async () => {
  await pool.end();
});

fastify.listen({ port: 3000 });
```

### NestJS

```typescript
// database.module.ts
import { Module, Global } from '@nestjs/common';
import { Pool } from 'pg';

const poolProvider = {
  provide: 'DATABASE_POOL',
  useFactory: () => {
    return new Pool({
      connectionString: process.env.DATABASE_URL
    });
  }
};

@Global()
@Module({
  providers: [poolProvider],
  exports: [poolProvider]
})
export class DatabaseModule {}

// users.service.ts
import { Injectable, Inject } from '@nestjs/common';
import { Pool } from 'pg';

@Injectable()
export class UsersService {
  constructor(@Inject('DATABASE_POOL') private pool: Pool) {}

  async findAll() {
    const { rows } = await this.pool.query('SELECT * FROM users');
    return rows;
  }

  async findOne(id: number) {
    const { rows } = await this.pool.query(
      'SELECT * FROM users WHERE id = $1',
      [id]
    );
    return rows[0];
  }

  async create(username: string, email: string) {
    const { rows } = await this.pool.query(
      'INSERT INTO users (username, email) VALUES ($1, $2) RETURNING *',
      [username, email]
    );
    return rows[0];
  }
}
```

---

## Common Issues

### Issue: Connection Refused

```typescript
// Error: connect ECONNREFUSED

// Check: Is ScratchBird running?
// Check: Is the port correct?
// Check: Is the host accessible?

const pool = new Pool({
  host: 'localhost',  // or '127.0.0.1'
  port: 5432,
  connectionTimeout: 10000  // 10 second timeout
});
```

### Issue: SSL Required

```typescript
// Error: SSL required

const pool = new Pool({
  connectionString: 'postgresql://localhost:5432/scratchbird',
  ssl: {
    rejectUnauthorized: false  // For self-signed certs
  }
});

// Or with proper cert
const pool2 = new Pool({
  ssl: {
    ca: fs.readFileSync('/path/to/server-ca.pem'),
    key: fs.readFileSync('/path/to/client-key.pem'),
    cert: fs.readFileSync('/path/to/client-cert.pem')
  }
});
```

### Issue: Too Many Connections

```typescript
// Error: too many connections

// Use connection pooling with limits
const pool = new Pool({
  max: 10,  // Limit pool size
  min: 2
});

// Always release connections
const client = await pool.connect();
try {
  // ... use client
} finally {
  client.release();  // ALWAYS release!
}
```

### Issue: Query Timeout

```typescript
// Add query timeout
const pool = new Pool({
  connectionString: 'postgresql://localhost:5432/scratchbird',
  statement_timeout: 30000,  // 30 seconds
  query_timeout: 30000
});

// Or per-query timeout
const result = await pool.query({
  text: 'SELECT * FROM large_table',
  values: [],
  timeout: 10000  // 10 second timeout
});
```

---

## See Also

- [Driver Comparison](Driver-Comparison.md) - Compare all drivers
- [First Connection](../getting-started/first-connection.md) - Getting started guide
- [Connection Problems](../troubleshooting/Connection-Problems.md) - Troubleshooting
- [Web App Tutorial (Express)](../tutorials/Web-App-NodeJS-Express.md) - Full Express example

