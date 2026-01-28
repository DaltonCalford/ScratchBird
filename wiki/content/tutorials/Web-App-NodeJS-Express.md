# Web App: Node.js + Express

**Last Updated:** 2026-01-28

---

## Overview

This tutorial guides you through building a complete REST API with Node.js and Express, backed by ScratchBird. You'll create a task management API with proper connection handling, validation, and TypeScript support.

**What you'll build:**
- A RESTful API using Express
- Database connection pooling with pg
- Input validation with Zod
- Proper error handling middleware

**Time:** 60-90 minutes

---

## Prerequisites

- Node.js 18 or later
- ScratchBird running and accessible
- Basic JavaScript/TypeScript knowledge

---

## Part 1: Project Setup

### Initialize Project

```bash
mkdir express-taskapi
cd express-taskapi
npm init -y
```

### Install Dependencies

```bash
# Core dependencies
npm install express pg dotenv

# Development dependencies
npm install -D typescript @types/node @types/express @types/pg
npm install -D ts-node nodemon

# Validation (optional but recommended)
npm install zod
```

### TypeScript Configuration

Create `tsconfig.json`:
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "commonjs",
    "lib": ["ES2022"],
    "outDir": "./dist",
    "rootDir": "./src",
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "forceConsistentCasingInFileNames": true,
    "resolveJsonModule": true
  },
  "include": ["src/**/*"],
  "exclude": ["node_modules", "dist"]
}
```

### Update package.json

Add scripts to `package.json`:
```json
{
  "scripts": {
    "dev": "nodemon --exec ts-node src/index.ts",
    "build": "tsc",
    "start": "node dist/index.js"
  }
}
```

### Project Structure

```
express-taskapi/
├── src/
│   ├── index.ts
│   ├── config.ts
│   ├── db/
│   │   ├── index.ts
│   │   └── queries.ts
│   ├── routes/
│   │   ├── index.ts
│   │   ├── tasks.ts
│   │   └── projects.ts
│   ├── middleware/
│   │   ├── errorHandler.ts
│   │   └── validate.ts
│   └── types/
│       └── index.ts
├── .env
├── .env.example
├── package.json
└── tsconfig.json
```

---

## Part 2: Configuration

### Environment Variables

Create `.env.example`:
```env
# Server Configuration
PORT=3000
NODE_ENV=development

# Database Configuration
DB_HOST=localhost
DB_PORT=5432
DB_NAME=taskmanager
DB_USER=admin
DB_PASSWORD=secret

# Connection Pool
DB_POOL_MIN=2
DB_POOL_MAX=10
```

Copy and update:
```bash
cp .env.example .env
```

### Configuration Module

Create `src/config.ts`:
```typescript
import dotenv from 'dotenv';

dotenv.config();

export const config = {
  server: {
    port: parseInt(process.env.PORT || '3000', 10),
    nodeEnv: process.env.NODE_ENV || 'development',
  },
  database: {
    host: process.env.DB_HOST || 'localhost',
    port: parseInt(process.env.DB_PORT || '5432', 10),
    database: process.env.DB_NAME || 'taskmanager',
    user: process.env.DB_USER || 'admin',
    password: process.env.DB_PASSWORD || '',
    min: parseInt(process.env.DB_POOL_MIN || '2', 10),
    max: parseInt(process.env.DB_POOL_MAX || '10', 10),
  },
};
```

---

## Part 3: Types

Create `src/types/index.ts`:
```typescript
export interface User {
  id: number;
  username: string;
  email: string;
  full_name: string | null;
  created_at: Date;
}

export interface Project {
  id: number;
  name: string;
  description: string | null;
  owner_id: number;
  status: 'active' | 'archived' | 'completed';
  created_at: Date;
  updated_at: Date;
  // Joined fields
  owner_name?: string;
  task_count?: number;
}

export interface Task {
  id: number;
  title: string;
  description: string | null;
  project_id: number;
  assigned_to: number | null;
  priority: number;
  status: 'pending' | 'in_progress' | 'completed' | 'cancelled';
  due_date: Date | null;
  created_at: Date;
  completed_at: Date | null;
  // Joined fields
  project_name?: string;
  assignee_name?: string;
}

