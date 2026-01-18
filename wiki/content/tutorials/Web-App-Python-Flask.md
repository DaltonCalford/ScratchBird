# Web App: Python + Flask

**Status:** Alpha documentation
**Last Updated:** 2026-01-18

---

## Overview

This tutorial guides you through building a complete REST API with Python and Flask, backed by ScratchBird. You'll create a task management API with proper connection handling, error management, and best practices.

**What you'll build:**
- A RESTful API for managing tasks
- Database connection pooling
- Input validation and error handling
- Proper transaction management

**Time:** 60-90 minutes

---

## Prerequisites

- Python 3.8 or later
- ScratchBird running and accessible
- Basic Python and Flask knowledge

---

## Part 1: Project Setup

### Create Project Directory

```bash
mkdir flask-taskapi
cd flask-taskapi
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
```

### Install Dependencies

```bash
pip install flask psycopg2-binary python-dotenv
```

Create `requirements.txt`:
```txt
flask>=2.3.0
psycopg2-binary>=2.9.0
python-dotenv>=1.0.0
```

### Project Structure

```
flask-taskapi/
├── app/
│   ├── __init__.py
│   ├── config.py
│   ├── database.py
│   ├── models.py
│   └── routes/
│       ├── __init__.py
│       ├── tasks.py
│       └── projects.py
├── .env
├── .env.example
├── requirements.txt
└── run.py
```

---

## Part 2: Configuration

### Environment Variables

Create `.env.example`:
```env
# Database Configuration
DB_HOST=localhost
DB_PORT=5432
DB_NAME=taskmanager
DB_USER=admin
DB_PASSWORD=secret

# Flask Configuration
FLASK_ENV=development
FLASK_DEBUG=1
SECRET_KEY=your-secret-key-here
```

Copy to `.env` and update with your values:
```bash
cp .env.example .env
```

### Configuration Module

Create `app/config.py`:
```python
import os
from dotenv import load_dotenv

load_dotenv()

class Config:
    """Application configuration."""

    # Flask settings
    SECRET_KEY = os.getenv('SECRET_KEY', 'dev-key-change-in-production')
    DEBUG = os.getenv('FLASK_DEBUG', '0') == '1'

    # Database settings
    DB_HOST = os.getenv('DB_HOST', 'localhost')
    DB_PORT = int(os.getenv('DB_PORT', '5432'))
    DB_NAME = os.getenv('DB_NAME', 'taskmanager')
    DB_USER = os.getenv('DB_USER', 'admin')
    DB_PASSWORD = os.getenv('DB_PASSWORD', '')

    # Connection pool settings
    DB_MIN_CONNECTIONS = int(os.getenv('DB_MIN_CONNECTIONS', '2'))
    DB_MAX_CONNECTIONS = int(os.getenv('DB_MAX_CONNECTIONS', '10'))

    @classmethod
    def get_db_config(cls):
        """Return database configuration dictionary."""
        return {
            'host': cls.DB_HOST,
            'port': cls.DB_PORT,
            'dbname': cls.DB_NAME,
            'user': cls.DB_USER,
            'password': cls.DB_PASSWORD,
        }
```

---

## Part 3: Database Layer

### Connection Pool

