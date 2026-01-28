# First Application

**Last Updated:** 2026-01-28

---

## Overview

This tutorial guides you through building your first application with ScratchBird. You'll create a simple task management system from scratch, covering database design, CRUD operations, and basic querying patterns.

**What you'll learn:**
- Starting the ScratchBird server
- Connecting with the native client
- Designing a simple schema
- Performing CRUD operations
- Writing useful queries

**Time:** 30-45 minutes

---

## Prerequisites

Before starting, ensure you have:

1. ScratchBird installed ([Installation Guide](../Getting-Started.md))
2. Server running and accessible
3. Basic SQL knowledge helpful but not required

---

## Part 1: Start the Server

### Check Server Status

```bash
# Check if server is already running
pgrep sb_server

# Or check systemd status (Linux)
sudo systemctl status scratchbird
```

### Start the Server

**Linux (systemd):**
```bash
sudo systemctl start scratchbird
```

**macOS (launchd):**
```bash
sudo launchctl load /Library/LaunchDaemons/com.scratchbird.server.plist
```

**Foreground (development):**
```bash
sb_server -F --config /etc/scratchbird/sb_server.conf
```

**Docker:**
```bash
docker run -d --name scratchbird \
    -p 3092:3092 -p 5432:5432 \
    -v scratchbird_data:/var/lib/scratchbird \
    scratchbird/scratchbird:latest
```

### Verify Server is Running

```bash
# Check listening ports
ss -tlnp | grep -E '3092|5432'

# Expected output:
# LISTEN 0 128 *:3092 *:* users:(("sb_server",...))
# LISTEN 0 128 *:5432 *:* users:(("sb_server",...))
```

---

## Part 2: Connect to the Database

### Using the Native Client (sb_isql)

```bash
sb_isql -H localhost -p 3092 -U admin -d scratchbird
```

You should see:
```
Password: ********
Connected to ScratchBird 1.0.0
Type "help" for help.

scratchbird=>
```

### Alternative: Using psql

```bash
psql -h localhost -p 5432 -U admin -d scratchbird
```

### Test the Connection

```sql
-- Check version
SELECT version();

-- Expected output:
--                   version
-- --------------------------------------------
--  ScratchBird 1.0.0 on Linux x86_64
-- (1 row)

-- Simple calculation
SELECT 1 + 1 AS result;
```

---

## Part 3: Create Your Database

For this tutorial, we'll create a dedicated database for our task manager.

### Create the Database

```sql
-- Create a new database
CREATE DATABASE taskmanager;

-- Verify it was created
SELECT * FROM sb_catalog.databases;
```

### Connect to the New Database

In `sb_isql`:
```
\c taskmanager
```

Or reconnect:
```bash
sb_isql -H localhost -p 3092 -U admin -d taskmanager
```

---

## Part 4: Design the Schema

Our task manager will have three tables:
- `users` - People who use the system
- `projects` - Groups of related tasks
- `tasks` - Individual work items

### Create the Users Table

```sql
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    email VARCHAR(100) NOT NULL UNIQUE,
    full_name VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Add a comment for documentation
COMMENT ON TABLE users IS 'Application users';
```

### Create the Projects Table

```sql
CREATE TABLE projects (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    owner_id INTEGER NOT NULL REFERENCES users(id),
    status VARCHAR(20) DEFAULT 'active' CHECK (status IN ('active', 'archived', 'completed')),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Index for faster lookups by owner
CREATE INDEX idx_projects_owner ON projects(owner_id);
```

### Create the Tasks Table

```sql
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

-- Indexes for common queries
CREATE INDEX idx_tasks_project ON tasks(project_id);
CREATE INDEX idx_tasks_assigned ON tasks(assigned_to);
CREATE INDEX idx_tasks_status ON tasks(status);
CREATE INDEX idx_tasks_due_date ON tasks(due_date);
```

### Verify the Schema

```sql
-- List all tables
\dt

-- Describe each table
\d users
\d projects
\d tasks
```

---

## Part 5: Insert Sample Data

### Add Users

```sql
INSERT INTO users (username, email, full_name) VALUES
    ('alice', 'alice@example.com', 'Alice Johnson'),
    ('bob', 'bob@example.com', 'Bob Smith'),
    ('carol', 'carol@example.com', 'Carol Williams');

-- Verify
SELECT * FROM users;
```