export interface TaskStats {
  total: number;
  pending: number;
  in_progress: number;
  completed: number;
  overdue: number;
}

export interface CreateTaskInput {
  title: string;
  project_id: number;
  description?: string;
  assigned_to?: number;
  priority?: number;
  due_date?: string;
}

export interface UpdateTaskInput {
  title?: string;
  description?: string;
  assigned_to?: number | null;
  priority?: number;
  status?: Task['status'];
  due_date?: string | null;
}

export interface CreateProjectInput {
  name: string;
  owner_id: number;
  description?: string;
}

export interface UpdateProjectInput {
  name?: string;
  description?: string;
  status?: Project['status'];
}
```

---

## Part 4: Database Layer

### Connection Pool

Create `src/db/index.ts`:
```typescript
import { Pool, PoolClient, QueryResult } from 'pg';
import { config } from '../config';

// Create connection pool
export const pool = new Pool({
  host: config.database.host,
  port: config.database.port,
  database: config.database.database,
  user: config.database.user,
  password: config.database.password,
  min: config.database.min,
  max: config.database.max,
});

// Log pool errors
pool.on('error', (err) => {
  console.error('Unexpected error on idle client', err);
});

// Query helper
export async function query<T>(
  text: string,
  params?: (string | number | boolean | null | Date)[]
): Promise<T[]> {
  const result = await pool.query(text, params);
  return result.rows;
}

// Single row query helper
export async function queryOne<T>(
  text: string,
  params?: (string | number | boolean | null | Date)[]
): Promise<T | null> {
  const result = await pool.query(text, params);
  return result.rows[0] || null;
}

// Transaction helper
export async function withTransaction<T>(
  callback: (client: PoolClient) => Promise<T>
): Promise<T> {
  const client = await pool.connect();
  try {
    await client.query('BEGIN');
    const result = await callback(client);
    await client.query('COMMIT');
    return result;
  } catch (error) {
    await client.query('ROLLBACK');
    throw error;
  } finally {
    client.release();
  }
}

// Graceful shutdown
export async function closePool(): Promise<void> {
  await pool.end();
}
```

### Database Queries

Create `src/db/queries.ts`:
```typescript
import { query, queryOne } from './index';
import {
  Task,
  Project,
  TaskStats,
  CreateTaskInput,
  UpdateTaskInput,
  CreateProjectInput,
  UpdateProjectInput,
} from '../types';

// ============ Task Queries ============

export async function getAllTasks(filters: {
  project_id?: number;
  status?: string;
  assigned_to?: number;
  limit?: number;
  offset?: number;
}): Promise<Task[]> {
  const { project_id, status, assigned_to, limit = 100, offset = 0 } = filters;

  let sql = `
    SELECT t.*,
           p.name AS project_name,
           u.full_name AS assignee_name
    FROM tasks t
    JOIN projects p ON t.project_id = p.id
    LEFT JOIN users u ON t.assigned_to = u.id
    WHERE 1=1
  `;
  const params: (string | number)[] = [];
  let paramIndex = 1;

  if (project_id) {
    sql += ` AND t.project_id = $${paramIndex++}`;
    params.push(project_id);
  }

  if (status) {
    sql += ` AND t.status = $${paramIndex++}`;
    params.push(status);
  }

  if (assigned_to) {
    sql += ` AND t.assigned_to = $${paramIndex++}`;
    params.push(assigned_to);
  }

  sql += ` ORDER BY t.priority, t.due_date LIMIT $${paramIndex++} OFFSET $${paramIndex}`;
  params.push(limit, offset);

  return query<Task>(sql, params);
}

export async function getTaskById(id: number): Promise<Task | null> {
  const sql = `
    SELECT t.*,
           p.name AS project_name,
           u.full_name AS assignee_name
    FROM tasks t
    JOIN projects p ON t.project_id = p.id
    LEFT JOIN users u ON t.assigned_to = u.id
    WHERE t.id = $1
  `;
  return queryOne<Task>(sql, [id]);
}