Create `app/database.py`:
```python
import psycopg2
from psycopg2 import pool, extras
from contextlib import contextmanager
from flask import g, current_app

# Global connection pool
_connection_pool = None

def init_db(app):
    """Initialize the database connection pool."""
    global _connection_pool

    config = app.config

    _connection_pool = pool.ThreadedConnectionPool(
        minconn=config.get('DB_MIN_CONNECTIONS', 2),
        maxconn=config.get('DB_MAX_CONNECTIONS', 10),
        host=config['DB_HOST'],
        port=config['DB_PORT'],
        dbname=config['DB_NAME'],
        user=config['DB_USER'],
        password=config['DB_PASSWORD']
    )

    app.logger.info(f"Database pool initialized: {config['DB_HOST']}:{config['DB_PORT']}/{config['DB_NAME']}")

def close_db(e=None):
    """Close database connection for current request."""
    db = g.pop('db', None)
    if db is not None:
        _connection_pool.putconn(db)

def get_db():
    """Get a database connection for the current request."""
    if 'db' not in g:
        g.db = _connection_pool.getconn()
    return g.db

@contextmanager
def get_cursor(commit=True):
    """Context manager for database cursor with automatic commit/rollback."""
    conn = get_db()
    cursor = conn.cursor(cursor_factory=extras.RealDictCursor)
    try:
        yield cursor
        if commit:
            conn.commit()
    except Exception as e:
        conn.rollback()
        raise
    finally:
        cursor.close()

def execute_query(query, params=None, fetch_one=False, fetch_all=True):
    """Execute a query and return results."""
    with get_cursor(commit=False) as cursor:
        cursor.execute(query, params)
        if fetch_one:
            return dict(cursor.fetchone()) if cursor.rowcount > 0 else None
        elif fetch_all:
            return [dict(row) for row in cursor.fetchall()]
        return cursor.rowcount

def execute_write(query, params=None, returning=False):
    """Execute a write query (INSERT, UPDATE, DELETE)."""
    with get_cursor(commit=True) as cursor:
        cursor.execute(query, params)
        if returning:
            return dict(cursor.fetchone()) if cursor.rowcount > 0 else None
        return cursor.rowcount
```

---

## Part 4: Models

### Task and Project Models

