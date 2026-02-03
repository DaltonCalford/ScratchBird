# Python Flask API

**Last Updated:** 2026-02-03

---

## Goal

Build a minimal REST API that reads and writes ScratchBird data.

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

## Step 2: Example Project Layout

```
app/
  app.py
  requirements.txt
```

---

## Step 3: Flask App (Example)

```python
from flask import Flask, request, jsonify
import psycopg  # Use a client compatible with your enabled listener

app = Flask(__name__)

conn = psycopg.connect(
    "dbname=app user=SYSARCH password=ScratchBirdBeta1! host=localhost port=5432"
)

@app.get("/todos")
def list_todos():
    with conn.cursor() as cur:
        cur.execute("SELECT id, title, done FROM app.todos ORDER BY id")
        rows = cur.fetchall()
    return jsonify([{"id": r[0], "title": r[1], "done": r[2]} for r in rows])

@app.post("/todos")
def create_todo():
    data = request.get_json()
    with conn.cursor() as cur:
        cur.execute(
            "INSERT INTO app.todos (id, title, done) VALUES (%s, %s, %s)",
            (data["id"], data["title"], bool(data.get("done", False)))
        )
        conn.commit()
    return {"ok": True}, 201

@app.put("/todos/<int:todo_id>")
def update_todo(todo_id):
    data = request.get_json()
    with conn.cursor() as cur:
        cur.execute(
            "UPDATE app.todos SET title = %s, done = %s WHERE id = %s",
            (data["title"], bool(data.get("done", False)), todo_id)
        )
        conn.commit()
    return {"ok": True}

@app.delete("/todos/<int:todo_id>")
def delete_todo(todo_id):
    with conn.cursor() as cur:
        cur.execute("DELETE FROM app.todos WHERE id = %s", (todo_id,))
        conn.commit()
    return {"ok": True}
```

---

## Step 4: Test

```bash
curl -X POST localhost:5000/todos   -H 'Content-Type: application/json'   -d '{"id":1,"title":"try scratchbird"}'

curl localhost:5000/todos
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