export async function createTask(input: CreateTaskInput): Promise<Task> {
  const sql = `
    INSERT INTO tasks (title, description, project_id, assigned_to, priority, due_date)
    VALUES ($1, $2, $3, $4, $5, $6)
    RETURNING *
  `;
  const params = [
    input.title,
    input.description || null,
    input.project_id,
    input.assigned_to || null,
    input.priority || 3,
    input.due_date || null,
  ];
  const result = await queryOne<Task>(sql, params);
  if (!result) throw new Error('Failed to create task');
  return result;
}

export async function updateTask(
  id: number,
  input: UpdateTaskInput
): Promise<Task | null> {
  const updates: string[] = [];
  const params: (string | number | null)[] = [];
  let paramIndex = 1;

  const allowedFields: (keyof UpdateTaskInput)[] = [
    'title',
    'description',
    'assigned_to',
    'priority',
    'status',
    'due_date',
  ];

  for (const field of allowedFields) {
    if (field in input) {
      updates.push(`${field} = $${paramIndex++}`);
      params.push(input[field] ?? null);
    }
  }

  if (updates.length === 0) return null;

  // Set completed_at when status changes to completed
  if (input.status === 'completed') {
    updates.push(`completed_at = CURRENT_TIMESTAMP`);
  }

  params.push(id);

  const sql = `
    UPDATE tasks
    SET ${updates.join(', ')}
    WHERE id = $${paramIndex}
    RETURNING *
  `;

  return queryOne<Task>(sql, params);
}

export async function deleteTask(id: number): Promise<boolean> {
  const sql = 'DELETE FROM tasks WHERE id = $1 RETURNING id';
  const result = await queryOne<{ id: number }>(sql, [id]);
  return result !== null;
}

export async function getTaskStats(): Promise<TaskStats> {
  const sql = `
    SELECT
      COUNT(*)::int AS total,
      COUNT(CASE WHEN status = 'pending' THEN 1 END)::int AS pending,
      COUNT(CASE WHEN status = 'in_progress' THEN 1 END)::int AS in_progress,
      COUNT(CASE WHEN status = 'completed' THEN 1 END)::int AS completed,
      COUNT(CASE WHEN due_date < CURRENT_DATE AND status NOT IN ('completed', 'cancelled') THEN 1 END)::int AS overdue
    FROM tasks
  `;
  const result = await queryOne<TaskStats>(sql);
  return result || { total: 0, pending: 0, in_progress: 0, completed: 0, overdue: 0 };
}

// ============ Project Queries ============

export async function getAllProjects(filters: {
  status?: string;
  owner_id?: number;
}): Promise<Project[]> {
  const { status, owner_id } = filters;

  let sql = `
    SELECT p.*,
           u.full_name AS owner_name,
           COUNT(t.id)::int AS task_count
    FROM projects p
    JOIN users u ON p.owner_id = u.id
    LEFT JOIN tasks t ON p.id = t.project_id
    WHERE 1=1
  `;
  const params: (string | number)[] = [];
  let paramIndex = 1;

  if (status) {
    sql += ` AND p.status = $${paramIndex++}`;
    params.push(status);
  }

  if (owner_id) {
    sql += ` AND p.owner_id = $${paramIndex++}`;
    params.push(owner_id);
  }

  sql += ' GROUP BY p.id, u.full_name ORDER BY p.created_at DESC';

  return query<Project>(sql, params);
}

export async function getProjectById(id: number): Promise<Project | null> {
  const sql = `
    SELECT p.*,
           u.full_name AS owner_name,
           COUNT(t.id)::int AS total_tasks,
           COUNT(CASE WHEN t.status = 'completed' THEN 1 END)::int AS completed_tasks
    FROM projects p
    JOIN users u ON p.owner_id = u.id
    LEFT JOIN tasks t ON p.id = t.project_id
    WHERE p.id = $1
    GROUP BY p.id, u.full_name
  `;
  return queryOne<Project>(sql, [id]);
}

export async function createProject(input: CreateProjectInput): Promise<Project> {
  const sql = `
    INSERT INTO projects (name, description, owner_id)
    VALUES ($1, $2, $3)
    RETURNING *
  `;
  const result = await queryOne<Project>(sql, [
    input.name,
    input.description || null,
    input.owner_id,
  ]);
  if (!result) throw new Error('Failed to create project');
  return result;
}