Create `app/models.py`:
```python
from app.database import execute_query, execute_write, get_cursor

class Task:
    """Task model with database operations."""

    @staticmethod
    def get_all(project_id=None, status=None, assigned_to=None, limit=100, offset=0):
        """Get all tasks with optional filters."""
        query = """
            SELECT t.*,
                   p.name AS project_name,
                   u.full_name AS assignee_name
            FROM tasks t
            JOIN projects p ON t.project_id = p.id
            LEFT JOIN users u ON t.assigned_to = u.id
            WHERE 1=1
        """
        params = []

        if project_id:
            query += " AND t.project_id = %s"
            params.append(project_id)

        if status:
            query += " AND t.status = %s"
            params.append(status)

        if assigned_to:
            query += " AND t.assigned_to = %s"
            params.append(assigned_to)

        query += " ORDER BY t.priority, t.due_date LIMIT %s OFFSET %s"
        params.extend([limit, offset])

        return execute_query(query, params)

    @staticmethod
    def get_by_id(task_id):
        """Get a single task by ID."""
        query = """
            SELECT t.*,
                   p.name AS project_name,
                   u.full_name AS assignee_name
            FROM tasks t
            JOIN projects p ON t.project_id = p.id
            LEFT JOIN users u ON t.assigned_to = u.id
            WHERE t.id = %s
        """
        return execute_query(query, [task_id], fetch_one=True)

    @staticmethod
    def create(title, project_id, description=None, assigned_to=None,
               priority=3, due_date=None):
        """Create a new task."""
        query = """
            INSERT INTO tasks (title, description, project_id, assigned_to, priority, due_date)
            VALUES (%s, %s, %s, %s, %s, %s)
            RETURNING *
        """
        params = [title, description, project_id, assigned_to, priority, due_date]
        return execute_write(query, params, returning=True)

    @staticmethod
    def update(task_id, **kwargs):
        """Update a task with given fields."""
        allowed_fields = ['title', 'description', 'assigned_to', 'priority',
                         'status', 'due_date']

        updates = []
        params = []

        for field, value in kwargs.items():
            if field in allowed_fields:
                updates.append(f"{field} = %s")
                params.append(value)

        if not updates:
            return None

        # Handle status change to completed
        if kwargs.get('status') == 'completed':
            updates.append("completed_at = CURRENT_TIMESTAMP")

        params.append(task_id)

        query = f"""
            UPDATE tasks
            SET {', '.join(updates)}
            WHERE id = %s
            RETURNING *
        """
        return execute_write(query, params, returning=True)

    @staticmethod
    def delete(task_id):
        """Delete a task."""
        query = "DELETE FROM tasks WHERE id = %s RETURNING id"
        return execute_write(query, [task_id], returning=True)

    @staticmethod
    def get_stats():
        """Get task statistics."""
        query = """
            SELECT
                COUNT(*) AS total,
                COUNT(CASE WHEN status = 'pending' THEN 1 END) AS pending,
                COUNT(CASE WHEN status = 'in_progress' THEN 1 END) AS in_progress,
                COUNT(CASE WHEN status = 'completed' THEN 1 END) AS completed,
                COUNT(CASE WHEN due_date < CURRENT_DATE AND status NOT IN ('completed', 'cancelled') THEN 1 END) AS overdue
            FROM tasks
        """
        return execute_query(query, fetch_one=True)


class Project:
    """Project model with database operations."""

    @staticmethod
    def get_all(status=None, owner_id=None):
        """Get all projects with optional filters."""
        query = """
            SELECT p.*,
                   u.full_name AS owner_name,
                   COUNT(t.id) AS task_count
            FROM projects p
            JOIN users u ON p.owner_id = u.id
            LEFT JOIN tasks t ON p.id = t.project_id
            WHERE 1=1
        """
        params = []

        if status:
            query += " AND p.status = %s"
            params.append(status)

        if owner_id:
            query += " AND p.owner_id = %s"
            params.append(owner_id)

        query += " GROUP BY p.id, u.full_name ORDER BY p.created_at DESC"

        return execute_query(query, params)

    @staticmethod
    def get_by_id(project_id):
        """Get a single project by ID with task summary."""
        query = """
            SELECT p.*,
                   u.full_name AS owner_name,
                   COUNT(t.id) AS total_tasks,
                   COUNT(CASE WHEN t.status = 'completed' THEN 1 END) AS completed_tasks
            FROM projects p
            JOIN users u ON p.owner_id = u.id
            LEFT JOIN tasks t ON p.id = t.project_id
            WHERE p.id = %s
            GROUP BY p.id, u.full_name
        """
        return execute_query(query, [project_id], fetch_one=True)

    @staticmethod
    def create(name, owner_id, description=None):
        """Create a new project."""
        query = """
            INSERT INTO projects (name, description, owner_id)
            VALUES (%s, %s, %s)
            RETURNING *
        """
        return execute_write(query, [name, description, owner_id], returning=True)

    @staticmethod
    def update(project_id, **kwargs):
        """Update a project."""
        allowed_fields = ['name', 'description', 'status']

        updates = ["updated_at = CURRENT_TIMESTAMP"]
        params = []

        for field, value in kwargs.items():
            if field in allowed_fields:
                updates.append(f"{field} = %s")
                params.append(value)

        if len(updates) == 1:  # Only timestamp update
            return None

        params.append(project_id)

        query = f"""
            UPDATE projects
            SET {', '.join(updates)}
            WHERE id = %s
            RETURNING *
        """
        return execute_write(query, params, returning=True)

    @staticmethod
    def delete(project_id):
        """Delete a project (cascades to tasks)."""
        query = "DELETE FROM projects WHERE id = %s RETURNING id"
        return execute_write(query, [project_id], returning=True)


class User:
    """User model for lookups."""

    @staticmethod
    def get_all():
        """Get all users."""
        query = "SELECT id, username, email, full_name FROM users ORDER BY username"
        return execute_query(query)

    @staticmethod
    def get_by_id(user_id):
        """Get user by ID."""
        query = "SELECT id, username, email, full_name FROM users WHERE id = %s"
        return execute_query(query, [user_id], fetch_one=True)
```

---

## Part 5: API Routes

### Tasks API