Expected output:
```
 id | username |       email        |   full_name    |     created_at
----+----------+--------------------+----------------+---------------------
  1 | alice    | alice@example.com  | Alice Johnson  | 2026-01-18 10:30:00
  2 | bob      | bob@example.com    | Bob Smith      | 2026-01-18 10:30:00
  3 | carol    | carol@example.com  | Carol Williams | 2026-01-18 10:30:00
(3 rows)
```

### Create Projects

```sql
INSERT INTO projects (name, description, owner_id) VALUES
    ('Website Redesign', 'Modernize the company website', 1),
    ('Mobile App', 'Build iOS and Android app', 2),
    ('Documentation', 'Update all technical docs', 1);

-- Verify
SELECT id, name, owner_id, status FROM projects;
```

### Add Tasks

```sql
INSERT INTO tasks (title, description, project_id, assigned_to, priority, due_date) VALUES
    -- Website Redesign tasks
    ('Design mockups', 'Create wireframes for homepage', 1, 1, 1, '2026-02-01'),
    ('Implement navigation', 'Build responsive nav component', 1, 2, 2, '2026-02-15'),
    ('Write content', 'Update About and Contact pages', 1, 3, 3, '2026-02-20'),

    -- Mobile App tasks
    ('Setup project', 'Initialize React Native project', 2, 2, 1, '2026-01-25'),
    ('Design UI', 'Create component library', 2, 1, 2, '2026-02-10'),
    ('Implement auth', 'Add login and registration', 2, 2, 1, '2026-02-28'),

    -- Documentation tasks
    ('Audit existing docs', 'Review all current documentation', 3, 3, 2, '2026-01-30'),
    ('Update API docs', 'Document all endpoints', 3, 1, 1, '2026-02-15'),
    ('Add tutorials', 'Write getting started guides', 3, 3, 3, '2026-03-01');

-- Verify
SELECT id, title, project_id, priority, status FROM tasks;
```

---

## Part 6: Query Your Data

### Basic Queries

**Find all tasks assigned to Alice:**
```sql
SELECT t.id, t.title, t.status, t.due_date
FROM tasks t
JOIN users u ON t.assigned_to = u.id
WHERE u.username = 'alice'
ORDER BY t.due_date;
```

**Count tasks by status:**
```sql
SELECT status, COUNT(*) AS count
FROM tasks
GROUP BY status
ORDER BY count DESC;
```

**Find overdue tasks:**
```sql
SELECT t.title, t.due_date, p.name AS project
FROM tasks t
JOIN projects p ON t.project_id = p.id
WHERE t.due_date < CURRENT_DATE
  AND t.status NOT IN ('completed', 'cancelled')
ORDER BY t.due_date;
```

### Join Queries

**Show all tasks with user and project names:**
```sql
SELECT
    t.id,
    t.title,
    p.name AS project,
    u.full_name AS assigned_to,
    t.priority,
    t.status,
    t.due_date
FROM tasks t
JOIN projects p ON t.project_id = p.id
LEFT JOIN users u ON t.assigned_to = u.id
ORDER BY t.priority, t.due_date;
```

### Aggregate Queries

**Project summary with task counts:**
```sql
SELECT
    p.name AS project,
    u.full_name AS owner,
    COUNT(t.id) AS total_tasks,
    COUNT(CASE WHEN t.status = 'completed' THEN 1 END) AS completed,
    COUNT(CASE WHEN t.status = 'pending' THEN 1 END) AS pending,
    COUNT(CASE WHEN t.status = 'in_progress' THEN 1 END) AS in_progress
FROM projects p
JOIN users u ON p.owner_id = u.id
LEFT JOIN tasks t ON p.id = t.project_id
GROUP BY p.id, p.name, u.full_name
ORDER BY p.name;
```

**User workload:**
```sql
SELECT
    u.full_name,
    COUNT(t.id) AS assigned_tasks,
    COUNT(CASE WHEN t.status = 'pending' THEN 1 END) AS pending,
    COUNT(CASE WHEN t.due_date < CURRENT_DATE AND t.status NOT IN ('completed', 'cancelled') THEN 1 END) AS overdue
FROM users u
LEFT JOIN tasks t ON u.id = t.assigned_to
GROUP BY u.id, u.full_name
ORDER BY assigned_tasks DESC;
```