export async function updateProject(
  id: number,
  input: UpdateProjectInput
): Promise<Project | null> {
  const updates: string[] = ['updated_at = CURRENT_TIMESTAMP'];
  const params: (string | number | null)[] = [];
  let paramIndex = 1;

  const allowedFields: (keyof UpdateProjectInput)[] = ['name', 'description', 'status'];

  for (const field of allowedFields) {
    if (field in input) {
      updates.push(`${field} = $${paramIndex++}`);
      params.push(input[field] ?? null);
    }
  }

  if (updates.length === 1) return null; // Only timestamp update

  params.push(id);

  const sql = `
    UPDATE projects
    SET ${updates.join(', ')}
    WHERE id = $${paramIndex}
    RETURNING *
  `;

  return queryOne<Project>(sql, params);
}

export async function deleteProject(id: number): Promise<boolean> {
  const sql = 'DELETE FROM projects WHERE id = $1 RETURNING id';
  const result = await queryOne<{ id: number }>(sql, [id]);
  return result !== null;
}
```

---

## Part 5: Middleware

### Error Handler

Create `src/middleware/errorHandler.ts`:
```typescript
import { Request, Response, NextFunction } from 'express';

export class ApiError extends Error {
  constructor(
    public statusCode: number,
    message: string
  ) {
    super(message);
    this.name = 'ApiError';
  }
}

export function errorHandler(
  err: Error,
  req: Request,
  res: Response,
  next: NextFunction
): void {
  console.error('Error:', err);

  if (err instanceof ApiError) {
    res.status(err.statusCode).json({ error: err.message });
    return;
  }

  // Database errors
  if (err.message.includes('violates foreign key constraint')) {
    res.status(400).json({ error: 'Referenced record does not exist' });
    return;
  }

  if (err.message.includes('duplicate key value violates unique constraint')) {
    res.status(409).json({ error: 'Record already exists' });
    return;
  }

  // Generic error
  res.status(500).json({
    error: process.env.NODE_ENV === 'production'
      ? 'Internal server error'
      : err.message,
  });
}

export function notFound(req: Request, res: Response): void {
  res.status(404).json({ error: 'Not found' });
}
```

### Validation Middleware

Create `src/middleware/validate.ts`:
```typescript
import { Request, Response, NextFunction } from 'express';
import { z, ZodSchema } from 'zod';
import { ApiError } from './errorHandler';

export function validate(schema: ZodSchema) {
  return (req: Request, res: Response, next: NextFunction) => {
    try {
      schema.parse(req.body);
      next();
    } catch (error) {
      if (error instanceof z.ZodError) {
        const messages = error.errors.map((e) => `${e.path.join('.')}: ${e.message}`);
        next(new ApiError(400, messages.join(', ')));
      } else {
        next(error);
      }
    }
  };
}

// Validation schemas
export const createTaskSchema = z.object({
  title: z.string().min(1, 'Title is required').max(200),
  project_id: z.number().int().positive('Project ID is required'),
  description: z.string().max(5000).optional(),
  assigned_to: z.number().int().positive().optional(),
  priority: z.number().int().min(1).max(5).optional().default(3),
  due_date: z.string().date().optional(),
});

export const updateTaskSchema = z.object({
  title: z.string().min(1).max(200).optional(),
  description: z.string().max(5000).optional().nullable(),
  assigned_to: z.number().int().positive().optional().nullable(),
  priority: z.number().int().min(1).max(5).optional(),
  status: z.enum(['pending', 'in_progress', 'completed', 'cancelled']).optional(),
  due_date: z.string().date().optional().nullable(),
});

export const createProjectSchema = z.object({
  name: z.string().min(1, 'Name is required').max(100),
  owner_id: z.number().int().positive('Owner ID is required'),
  description: z.string().max(5000).optional(),
});

export const updateProjectSchema = z.object({
  name: z.string().min(1).max(100).optional(),
  description: z.string().max(5000).optional().nullable(),
  status: z.enum(['active', 'archived', 'completed']).optional(),
});
```

---

## Part 6: Routes

### Tasks Routes

Create `src/routes/tasks.ts`:
```typescript
import { Router, Request, Response, NextFunction } from 'express';
import * as db from '../db/queries';
import { ApiError } from '../middleware/errorHandler';
import { validate, createTaskSchema, updateTaskSchema } from '../middleware/validate';