Create `app/routes/__init__.py`:
```python
# Routes package
```

Create `app/routes/tasks.py`:
```python
from flask import Blueprint, request, jsonify
from app.models import Task

tasks_bp = Blueprint('tasks', __name__, url_prefix='/api/tasks')

@tasks_bp.route('', methods=['GET'])
def get_tasks():
    """Get all tasks with optional filters."""
    project_id = request.args.get('project_id', type=int)
    status = request.args.get('status')
    assigned_to = request.args.get('assigned_to', type=int)
    limit = request.args.get('limit', 100, type=int)
    offset = request.args.get('offset', 0, type=int)

    tasks = Task.get_all(
        project_id=project_id,
        status=status,
        assigned_to=assigned_to,
        limit=min(limit, 1000),  # Cap at 1000
        offset=offset
    )

    return jsonify({
        'tasks': tasks,
        'count': len(tasks),
        'limit': limit,
        'offset': offset
    })

@tasks_bp.route('/<int:task_id>', methods=['GET'])
def get_task(task_id):
    """Get a single task by ID."""
    task = Task.get_by_id(task_id)

    if not task:
        return jsonify({'error': 'Task not found'}), 404

    return jsonify(task)

@tasks_bp.route('', methods=['POST'])
def create_task():
    """Create a new task."""
    data = request.get_json()

    # Validate required fields
    if not data.get('title'):
        return jsonify({'error': 'Title is required'}), 400

    if not data.get('project_id'):
        return jsonify({'error': 'Project ID is required'}), 400

    # Validate priority
    priority = data.get('priority', 3)
    if not 1 <= priority <= 5:
        return jsonify({'error': 'Priority must be between 1 and 5'}), 400

    try:
        task = Task.create(
            title=data['title'],
            project_id=data['project_id'],
            description=data.get('description'),
            assigned_to=data.get('assigned_to'),
            priority=priority,
            due_date=data.get('due_date')
        )
        return jsonify(task), 201
    except Exception as e:
        return jsonify({'error': str(e)}), 400

@tasks_bp.route('/<int:task_id>', methods=['PUT', 'PATCH'])
def update_task(task_id):
    """Update a task."""
    data = request.get_json()

    # Validate status if provided
    valid_statuses = ['pending', 'in_progress', 'completed', 'cancelled']
    if 'status' in data and data['status'] not in valid_statuses:
        return jsonify({'error': f'Invalid status. Must be one of: {valid_statuses}'}), 400

    # Validate priority if provided
    if 'priority' in data:
        if not 1 <= data['priority'] <= 5:
            return jsonify({'error': 'Priority must be between 1 and 5'}), 400

    task = Task.update(task_id, **data)

    if not task:
        return jsonify({'error': 'Task not found or no changes made'}), 404

    return jsonify(task)

@tasks_bp.route('/<int:task_id>', methods=['DELETE'])
def delete_task(task_id):
    """Delete a task."""
    result = Task.delete(task_id)

    if not result:
        return jsonify({'error': 'Task not found'}), 404

    return jsonify({'message': 'Task deleted', 'id': task_id})

@tasks_bp.route('/stats', methods=['GET'])
def get_stats():
    """Get task statistics."""
    stats = Task.get_stats()
    return jsonify(stats)
```

### Projects API