---

## Part 7: Update and Delete

### Update Task Status

```sql
-- Start working on a task
UPDATE tasks
SET status = 'in_progress'
WHERE id = 1
RETURNING id, title, status;

-- Complete a task
UPDATE tasks
SET status = 'completed',
    completed_at = CURRENT_TIMESTAMP
WHERE id = 1
RETURNING *;
```

### Bulk Updates

```sql
-- Mark all high priority tasks as in_progress
UPDATE tasks
SET status = 'in_progress'
WHERE priority = 1
  AND status = 'pending'
RETURNING id, title;
```

### Delete Records

```sql
-- Delete a cancelled task
DELETE FROM tasks WHERE id = 9;

-- Delete with confirmation
DELETE FROM tasks
WHERE status = 'cancelled'
RETURNING id, title;
```

---

## Part 8: Transactions

Use transactions when making multiple related changes.

### Example: Reassign All Tasks

```sql
BEGIN;

-- Find the user IDs
SELECT id, username FROM users WHERE username IN ('bob', 'carol');

-- Reassign Bob's tasks to Carol
UPDATE tasks
SET assigned_to = 3  -- Carol's ID
WHERE assigned_to = 2;  -- Bob's ID

-- Verify before committing
SELECT t.title, u.username AS assigned_to
FROM tasks t
LEFT JOIN users u ON t.assigned_to = u.id;

-- If looks good:
COMMIT;

-- Or if something is wrong:
-- ROLLBACK;
```

### Example: Create Project with Initial Task

```sql
BEGIN;

-- Create new project
INSERT INTO projects (name, description, owner_id)
VALUES ('Q1 Planning', 'Quarterly planning tasks', 1)
RETURNING id;

-- Use the returned ID (let's say it's 4)
INSERT INTO tasks (title, project_id, assigned_to, priority, due_date)
VALUES ('Define Q1 goals', 4, 1, 1, '2026-01-31');

COMMIT;
```

---

## Part 9: Useful Commands

### sb_isql Commands

| Command | Description |
|---------|-------------|
| `\l` | List databases |
| `\c dbname` | Connect to database |
| `\dt` | List tables |
| `\d tablename` | Describe table |
| `\di` | List indexes |
| `\timing` | Toggle query timing |
| `\x` | Toggle expanded output |
| `\q` | Quit |

### Enable Query Timing

```sql
\timing on

SELECT COUNT(*) FROM tasks;
-- Time: 0.5 ms
```

### Export Query Results

```bash
# Export to CSV
sb_isql -H localhost -p 3092 -U admin -d taskmanager \
    -c "COPY (SELECT * FROM tasks) TO STDOUT WITH CSV HEADER" > tasks.csv

# Run script from file
sb_isql -H localhost -p 3092 -U admin -d taskmanager -f queries.sql
```

---

## Part 10: Clean Up (Optional)

If you want to start fresh or remove the tutorial data:

```sql
-- Connect to different database first
\c scratchbird

-- Drop the tutorial database
DROP DATABASE taskmanager;
```

---

## Complete Schema Reference

Here's the complete schema for reference:

```sql
-- Users table
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    email VARCHAR(100) NOT NULL UNIQUE,
    full_name VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Projects table
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

-- Tasks table
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
CREATE INDEX idx_tasks_due_date ON tasks(due_date);
```

---

## Next Steps

Now that you've built your first application, continue learning:

- **Build a web app:** [Python Flask Tutorial](Web-App-Python-Flask.md) or [Node.js Express Tutorial](Web-App-NodeJS-Express.md)
- **Learn more SQL:** [Basic SQL Guide](../getting-started/basic-sql.md)
- **Explore features:** [Language Guides](../language-guides/README.md)
- **Deploy to production:** [Docker Deployment](Docker-Deployment.md)

---

## Troubleshooting

### "Connection refused"
Server isn't running. Start it with `systemctl start scratchbird` or run in foreground mode.

### "Database does not exist"
Create the database first with `CREATE DATABASE taskmanager;`

### "Permission denied"
Check your username and password. Default admin user is `admin`.

### "Relation does not exist"
Make sure you're connected to the correct database (`\c taskmanager`).