const router = Router();

// GET /api/tasks
router.get('/', async (req: Request, res: Response, next: NextFunction) => {
  try {
    const project_id = req.query.project_id
      ? parseInt(req.query.project_id as string, 10)
      : undefined;
    const status = req.query.status as string | undefined;
    const assigned_to = req.query.assigned_to
      ? parseInt(req.query.assigned_to as string, 10)
      : undefined;
    const limit = Math.min(parseInt(req.query.limit as string, 10) || 100, 1000);
    const offset = parseInt(req.query.offset as string, 10) || 0;

    const tasks = await db.getAllTasks({
      project_id,
      status,
      assigned_to,
      limit,
      offset,
    });

    res.json({
      tasks,
      count: tasks.length,
      limit,
      offset,
    });
  } catch (error) {
    next(error);
  }
});

// GET /api/tasks/stats
router.get('/stats', async (req: Request, res: Response, next: NextFunction) => {
  try {
    const stats = await db.getTaskStats();
    res.json(stats);
  } catch (error) {
    next(error);
  }
});

// GET /api/tasks/:id
router.get('/:id', async (req: Request, res: Response, next: NextFunction) => {
  try {
    const id = parseInt(req.params.id, 10);
    const task = await db.getTaskById(id);

    if (!task) {
      throw new ApiError(404, 'Task not found');
    }

    res.json(task);
  } catch (error) {
    next(error);
  }
});

// POST /api/tasks
router.post(
  '/',
  validate(createTaskSchema),
  async (req: Request, res: Response, next: NextFunction) => {
    try {
      const task = await db.createTask(req.body);
      res.status(201).json(task);
    } catch (error) {
      next(error);
    }
  }
);

// PATCH /api/tasks/:id
router.patch(
  '/:id',
  validate(updateTaskSchema),
  async (req: Request, res: Response, next: NextFunction) => {
    try {
      const id = parseInt(req.params.id, 10);
      const task = await db.updateTask(id, req.body);

      if (!task) {
        throw new ApiError(404, 'Task not found or no changes made');
      }

      res.json(task);
    } catch (error) {
      next(error);
    }
  }
);

// DELETE /api/tasks/:id
router.delete('/:id', async (req: Request, res: Response, next: NextFunction) => {
  try {
    const id = parseInt(req.params.id, 10);
    const deleted = await db.deleteTask(id);

    if (!deleted) {
      throw new ApiError(404, 'Task not found');
    }

    res.json({ message: 'Task deleted', id });
  } catch (error) {
    next(error);
  }
});

export default router;
```

### Projects Routes

Create `src/routes/projects.ts`:
```typescript
import { Router, Request, Response, NextFunction } from 'express';
import * as db from '../db/queries';
import { ApiError } from '../middleware/errorHandler';
import { validate, createProjectSchema, updateProjectSchema } from '../middleware/validate';

const router = Router();

// GET /api/projects
router.get('/', async (req: Request, res: Response, next: NextFunction) => {
  try {
    const status = req.query.status as string | undefined;
    const owner_id = req.query.owner_id
      ? parseInt(req.query.owner_id as string, 10)
      : undefined;

    const projects = await db.getAllProjects({ status, owner_id });

    res.json({
      projects,
      count: projects.length,
    });
  } catch (error) {
    next(error);
  }
});

// GET /api/projects/:id
router.get('/:id', async (req: Request, res: Response, next: NextFunction) => {
  try {
    const id = parseInt(req.params.id, 10);
    const project = await db.getProjectById(id);

    if (!project) {
      throw new ApiError(404, 'Project not found');
    }

    res.json(project);
  } catch (error) {
    next(error);
  }
});

// POST /api/projects
router.post(
  '/',
  validate(createProjectSchema),
  async (req: Request, res: Response, next: NextFunction) => {
    try {
      const project = await db.createProject(req.body);
      res.status(201).json(project);
    } catch (error) {
      next(error);
    }
  }
);