Create `app/routes/projects.py`:
```python
from flask import Blueprint, request, jsonify
from app.models import Project, Task

projects_bp = Blueprint('projects', __name__, url_prefix='/api/projects')

@projects_bp.route('', methods=['GET'])
def get_projects():
    """Get all projects."""
    status = request.args.get('status')
    owner_id = request.args.get('owner_id', type=int)

    projects = Project.get_all(status=status, owner_id=owner_id)

    return jsonify({
        'projects': projects,
        'count': len(projects)
    })

@projects_bp.route('/<int:project_id>', methods=['GET'])
def get_project(project_id):
    """Get a single project by ID."""
    project = Project.get_by_id(project_id)

    if not project:
        return jsonify({'error': 'Project not found'}), 404

    return jsonify(project)

@projects_bp.route('', methods=['POST'])
def create_project():
    """Create a new project."""
    data = request.get_json()

    if not data.get('name'):
        return jsonify({'error': 'Name is required'}), 400

    if not data.get('owner_id'):
        return jsonify({'error': 'Owner ID is required'}), 400

    try:
        project = Project.create(
            name=data['name'],
            owner_id=data['owner_id'],
            description=data.get('description')
        )
        return jsonify(project), 201
    except Exception as e:
        return jsonify({'error': str(e)}), 400

@projects_bp.route('/<int:project_id>', methods=['PUT', 'PATCH'])
def update_project(project_id):
    """Update a project."""
    data = request.get_json()

    valid_statuses = ['active', 'archived', 'completed']
    if 'status' in data and data['status'] not in valid_statuses:
        return jsonify({'error': f'Invalid status. Must be one of: {valid_statuses}'}), 400

    project = Project.update(project_id, **data)

    if not project:
        return jsonify({'error': 'Project not found or no changes made'}), 404

    return jsonify(project)

@projects_bp.route('/<int:project_id>', methods=['DELETE'])
def delete_project(project_id):
    """Delete a project (and all its tasks)."""
    result = Project.delete(project_id)

    if not result:
        return jsonify({'error': 'Project not found'}), 404

    return jsonify({'message': 'Project deleted', 'id': project_id})

@projects_bp.route('/<int:project_id>/tasks', methods=['GET'])
def get_project_tasks(project_id):
    """Get all tasks for a project."""
    project = Project.get_by_id(project_id)
    if not project:
        return jsonify({'error': 'Project not found'}), 404

    tasks = Task.get_all(project_id=project_id)

    return jsonify({
        'project': project,
        'tasks': tasks,
        'count': len(tasks)
    })
```

---

## Part 6: Application Factory

### Initialize the App

Create `app/__init__.py`:
```python
from flask import Flask, jsonify
from app.config import Config
from app.database import init_db, close_db

def create_app(config_class=Config):
    """Application factory."""
    app = Flask(__name__)

    # Load configuration
    app.config.from_object(config_class)
    app.config.update(config_class.get_db_config())

    # Initialize database
    init_db(app)

    # Register teardown
    app.teardown_appcontext(close_db)

    # Register blueprints
    from app.routes.tasks import tasks_bp
    from app.routes.projects import projects_bp

    app.register_blueprint(tasks_bp)
    app.register_blueprint(projects_bp)

    # Health check endpoint
    @app.route('/health')
    def health():
        return jsonify({'status': 'healthy'})

    # Error handlers
    @app.errorhandler(404)
    def not_found(e):
        return jsonify({'error': 'Not found'}), 404

    @app.errorhandler(500)
    def server_error(e):
        app.logger.error(f'Server error: {e}')
        return jsonify({'error': 'Internal server error'}), 500

    return app
```

### Run Script

Create `run.py`:
```python
from app import create_app

app = create_app()

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
```

---

## Part 7: Database Setup

Before running the API, create the database schema:

```bash
# Connect to ScratchBird
sb_isql -H localhost -p 3092 -U admin -d scratchbird

# Create database
CREATE DATABASE taskmanager;
\c taskmanager
```

Then create the tables (from [First Application](First-Application.md)):

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

## Part 8: Run and Test

### Start the API

```bash
# Activate virtual environment
source venv/bin/activate

# Run the application
python run.py
```

You should see:
```
 * Running on http://0.0.0.0:5000
```

### Test with curl

**Health check:**
```bash
curl http://localhost:5000/health
```

**Create a project:**
```bash
curl -X POST http://localhost:5000/api/projects \
    -H "Content-Type: application/json" \
    -d '{"name": "Test Project", "description": "A test project", "owner_id": 1}'
```

