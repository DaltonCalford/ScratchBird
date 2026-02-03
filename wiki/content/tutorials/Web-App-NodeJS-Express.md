# Node.js Express API

**Last Updated:** 2026-02-03

---

## Goal

Build a small REST API using Node.js and a PostgreSQL‑compatible client.

---

## Step 1: Schema

```sql
CREATE SCHEMA app;
CREATE TABLE app.todos (
    id INTEGER PRIMARY KEY,
    title VARCHAR(200) NOT NULL,
    done BOOLEAN DEFAULT FALSE
);
```

---

## Step 2: Express App (Example)

```js
const express = require('express');
const { Client } = require('pg');

const app = express();
app.use(express.json());

const db = new Client({
  host: 'localhost',
  port: 5432,
  user: 'SYSARCH',
  password: 'ScratchBirdBeta1!',
  database: 'app'
});

db.connect();

app.get('/todos', async (req, res) => {
  const result = await db.query('SELECT id, title, done FROM app.todos ORDER BY id');
  res.json(result.rows);
});

app.post('/todos', async (req, res) => {
  const { id, title, done } = req.body;
  await db.query(
    'INSERT INTO app.todos (id, title, done) VALUES ($1, $2, $3)',
    [id, title, !!done]
  );
  res.status(201).json({ ok: true });
});

app.put('/todos/:id', async (req, res) => {
  const { title, done } = req.body;
  await db.query(
    'UPDATE app.todos SET title = $1, done = $2 WHERE id = $3',
    [title, !!done, req.params.id]
  );
  res.json({ ok: true });
});

app.delete('/todos/:id', async (req, res) => {
  await db.query('DELETE FROM app.todos WHERE id = $1', [req.params.id]);
  res.json({ ok: true });
});

app.listen(3000);
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