// PATCH /api/projects/:id
router.patch(
  '/:id',
  validate(updateProjectSchema),
  async (req: Request, res: Response, next: NextFunction) => {
    try {
      const id = parseInt(req.params.id, 10);
      const project = await db.updateProject(id, req.body);

      if (!project) {
        throw new ApiError(404, 'Project not found or no changes made');
      }

      res.json(project);
    } catch (error) {
      next(error);
    }
  }
);

// DELETE /api/projects/:id
router.delete('/:id', async (req: Request, res: Response, next: NextFunction) => {
  try {
    const id = parseInt(req.params.id, 10);
    const deleted = await db.deleteProject(id);

    if (!deleted) {
      throw new ApiError(404, 'Project not found');
    }

    res.json({ message: 'Project deleted', id });
  } catch (error) {
    next(error);
  }
});

// GET /api/projects/:id/tasks
router.get('/:id/tasks', async (req: Request, res: Response, next: NextFunction) => {
  try {
    const id = parseInt(req.params.id, 10);
    const project = await db.getProjectById(id);

    if (!project) {
      throw new ApiError(404, 'Project not found');
    }

    const tasks = await db.getAllTasks({ project_id: id });

    res.json({
      project,
      tasks,
      count: tasks.length,
    });
  } catch (error) {
    next(error);
  }
});

export default router;
```

### Routes Index

Create `src/routes/index.ts`:
```typescript
import { Router } from 'express';
import tasksRouter from './tasks';
import projectsRouter from './projects';

const router = Router();

router.use('/tasks', tasksRouter);
router.use('/projects', projectsRouter);

export default router;
```

---

## Part 7: Application Entry Point

Create `src/index.ts`:
```typescript
import express from 'express';
import { config } from './config';
import { closePool } from './db';
import routes from './routes';
import { errorHandler, notFound } from './middleware/errorHandler';

const app = express();

// Middleware
app.use(express.json());

// Health check
app.get('/health', (req, res) => {
  res.json({ status: 'healthy' });
});

// API routes
app.use('/api', routes);

// Error handling
app.use(notFound);
app.use(errorHandler);

// Start server
const server = app.listen(config.server.port, () => {
  console.log(`Server running on port ${config.server.port}`);
  console.log(`Environment: ${config.server.nodeEnv}`);
});

// Graceful shutdown
const shutdown = async () => {
  console.log('Shutting down...');
  server.close(async () => {
    await closePool();
    console.log('Server stopped');
    process.exit(0);
  });
};

process.on('SIGTERM', shutdown);
process.on('SIGINT', shutdown);
```

---

## Part 8: Database Setup

Before running the API, create the database schema (if not already done):

```bash
# Connect to ScratchBird
sb_isql -H localhost -p 3092 -U admin -d scratchbird