**Create a task:**
```bash
curl -X POST http://localhost:5000/api/tasks \
    -H "Content-Type: application/json" \
    -d '{"title": "First task", "project_id": 1, "priority": 1}'
```

**Get all tasks:**
```bash
curl http://localhost:5000/api/tasks
```

**Get tasks filtered by status:**
```bash
curl "http://localhost:5000/api/tasks?status=pending"
```

**Update a task:**
```bash
curl -X PATCH http://localhost:5000/api/tasks/1 \
    -H "Content-Type: application/json" \
    -d '{"status": "in_progress"}'
```

**Get task statistics:**
```bash
curl http://localhost:5000/api/tasks/stats
```

**Delete a task:**
```bash
curl -X DELETE http://localhost:5000/api/tasks/1
```

---

## Part 9: Production Considerations

### Use Gunicorn

For production, use Gunicorn instead of Flask's development server:

```bash
pip install gunicorn
gunicorn -w 4 -b 0.0.0.0:5000 "app:create_app()"
```

### Docker Deployment

Create `Dockerfile`:
```dockerfile
FROM python:3.11-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
RUN pip install gunicorn

COPY . .

EXPOSE 5000

CMD ["gunicorn", "-w", "4", "-b", "0.0.0.0:5000", "app:create_app()"]
```

Create `docker-compose.yml`:
```yaml
version: '3.8'

services:
  api:
    build: .
    ports:
      - "5000:5000"
    environment:
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

Run with:
```bash
docker compose up -d
```

### Add Logging

Update `app/__init__.py`:
```python
import logging
from logging.handlers import RotatingFileHandler

def create_app(config_class=Config):
    app = Flask(__name__)

    # ... existing code ...

    # Configure logging
    if not app.debug:
        handler = RotatingFileHandler('app.log', maxBytes=10240, backupCount=10)
        handler.setFormatter(logging.Formatter(
            '%(asctime)s %(levelname)s: %(message)s [in %(pathname)s:%(lineno)d]'
        ))
        handler.setLevel(logging.INFO)
        app.logger.addHandler(handler)
        app.logger.setLevel(logging.INFO)
        app.logger.info('TaskAPI startup')

    return app
```

---

## Complete File List

```
flask-taskapi/
├── app/
│   ├── __init__.py      # Application factory
│   ├── config.py        # Configuration
│   ├── database.py      # Database connection pool
│   ├── models.py        # Data models
│   └── routes/
│       ├── __init__.py
│       ├── tasks.py     # Tasks API
│       └── projects.py  # Projects API
├── .env                 # Environment variables
├── .env.example         # Example environment file
├── requirements.txt     # Python dependencies
├── run.py              # Development runner
├── Dockerfile          # Docker build
└── docker-compose.yml  # Docker Compose config
```

---

## API Reference

### Tasks

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/tasks` | List all tasks |
| GET | `/api/tasks/:id` | Get a single task |
| POST | `/api/tasks` | Create a task |
| PUT/PATCH | `/api/tasks/:id` | Update a task |
| DELETE | `/api/tasks/:id` | Delete a task |
| GET | `/api/tasks/stats` | Get task statistics |

### Projects

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/projects` | List all projects |
| GET | `/api/projects/:id` | Get a single project |
| POST | `/api/projects` | Create a project |
| PUT/PATCH | `/api/projects/:id` | Update a project |
| DELETE | `/api/projects/:id` | Delete a project |
| GET | `/api/projects/:id/tasks` | Get project tasks |

---

## Next Steps

- Add authentication with JWT or OAuth
- Implement input validation with marshmallow or pydantic
- Add pagination with cursor-based pagination
- Implement rate limiting
- Add API documentation with Swagger/OpenAPI

---

## See Also

- [Python Driver Documentation](../drivers/Python.md)
- [REST API Design](REST-API-Design.md)
- [Docker Deployment](Docker-Deployment.md)
- [First Application](First-Application.md)