# Create database
CREATE DATABASE taskmanager;
\c taskmanager
```

Create tables:
```sql
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    email VARCHAR(100) NOT NULL UNIQUE,
    full_name VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE projects (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    owner_id INTEGER NOT NULL REFERENCES users(id),
    status VARCHAR(20) DEFAULT 'active' CHECK (status IN ('active', 'archived', 'completed')),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_projects_owner ON projects(owner_id);

CREATE TABLE tasks (
    id SERIAL PRIMARY KEY,
    title VARCHAR(200) NOT NULL,
    description TEXT,
    project_id INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    assigned_to INTEGER REFERENCES users(id),
    priority INTEGER DEFAULT 3 CHECK (priority BETWEEN 1 AND 5),
    status VARCHAR(20) DEFAULT 'pending' CHECK (status IN ('pending', 'in_progress', 'completed', 'cancelled')),
    due_date DATE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    completed_at TIMESTAMP
);

CREATE INDEX idx_tasks_project ON tasks(project_id);
CREATE INDEX idx_tasks_assigned ON tasks(assigned_to);
CREATE INDEX idx_tasks_status ON tasks(status);

-- Insert a test user
INSERT INTO users (username, email, full_name) VALUES ('admin', 'admin@example.com', 'Administrator');
```

---

## Part 9: Run and Test

### Start the API

```bash
# Development mode with hot reload
npm run dev

# Or build and run
npm run build
npm start
```

### Test with curl

**Health check:**
```bash
curl http://localhost:3000/health
```

**Create a project:**
```bash
curl -X POST http://localhost:3000/api/projects \
    -H "Content-Type: application/json" \
    -d '{"name": "Test Project", "description": "A test project", "owner_id": 1}'
```

**Create a task:**
```bash
curl -X POST http://localhost:3000/api/tasks \
    -H "Content-Type: application/json" \
    -d '{"title": "First task", "project_id": 1, "priority": 1}'
```

**Get all tasks:**
```bash
curl http://localhost:3000/api/tasks
```

**Get tasks filtered by status:**
```bash
curl "http://localhost:3000/api/tasks?status=pending"
```

**Update a task:**
```bash
curl -X PATCH http://localhost:3000/api/tasks/1 \
    -H "Content-Type: application/json" \
    -d '{"status": "in_progress"}'
```

**Get task statistics:**
```bash
curl http://localhost:3000/api/tasks/stats
```

**Delete a task:**
```bash
curl -X DELETE http://localhost:3000/api/tasks/1
```

---

## Part 10: Production Deployment

### Docker Setup

Create `Dockerfile`:
```dockerfile
FROM node:20-alpine AS builder

WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY . .
RUN npm run build

FROM node:20-alpine

WORKDIR /app
COPY package*.json ./
RUN npm ci --production
COPY --from=builder /app/dist ./dist

EXPOSE 3000

USER node

CMD ["node", "dist/index.js"]
```

Create `docker-compose.yml`:
```yaml
version: '3.8'

services:
  api:
    build: .
    ports:
      - "3000:3000"
    environment:
      - NODE_ENV=production
      - DB_HOST=scratchbird
      - DB_PORT=5432
      - DB_NAME=taskmanager
      - DB_USER=admin
      - DB_PASSWORD=secret
    depends_on:
      - scratchbird

  scratchbird:
    image: scratchbird/scratchbird:latest
    ports:
      - "5432:5432"
    volumes:
      - scratchbird_data:/var/lib/scratchbird

volumes:
  scratchbird_data:
```

Run:
```bash
docker compose up -d
```

### PM2 Process Manager

For non-Docker deployments:

```bash
npm install -g pm2

# Start application
pm2 start dist/index.js --name taskapi

# Enable startup script
pm2 startup
pm2 save

# Monitor
pm2 monit
```

---

## Complete File List

```
express-taskapi/
├── src/
│   ├── index.ts           # Application entry point
│   ├── config.ts          # Configuration
│   ├── db/
│   │   ├── index.ts       # Connection pool
│   │   └── queries.ts     # Database queries
│   ├── routes/
│   │   ├── index.ts       # Routes aggregator
│   │   ├── tasks.ts       # Tasks API
│   │   └── projects.ts    # Projects API
│   ├── middleware/
│   │   ├── errorHandler.ts # Error handling
│   │   └── validate.ts    # Input validation
│   └── types/
│       └── index.ts       # TypeScript types
├── .env                   # Environment variables
├── .env.example           # Example environment
├── package.json           # Dependencies
├── tsconfig.json          # TypeScript config
├── Dockerfile             # Docker build
└── docker-compose.yml     # Docker Compose
```

---

## API Reference

### Tasks

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/tasks` | List all tasks |
| GET | `/api/tasks/:id` | Get a single task |
| POST | `/api/tasks` | Create a task |
| PATCH | `/api/tasks/:id` | Update a task |
| DELETE | `/api/tasks/:id` | Delete a task |
| GET | `/api/tasks/stats` | Get task statistics |

### Projects

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/projects` | List all projects |
| GET | `/api/projects/:id` | Get a single project |
| POST | `/api/projects` | Create a project |
| PATCH | `/api/projects/:id` | Update a project |
| DELETE | `/api/projects/:id` | Delete a project |
| GET | `/api/projects/:id/tasks` | Get project tasks |

---

## Next Steps

- Add authentication with Passport.js or JWT
- Implement WebSocket support for real-time updates
- Add caching with Redis
- Implement rate limiting with express-rate-limit
- Add OpenAPI/Swagger documentation

---

## See Also

- [Node.js Driver Documentation](../drivers/NodeJS.md)
- [REST API Design](REST-API-Design.md)
- [Docker Deployment](Docker-Deployment.md)
- [Python Flask Tutorial](Web-App-Python-Flask.md)

