# C# / .NET Driver Guide

**Status:** Complete
**Last Updated:** 2026-01-20

---

## Overview

ScratchBird supports multiple connection protocols for C# and .NET applications:

| Protocol | Port | Driver | Best For |
|----------|------|--------|----------|
| PostgreSQL | 5432 | Npgsql | Most applications (recommended) |
| MySQL | 3306 | MySqlConnector | MySQL compatibility |
| Firebird | 3050 | FirebirdSql.Data.FirebirdClient | Firebird migration |
| Native | 3092 | ScratchBird.Data (future) | Direct access |

**Recommendation:** Use **Npgsql** (PostgreSQL protocol) for most applications. It offers the best feature support, performance, and .NET integration.

---

## Part 1: Quick Start

### Installation

```bash
# Npgsql (PostgreSQL protocol - recommended)
dotnet add package Npgsql

# MySqlConnector (MySQL protocol)
dotnet add package MySqlConnector

# Firebird (Firebird protocol)
dotnet add package FirebirdSql.Data.FirebirdClient

# Entity Framework Core providers
dotnet add package Npgsql.EntityFrameworkCore.PostgreSQL
dotnet add package Pomelo.EntityFrameworkCore.MySql
dotnet add package FirebirdSql.EntityFrameworkCore.Firebird
```

### First Connection

```csharp
using Npgsql;

// Quick connection test
var connectionString = "Host=localhost;Port=5432;Database=scratchbird;Username=app_user;Password=secret";

await using var conn = new NpgsqlConnection(connectionString);
await conn.OpenAsync();

await using var cmd = new NpgsqlCommand("SELECT version()", conn);
var version = await cmd.ExecuteScalarAsync();
Console.WriteLine($"Connected to: {version}");
```

---

## Part 2: Npgsql (PostgreSQL Protocol)

Npgsql is the recommended driver for ScratchBird, offering the most complete feature support and best .NET integration.

### Connection Strings

**Basic connection:**
```csharp
var connString = "Host=localhost;Port=5432;Database=scratchbird;Username=app_user;Password=secret";
```

**With SSL:**
```csharp
var connString = "Host=localhost;Port=5432;Database=scratchbird;" +
                 "Username=app_user;Password=secret;" +
                 "SSL Mode=Require;Trust Server Certificate=true";
```

**With connection pooling:**
```csharp
var connString = "Host=localhost;Port=5432;Database=scratchbird;" +
                 "Username=app_user;Password=secret;" +
                 "Minimum Pool Size=5;Maximum Pool Size=100;" +
                 "Connection Idle Lifetime=300;Connection Pruning Interval=10";
```

**Connection string parameters:**

| Parameter | Default | Description |
|-----------|---------|-------------|
| Host | localhost | Server hostname |
| Port | 5432 | Server port |
| Database | postgres | Database name |
| Username | - | Login user |
| Password | - | Login password |
| SSL Mode | Prefer | Disable, Allow, Prefer, Require, VerifyCA, VerifyFull |
| Minimum Pool Size | 0 | Minimum connections in pool |
| Maximum Pool Size | 100 | Maximum connections in pool |
| Connection Idle Lifetime | 300 | Seconds before idle connection is closed |
| Timeout | 15 | Connection timeout in seconds |
| Command Timeout | 30 | Default command timeout in seconds |

### CRUD Operations

**Create (INSERT):**
```csharp
await using var conn = new NpgsqlConnection(connectionString);
await conn.OpenAsync();

await using var cmd = new NpgsqlCommand(@"
    INSERT INTO users (name, email, created_at)
    VALUES (@name, @email, @created_at)
    RETURNING id", conn);

cmd.Parameters.AddWithValue("name", "Alice");
cmd.Parameters.AddWithValue("email", "alice@example.com");
cmd.Parameters.AddWithValue("created_at", DateTime.UtcNow);

var newId = await cmd.ExecuteScalarAsync();
Console.WriteLine($"Created user with ID: {newId}");
```

**Read (SELECT):**
```csharp
await using var conn = new NpgsqlConnection(connectionString);
await conn.OpenAsync();

// Single row
await using (var cmd = new NpgsqlCommand("SELECT * FROM users WHERE id = @id", conn))
{
    cmd.Parameters.AddWithValue("id", 1);
    await using var reader = await cmd.ExecuteReaderAsync();

    if (await reader.ReadAsync())
    {
        Console.WriteLine($"User: {reader.GetString(1)}, Email: {reader.GetString(2)}");
    }
}

// Multiple rows
await using (var cmd = new NpgsqlCommand("SELECT id, name, email FROM users WHERE active = true", conn))
{
    await using var reader = await cmd.ExecuteReaderAsync();

    while (await reader.ReadAsync())
    {
        Console.WriteLine($"{reader.GetInt32(0)}: {reader.GetString(1)} <{reader.GetString(2)}>");
    }
}
```

**Update:**
```csharp
await using var conn = new NpgsqlConnection(connectionString);
await conn.OpenAsync();

await using var cmd = new NpgsqlCommand(@"
    UPDATE users
    SET email = @email, updated_at = @updated_at
    WHERE id = @id", conn);

cmd.Parameters.AddWithValue("id", 1);
cmd.Parameters.AddWithValue("email", "alice.new@example.com");
cmd.Parameters.AddWithValue("updated_at", DateTime.UtcNow);

var rowsAffected = await cmd.ExecuteNonQueryAsync();
Console.WriteLine($"Updated {rowsAffected} rows");
```

**Delete:**
```csharp
await using var conn = new NpgsqlConnection(connectionString);
await conn.OpenAsync();

await using var cmd = new NpgsqlCommand("DELETE FROM users WHERE id = @id", conn);
cmd.Parameters.AddWithValue("id", 1);

var rowsDeleted = await cmd.ExecuteNonQueryAsync();
Console.WriteLine($"Deleted {rowsDeleted} rows");
```

### Parameterized Queries

**Named parameters:**
```csharp
var cmd = new NpgsqlCommand(@"
    SELECT * FROM products
    WHERE category = @category
    AND price BETWEEN @min_price AND @max_price
    ORDER BY name", conn);

cmd.Parameters.AddWithValue("category", "electronics");
cmd.Parameters.AddWithValue("min_price", 100.00m);
cmd.Parameters.AddWithValue("max_price", 500.00m);
```

**Typed parameters:**
```csharp
cmd.Parameters.Add(new NpgsqlParameter("id", NpgsqlTypes.NpgsqlDbType.Integer) { Value = 42 });
cmd.Parameters.Add(new NpgsqlParameter("name", NpgsqlTypes.NpgsqlDbType.Varchar, 100) { Value = "Product" });
cmd.Parameters.Add(new NpgsqlParameter("price", NpgsqlTypes.NpgsqlDbType.Numeric) { Value = 29.99m });
cmd.Parameters.Add(new NpgsqlParameter("data", NpgsqlTypes.NpgsqlDbType.Jsonb) { Value = jsonString });
```

**Array parameters:**
```csharp
var cmd = new NpgsqlCommand("SELECT * FROM users WHERE id = ANY(@ids)", conn);
cmd.Parameters.AddWithValue("ids", new[] { 1, 2, 3, 4, 5 });
```

### Transactions

**Basic transaction:**
```csharp
await using var conn = new NpgsqlConnection(connectionString);
await conn.OpenAsync();

await using var transaction = await conn.BeginTransactionAsync();

try
{
    await using (var cmd = new NpgsqlCommand(
        "UPDATE accounts SET balance = balance - @amount WHERE id = @from_id", conn, transaction))
    {
        cmd.Parameters.AddWithValue("amount", 100.00m);
        cmd.Parameters.AddWithValue("from_id", 1);
        await cmd.ExecuteNonQueryAsync();
    }

    await using (var cmd = new NpgsqlCommand(
        "UPDATE accounts SET balance = balance + @amount WHERE id = @to_id", conn, transaction))
    {
        cmd.Parameters.AddWithValue("amount", 100.00m);
        cmd.Parameters.AddWithValue("to_id", 2);
        await cmd.ExecuteNonQueryAsync();
    }

    await transaction.CommitAsync();
    Console.WriteLine("Transfer completed");
}
catch (Exception ex)
{
    await transaction.RollbackAsync();
    Console.WriteLine($"Transfer failed: {ex.Message}");
    throw;
}
```

**Isolation levels:**
```csharp
// Read committed (default)
await using var tx = await conn.BeginTransactionAsync(IsolationLevel.ReadCommitted);

// Repeatable read
await using var tx = await conn.BeginTransactionAsync(IsolationLevel.RepeatableRead);

// Serializable
await using var tx = await conn.BeginTransactionAsync(IsolationLevel.Serializable);
```

**Savepoints:**
```csharp
await using var conn = new NpgsqlConnection(connectionString);
await conn.OpenAsync();
await using var tx = await conn.BeginTransactionAsync();

try
{
    // First operation
    await ExecuteCommand(conn, tx, "INSERT INTO orders (customer_id) VALUES (1)");

    // Create savepoint before risky operation
    await tx.SaveAsync("before_items");

    try
    {
        await ExecuteCommand(conn, tx, "INSERT INTO order_items (order_id, product_id) VALUES (1, 999)");
    }
    catch
    {
        // Rollback to savepoint, keep the order
        await tx.RollbackAsync("before_items");
    }

    await tx.CommitAsync();
}
catch
{
    await tx.RollbackAsync();
    throw;
}
```

### Batch Operations

**NpgsqlBatch (Npgsql 6.0+):**
```csharp
await using var conn = new NpgsqlConnection(connectionString);
await conn.OpenAsync();

await using var batch = new NpgsqlBatch(conn);

// Add multiple commands to batch
batch.BatchCommands.Add(new NpgsqlBatchCommand("INSERT INTO logs (message) VALUES ('Event 1')"));
batch.BatchCommands.Add(new NpgsqlBatchCommand("INSERT INTO logs (message) VALUES ('Event 2')"));
batch.BatchCommands.Add(new NpgsqlBatchCommand("INSERT INTO logs (message) VALUES ('Event 3')"));

await batch.ExecuteNonQueryAsync();
```

**Batch with parameters:**
```csharp
await using var batch = new NpgsqlBatch(conn);

foreach (var user in users)
{
    var cmd = new NpgsqlBatchCommand("INSERT INTO users (name, email) VALUES ($1, $2)");
    cmd.Parameters.AddWithValue(user.Name);
    cmd.Parameters.AddWithValue(user.Email);
    batch.BatchCommands.Add(cmd);
}

await batch.ExecuteNonQueryAsync();
```

**COPY for bulk inserts:**
```csharp
await using var conn = new NpgsqlConnection(connectionString);
await conn.OpenAsync();

await using var writer = await conn.BeginBinaryImportAsync(
    "COPY users (name, email, created_at) FROM STDIN (FORMAT BINARY)");

foreach (var user in users)
{
    await writer.StartRowAsync();
    await writer.WriteAsync(user.Name, NpgsqlDbType.Varchar);
    await writer.WriteAsync(user.Email, NpgsqlDbType.Varchar);
    await writer.WriteAsync(user.CreatedAt, NpgsqlDbType.TimestampTz);
}

await writer.CompleteAsync();
```

### Connection Pooling

Npgsql automatically manages connection pooling. Configure via connection string:

```csharp
var connString = "Host=localhost;Database=scratchbird;Username=app_user;Password=secret;" +
                 "Minimum Pool Size=10;" +      // Pre-create connections
                 "Maximum Pool Size=100;" +     // Max concurrent connections
                 "Connection Idle Lifetime=300;" +  // Close idle connections after 5 min
                 "Connection Pruning Interval=10";  // Check every 10 seconds

// Create data source for better pool management (Npgsql 7.0+)
await using var dataSource = NpgsqlDataSource.Create(connString);

// Get connections from pool
await using var conn1 = await dataSource.OpenConnectionAsync();
await using var conn2 = await dataSource.OpenConnectionAsync();

// Connections automatically return to pool when disposed
```

**Data source with configuration:**
```csharp
var dataSourceBuilder = new NpgsqlDataSourceBuilder(connString);

// Configure mapping
dataSourceBuilder.MapEnum<OrderStatus>("order_status");

// Configure logging
dataSourceBuilder.UseLoggerFactory(loggerFactory);

// Build data source
await using var dataSource = dataSourceBuilder.Build();
```

### Async Operations

All Npgsql operations support async/await:

```csharp
public async Task<List<User>> GetUsersAsync(CancellationToken cancellationToken = default)
{
    var users = new List<User>();

    await using var conn = new NpgsqlConnection(connectionString);
    await conn.OpenAsync(cancellationToken);

    await using var cmd = new NpgsqlCommand("SELECT id, name, email FROM users", conn);
    await using var reader = await cmd.ExecuteReaderAsync(cancellationToken);

    while (await reader.ReadAsync(cancellationToken))
    {
        users.Add(new User
        {
            Id = reader.GetInt32(0),
            Name = reader.GetString(1),
            Email = reader.GetString(2)
        });
    }

    return users;
}
```

**Streaming large results:**
```csharp
public async IAsyncEnumerable<User> StreamUsersAsync(
    [EnumeratorCancellation] CancellationToken cancellationToken = default)
{
    await using var conn = new NpgsqlConnection(connectionString);
    await conn.OpenAsync(cancellationToken);

    await using var cmd = new NpgsqlCommand("SELECT id, name, email FROM users", conn);
    await using var reader = await cmd.ExecuteReaderAsync(cancellationToken);

    while (await reader.ReadAsync(cancellationToken))
    {
        yield return new User
        {
            Id = reader.GetInt32(0),
            Name = reader.GetString(1),
            Email = reader.GetString(2)
        };
    }
}

// Usage
await foreach (var user in StreamUsersAsync())
{
    Console.WriteLine(user.Name);
}
```

---

## Part 3: MySqlConnector (MySQL Protocol)

MySqlConnector provides MySQL protocol support for ScratchBird.

### Connection Setup

```csharp
using MySqlConnector;

var connString = "Server=localhost;Port=3306;Database=scratchbird;" +
                 "User Id=app_user;Password=secret";

await using var conn = new MySqlConnection(connString);
await conn.OpenAsync();

await using var cmd = new MySqlCommand("SELECT VERSION()", conn);
var version = await cmd.ExecuteScalarAsync();
Console.WriteLine($"Connected: {version}");
```

### Connection String Options

```csharp
var connString = "Server=localhost;Port=3306;Database=scratchbird;" +
                 "User Id=app_user;Password=secret;" +
                 "SslMode=Required;" +
                 "Pooling=true;" +
                 "MinimumPoolSize=5;" +
                 "MaximumPoolSize=100;" +
                 "ConnectionLifeTime=300;" +
                 "ConnectionTimeout=15;" +
                 "DefaultCommandTimeout=30";
```

### CRUD Operations

```csharp
// INSERT
await using (var cmd = new MySqlCommand(@"
    INSERT INTO users (name, email, created_at)
    VALUES (@name, @email, @created_at)", conn))
{
    cmd.Parameters.AddWithValue("@name", "Bob");
    cmd.Parameters.AddWithValue("@email", "bob@example.com");
    cmd.Parameters.AddWithValue("@created_at", DateTime.UtcNow);
    await cmd.ExecuteNonQueryAsync();

    var lastId = cmd.LastInsertedId;
    Console.WriteLine($"Inserted ID: {lastId}");
}

// SELECT
await using (var cmd = new MySqlCommand("SELECT * FROM users WHERE id = @id", conn))
{
    cmd.Parameters.AddWithValue("@id", 1);
    await using var reader = await cmd.ExecuteReaderAsync();

    while (await reader.ReadAsync())
    {
        Console.WriteLine($"{reader["name"]}: {reader["email"]}");
    }
}

// UPDATE
await using (var cmd = new MySqlCommand(
    "UPDATE users SET email = @email WHERE id = @id", conn))
{
    cmd.Parameters.AddWithValue("@id", 1);
    cmd.Parameters.AddWithValue("@email", "bob.new@example.com");
    var rows = await cmd.ExecuteNonQueryAsync();
    Console.WriteLine($"Updated {rows} rows");
}

// DELETE
await using (var cmd = new MySqlCommand("DELETE FROM users WHERE id = @id", conn))
{
    cmd.Parameters.AddWithValue("@id", 1);
    await cmd.ExecuteNonQueryAsync();
}
```

### Transactions

```csharp
await using var conn = new MySqlConnection(connString);
await conn.OpenAsync();

await using var tx = await conn.BeginTransactionAsync();

try
{
    await using (var cmd = new MySqlCommand(
        "UPDATE accounts SET balance = balance - @amount WHERE id = @id", conn, tx))
    {
        cmd.Parameters.AddWithValue("@amount", 100.00m);
        cmd.Parameters.AddWithValue("@id", 1);
        await cmd.ExecuteNonQueryAsync();
    }

    await using (var cmd = new MySqlCommand(
        "UPDATE accounts SET balance = balance + @amount WHERE id = @id", conn, tx))
    {
        cmd.Parameters.AddWithValue("@amount", 100.00m);
        cmd.Parameters.AddWithValue("@id", 2);
        await cmd.ExecuteNonQueryAsync();
    }

    await tx.CommitAsync();
}
catch
{
    await tx.RollbackAsync();
    throw;
}
```

### Batch Operations

```csharp
await using var conn = new MySqlConnection(connString);
await conn.OpenAsync();

await using var batch = new MySqlBatch(conn);

foreach (var user in users)
{
    var cmd = new MySqlBatchCommand(
        "INSERT INTO users (name, email) VALUES (@name, @email)");
    cmd.Parameters.AddWithValue("@name", user.Name);
    cmd.Parameters.AddWithValue("@email", user.Email);
    batch.BatchCommands.Add(cmd);
}

await batch.ExecuteNonQueryAsync();
```

---

## Part 4: FirebirdSql.Data.FirebirdClient (Firebird Protocol)

For applications migrating from Firebird or requiring Firebird compatibility.

### Connection Setup

```csharp
using FirebirdSql.Data.FirebirdClient;

var connString = new FbConnectionStringBuilder
{
    DataSource = "localhost",
    Port = 3050,
    Database = "scratchbird",
    UserID = "SYSDBA",
    Password = "masterkey",
    Charset = "UTF8",
    Pooling = true,
    MinPoolSize = 5,
    MaxPoolSize = 100
}.ToString();

await using var conn = new FbConnection(connString);
await conn.OpenAsync();
```

### CRUD Operations

```csharp
// INSERT with RETURNING
await using (var cmd = new FbCommand(@"
    INSERT INTO users (name, email, created_at)
    VALUES (@name, @email, @created_at)
    RETURNING id", conn))
{
    cmd.Parameters.AddWithValue("@name", "Charlie");
    cmd.Parameters.AddWithValue("@email", "charlie@example.com");
    cmd.Parameters.AddWithValue("@created_at", DateTime.Now);

    var id = await cmd.ExecuteScalarAsync();
    Console.WriteLine($"Created ID: {id}");
}

// SELECT with FIRST/SKIP
await using (var cmd = new FbCommand(@"
    SELECT FIRST 10 SKIP 0 id, name, email
    FROM users
    ORDER BY name", conn))
{
    await using var reader = await cmd.ExecuteReaderAsync();
    while (await reader.ReadAsync())
    {
        Console.WriteLine($"{reader.GetInt32(0)}: {reader.GetString(1)}");
    }
}

// UPDATE OR INSERT (upsert)
await using (var cmd = new FbCommand(@"
    UPDATE OR INSERT INTO users (id, name, email)
    VALUES (@id, @name, @email)
    MATCHING (id)", conn))
{
    cmd.Parameters.AddWithValue("@id", 1);
    cmd.Parameters.AddWithValue("@name", "Charlie Updated");
    cmd.Parameters.AddWithValue("@email", "charlie@example.com");
    await cmd.ExecuteNonQueryAsync();
}
```

### Transactions

```csharp
await using var conn = new FbConnection(connString);
await conn.OpenAsync();

// Begin transaction with options
var txOptions = new FbTransactionOptions
{
    TransactionBehavior = FbTransactionBehavior.ReadCommitted |
                          FbTransactionBehavior.NoRecordVersion
};

await using var tx = await conn.BeginTransactionAsync(txOptions);

try
{
    await using var cmd = new FbCommand("UPDATE accounts SET balance = balance - 100 WHERE id = 1", conn, tx);
    await cmd.ExecuteNonQueryAsync();

    await tx.CommitAsync();
}
catch
{
    await tx.RollbackAsync();
    throw;
}
```

### Execute Block (Anonymous PL/SQL)

```csharp
await using var cmd = new FbCommand(@"
    EXECUTE BLOCK (p_customer_id INTEGER = @customer_id)
    RETURNS (order_count INTEGER, total_amount DECIMAL(18,2))
    AS
    BEGIN
        SELECT COUNT(*), COALESCE(SUM(amount), 0)
        FROM orders
        WHERE customer_id = :p_customer_id
        INTO :order_count, :total_amount;
        SUSPEND;
    END", conn);

cmd.Parameters.AddWithValue("@customer_id", 1);

await using var reader = await cmd.ExecuteReaderAsync();
if (await reader.ReadAsync())
{
    Console.WriteLine($"Orders: {reader["order_count"]}, Total: {reader["total_amount"]}");
}
```

---

## Part 5: Entity Framework Core

### Npgsql EF Core Provider

**Installation:**
```bash
dotnet add package Npgsql.EntityFrameworkCore.PostgreSQL
```

**DbContext setup:**
```csharp
using Microsoft.EntityFrameworkCore;

public class AppDbContext : DbContext
{
    public DbSet<User> Users { get; set; }
    public DbSet<Order> Orders { get; set; }
    public DbSet<Product> Products { get; set; }

    protected override void OnConfiguring(DbContextOptionsBuilder optionsBuilder)
    {
        optionsBuilder.UseNpgsql(
            "Host=localhost;Port=5432;Database=scratchbird;Username=app_user;Password=secret",
            npgsqlOptions =>
            {
                npgsqlOptions.EnableRetryOnFailure(
                    maxRetryCount: 3,
                    maxRetryDelay: TimeSpan.FromSeconds(10),
                    errorCodesToAdd: null);
                npgsqlOptions.CommandTimeout(30);
            });
    }

    protected override void OnModelCreating(ModelBuilder modelBuilder)
    {
        modelBuilder.Entity<User>(entity =>
        {
            entity.ToTable("users");
            entity.HasKey(e => e.Id);
            entity.Property(e => e.Name).HasMaxLength(100).IsRequired();
            entity.Property(e => e.Email).HasMaxLength(255).IsRequired();
            entity.HasIndex(e => e.Email).IsUnique();
        });

        modelBuilder.Entity<Order>(entity =>
        {
            entity.ToTable("orders");
            entity.HasOne(e => e.User)
                  .WithMany(u => u.Orders)
                  .HasForeignKey(e => e.UserId);
        });
    }
}

public class User
{
    public int Id { get; set; }
    public string Name { get; set; } = null!;
    public string Email { get; set; } = null!;
    public DateTime CreatedAt { get; set; }
    public List<Order> Orders { get; set; } = new();
}

public class Order
{
    public int Id { get; set; }
    public int UserId { get; set; }
    public User User { get; set; } = null!;
    public decimal Amount { get; set; }
    public DateTime OrderDate { get; set; }
}
```

**CRUD operations:**
```csharp
await using var db = new AppDbContext();

// Create
var user = new User
{
    Name = "Alice",
    Email = "alice@example.com",
    CreatedAt = DateTime.UtcNow
};
db.Users.Add(user);
await db.SaveChangesAsync();

// Read
var users = await db.Users
    .Where(u => u.Name.Contains("Ali"))
    .OrderBy(u => u.Name)
    .ToListAsync();

var userWithOrders = await db.Users
    .Include(u => u.Orders)
    .FirstOrDefaultAsync(u => u.Id == 1);

// Update
var existing = await db.Users.FindAsync(1);
if (existing != null)
{
    existing.Email = "alice.new@example.com";
    await db.SaveChangesAsync();
}

// Delete
var toDelete = await db.Users.FindAsync(1);
if (toDelete != null)
{
    db.Users.Remove(toDelete);
    await db.SaveChangesAsync();
}
```

**Transactions:**
```csharp
await using var db = new AppDbContext();
await using var tx = await db.Database.BeginTransactionAsync();

try
{
    var user = new User { Name = "New User", Email = "new@example.com" };
    db.Users.Add(user);
    await db.SaveChangesAsync();

    var order = new Order { UserId = user.Id, Amount = 99.99m, OrderDate = DateTime.UtcNow };
    db.Orders.Add(order);
    await db.SaveChangesAsync();

    await tx.CommitAsync();
}
catch
{
    await tx.RollbackAsync();
    throw;
}
```

**Raw SQL:**
```csharp
// Query with interpolation (parameters are safely handled)
var minAmount = 100m;
var orders = await db.Orders
    .FromSqlInterpolated($"SELECT * FROM orders WHERE amount > {minAmount}")
    .ToListAsync();

// Execute raw SQL
await db.Database.ExecuteSqlInterpolatedAsync(
    $"UPDATE users SET last_login = {DateTime.UtcNow} WHERE id = {userId}");
```

### Dependency Injection (ASP.NET Core)

```csharp
// Program.cs
var builder = WebApplication.CreateBuilder(args);

// Add DbContext with connection pooling
builder.Services.AddDbContextPool<AppDbContext>(options =>
    options.UseNpgsql(
        builder.Configuration.GetConnectionString("DefaultConnection"),
        npgsqlOptions =>
        {
            npgsqlOptions.EnableRetryOnFailure();
            npgsqlOptions.MigrationsAssembly("MyApp.Migrations");
        }));

// Or use NpgsqlDataSource for more control
builder.Services.AddNpgsqlDataSource(
    builder.Configuration.GetConnectionString("DefaultConnection"));
```

**appsettings.json:**
```json
{
  "ConnectionStrings": {
    "DefaultConnection": "Host=localhost;Port=5432;Database=scratchbird;Username=app_user;Password=secret"
  }
}
```

---

## Part 6: Dapper

Dapper is a lightweight micro-ORM that works well with ScratchBird.

### Installation

```bash
dotnet add package Dapper
```

### Basic Usage

```csharp
using Dapper;
using Npgsql;

var connectionString = "Host=localhost;Port=5432;Database=scratchbird;Username=app_user;Password=secret";

// Query single row
await using var conn = new NpgsqlConnection(connectionString);
var user = await conn.QuerySingleOrDefaultAsync<User>(
    "SELECT id, name, email, created_at FROM users WHERE id = @Id",
    new { Id = 1 });

// Query multiple rows
var users = await conn.QueryAsync<User>(
    "SELECT id, name, email, created_at FROM users WHERE active = @Active",
    new { Active = true });

// Insert
var newId = await conn.ExecuteScalarAsync<int>(@"
    INSERT INTO users (name, email, created_at)
    VALUES (@Name, @Email, @CreatedAt)
    RETURNING id",
    new { Name = "Dave", Email = "dave@example.com", CreatedAt = DateTime.UtcNow });

// Update
var affected = await conn.ExecuteAsync(
    "UPDATE users SET email = @Email WHERE id = @Id",
    new { Id = 1, Email = "dave.new@example.com" });

// Delete
await conn.ExecuteAsync("DELETE FROM users WHERE id = @Id", new { Id = 1 });
```

### Multi-Mapping (Joins)

```csharp
var sql = @"
    SELECT u.id, u.name, u.email, o.id, o.amount, o.order_date
    FROM users u
    INNER JOIN orders o ON u.id = o.user_id
    WHERE u.id = @UserId";

var userOrders = new Dictionary<int, User>();

await conn.QueryAsync<User, Order, User>(
    sql,
    (user, order) =>
    {
        if (!userOrders.TryGetValue(user.Id, out var existingUser))
        {
            existingUser = user;
            existingUser.Orders = new List<Order>();
            userOrders[user.Id] = existingUser;
        }
        existingUser.Orders.Add(order);
        return existingUser;
    },
    new { UserId = 1 },
    splitOn: "id");

var result = userOrders.Values.FirstOrDefault();
```

### Transactions

```csharp
await using var conn = new NpgsqlConnection(connectionString);
await conn.OpenAsync();

await using var tx = await conn.BeginTransactionAsync();

try
{
    await conn.ExecuteAsync(
        "UPDATE accounts SET balance = balance - @Amount WHERE id = @Id",
        new { Amount = 100m, Id = 1 },
        transaction: tx);

    await conn.ExecuteAsync(
        "UPDATE accounts SET balance = balance + @Amount WHERE id = @Id",
        new { Amount = 100m, Id = 2 },
        transaction: tx);

    await tx.CommitAsync();
}
catch
{
    await tx.RollbackAsync();
    throw;
}
```

### Bulk Operations with Dapper

```csharp
var users = new List<User>
{
    new User { Name = "User 1", Email = "user1@example.com" },
    new User { Name = "User 2", Email = "user2@example.com" },
    new User { Name = "User 3", Email = "user3@example.com" }
};

await conn.ExecuteAsync(
    "INSERT INTO users (name, email) VALUES (@Name, @Email)",
    users);
```

---

## Part 7: ASP.NET Core Integration

### Minimal API Example

```csharp
using Microsoft.EntityFrameworkCore;
using Npgsql;

var builder = WebApplication.CreateBuilder(args);

// Add services
builder.Services.AddDbContextPool<AppDbContext>(options =>
    options.UseNpgsql(builder.Configuration.GetConnectionString("DefaultConnection")));

var app = builder.Build();

// Endpoints
app.MapGet("/users", async (AppDbContext db) =>
    await db.Users.ToListAsync());

app.MapGet("/users/{id}", async (int id, AppDbContext db) =>
    await db.Users.FindAsync(id) is User user
        ? Results.Ok(user)
        : Results.NotFound());

app.MapPost("/users", async (User user, AppDbContext db) =>
{
    db.Users.Add(user);
    await db.SaveChangesAsync();
    return Results.Created($"/users/{user.Id}", user);
});

app.MapPut("/users/{id}", async (int id, User updated, AppDbContext db) =>
{
    var user = await db.Users.FindAsync(id);
    if (user is null) return Results.NotFound();

    user.Name = updated.Name;
    user.Email = updated.Email;
    await db.SaveChangesAsync();

    return Results.Ok(user);
});

app.MapDelete("/users/{id}", async (int id, AppDbContext db) =>
{
    var user = await db.Users.FindAsync(id);
    if (user is null) return Results.NotFound();

    db.Users.Remove(user);
    await db.SaveChangesAsync();

    return Results.NoContent();
});

app.Run();
```

### Controller-Based API

```csharp
[ApiController]
[Route("api/[controller]")]
public class UsersController : ControllerBase
{
    private readonly AppDbContext _db;
    private readonly ILogger<UsersController> _logger;

    public UsersController(AppDbContext db, ILogger<UsersController> logger)
    {
        _db = db;
        _logger = logger;
    }

    [HttpGet]
    public async Task<ActionResult<IEnumerable<User>>> GetUsers(
        [FromQuery] int page = 1,
        [FromQuery] int pageSize = 10)
    {
        var users = await _db.Users
            .OrderBy(u => u.Name)
            .Skip((page - 1) * pageSize)
            .Take(pageSize)
            .ToListAsync();

        return Ok(users);
    }

    [HttpGet("{id}")]
    public async Task<ActionResult<User>> GetUser(int id)
    {
        var user = await _db.Users.FindAsync(id);
        return user is null ? NotFound() : Ok(user);
    }

    [HttpPost]
    public async Task<ActionResult<User>> CreateUser(CreateUserRequest request)
    {
        var user = new User
        {
            Name = request.Name,
            Email = request.Email,
            CreatedAt = DateTime.UtcNow
        };

        _db.Users.Add(user);
        await _db.SaveChangesAsync();

        _logger.LogInformation("Created user {UserId}", user.Id);
        return CreatedAtAction(nameof(GetUser), new { id = user.Id }, user);
    }

    [HttpPut("{id}")]
    public async Task<IActionResult> UpdateUser(int id, UpdateUserRequest request)
    {
        var user = await _db.Users.FindAsync(id);
        if (user is null) return NotFound();

        user.Name = request.Name;
        user.Email = request.Email;

        await _db.SaveChangesAsync();
        return NoContent();
    }

    [HttpDelete("{id}")]
    public async Task<IActionResult> DeleteUser(int id)
    {
        var user = await _db.Users.FindAsync(id);
        if (user is null) return NotFound();

        _db.Users.Remove(user);
        await _db.SaveChangesAsync();

        return NoContent();
    }
}

public record CreateUserRequest(string Name, string Email);
public record UpdateUserRequest(string Name, string Email);
```

### Health Checks

```csharp
// Program.cs
builder.Services.AddHealthChecks()
    .AddNpgSql(
        builder.Configuration.GetConnectionString("DefaultConnection")!,
        name: "scratchbird",
        tags: new[] { "db", "ready" });

var app = builder.Build();

app.MapHealthChecks("/health", new HealthCheckOptions
{
    ResponseWriter = async (context, report) =>
    {
        context.Response.ContentType = "application/json";
        var result = new
        {
            status = report.Status.ToString(),
            checks = report.Entries.Select(e => new
            {
                name = e.Key,
                status = e.Value.Status.ToString(),
                duration = e.Value.Duration.TotalMilliseconds
            })
        };
        await context.Response.WriteAsJsonAsync(result);
    }
});
```

---

## Part 8: Error Handling

### Npgsql Exceptions

```csharp
using Npgsql;

try
{
    await using var conn = new NpgsqlConnection(connectionString);
    await conn.OpenAsync();

    await using var cmd = new NpgsqlCommand("INSERT INTO users (email) VALUES (@email)", conn);
    cmd.Parameters.AddWithValue("email", "duplicate@example.com");
    await cmd.ExecuteNonQueryAsync();
}
catch (NpgsqlException ex)
{
    switch (ex.SqlState)
    {
        case PostgresErrorCodes.UniqueViolation:  // 23505
            Console.WriteLine($"Duplicate value: {ex.ConstraintName}");
            break;
        case PostgresErrorCodes.ForeignKeyViolation:  // 23503
            Console.WriteLine($"Foreign key violation: {ex.ConstraintName}");
            break;
        case PostgresErrorCodes.NotNullViolation:  // 23502
            Console.WriteLine($"NULL value in column: {ex.ColumnName}");
            break;
        case PostgresErrorCodes.CheckViolation:  // 23514
            Console.WriteLine($"Check constraint failed: {ex.ConstraintName}");
            break;
        default:
            Console.WriteLine($"Database error [{ex.SqlState}]: {ex.Message}");
            break;
    }
}
catch (TimeoutException)
{
    Console.WriteLine("Connection or command timed out");
}
catch (OperationCanceledException)
{
    Console.WriteLine("Operation was cancelled");
}
```

### Common PostgreSQL Error Codes

| Code | Name | Description |
|------|------|-------------|
| 23505 | unique_violation | Duplicate key |
| 23503 | foreign_key_violation | FK constraint failed |
| 23502 | not_null_violation | NULL in NOT NULL column |
| 23514 | check_violation | CHECK constraint failed |
| 42P01 | undefined_table | Table doesn't exist |
| 42703 | undefined_column | Column doesn't exist |
| 08006 | connection_failure | Connection lost |
| 57014 | query_canceled | Query cancelled/timeout |

### Retry Logic

```csharp
using Polly;
using Polly.Retry;

public class DatabaseService
{
    private readonly NpgsqlDataSource _dataSource;
    private readonly AsyncRetryPolicy _retryPolicy;

    public DatabaseService(NpgsqlDataSource dataSource)
    {
        _dataSource = dataSource;

        _retryPolicy = Policy
            .Handle<NpgsqlException>(ex => IsTransient(ex))
            .Or<TimeoutException>()
            .WaitAndRetryAsync(
                retryCount: 3,
                sleepDurationProvider: attempt => TimeSpan.FromSeconds(Math.Pow(2, attempt)),
                onRetry: (exception, timespan, attempt, context) =>
                {
                    Console.WriteLine($"Retry {attempt} after {timespan}: {exception.Message}");
                });
    }

    private static bool IsTransient(NpgsqlException ex)
    {
        // Transient errors that may succeed on retry
        return ex.SqlState switch
        {
            "08006" => true,  // connection_failure
            "08001" => true,  // sqlclient_unable_to_establish_sqlconnection
            "08004" => true,  // sqlserver_rejected_establishment_of_sqlconnection
            "40001" => true,  // serialization_failure
            "40P01" => true,  // deadlock_detected
            "57P01" => true,  // admin_shutdown
            _ => false
        };
    }

    public async Task<List<User>> GetUsersAsync()
    {
        return await _retryPolicy.ExecuteAsync(async () =>
        {
            await using var conn = await _dataSource.OpenConnectionAsync();
            await using var cmd = new NpgsqlCommand("SELECT * FROM users", conn);
            await using var reader = await cmd.ExecuteReaderAsync();

            var users = new List<User>();
            while (await reader.ReadAsync())
            {
                users.Add(MapUser(reader));
            }
            return users;
        });
    }
}
```

---

## Part 9: Special Data Types

### JSON/JSONB

```csharp
using System.Text.Json;

// Insert JSON
await using var cmd = new NpgsqlCommand(@"
    INSERT INTO documents (name, metadata)
    VALUES (@name, @metadata::jsonb)", conn);

cmd.Parameters.AddWithValue("name", "config");
cmd.Parameters.AddWithValue("metadata", JsonSerializer.Serialize(new
{
    version = "1.0",
    settings = new { enabled = true, timeout = 30 }
}));

await cmd.ExecuteNonQueryAsync();

// Query JSON
await using var queryCmd = new NpgsqlCommand(@"
    SELECT name, metadata->>'version' as version,
           (metadata->'settings'->>'enabled')::boolean as enabled
    FROM documents
    WHERE metadata @> '{""version"": ""1.0""}'", conn);

await using var reader = await queryCmd.ExecuteReaderAsync();
while (await reader.ReadAsync())
{
    Console.WriteLine($"{reader["name"]}: v{reader["version"]}, enabled={reader["enabled"]}");
}
```

### Arrays

```csharp
// Insert array
await using var cmd = new NpgsqlCommand(@"
    INSERT INTO products (name, tags)
    VALUES (@name, @tags)", conn);

cmd.Parameters.AddWithValue("name", "Laptop");
cmd.Parameters.AddWithValue("tags", new[] { "electronics", "computers", "portable" });

await cmd.ExecuteNonQueryAsync();

// Query with array operations
await using var queryCmd = new NpgsqlCommand(@"
    SELECT name, tags
    FROM products
    WHERE 'electronics' = ANY(tags)", conn);

await using var reader = await queryCmd.ExecuteReaderAsync();
while (await reader.ReadAsync())
{
    var tags = reader.GetFieldValue<string[]>(1);
    Console.WriteLine($"{reader["name"]}: [{string.Join(", ", tags)}]");
}
```

### UUID

```csharp
// Insert UUID
await using var cmd = new NpgsqlCommand(@"
    INSERT INTO sessions (id, user_id, created_at)
    VALUES (@id, @user_id, @created_at)", conn);

cmd.Parameters.AddWithValue("id", Guid.NewGuid());
cmd.Parameters.AddWithValue("user_id", 1);
cmd.Parameters.AddWithValue("created_at", DateTime.UtcNow);

await cmd.ExecuteNonQueryAsync();

// Query by UUID
await using var queryCmd = new NpgsqlCommand(
    "SELECT * FROM sessions WHERE id = @id", conn);
queryCmd.Parameters.AddWithValue("id", sessionGuid);
```

### Date/Time

```csharp
// Npgsql 6.0+ uses DateOnly/TimeOnly
await using var cmd = new NpgsqlCommand(@"
    INSERT INTO events (name, event_date, start_time, created_at)
    VALUES (@name, @date, @time, @timestamp)", conn);

cmd.Parameters.AddWithValue("name", "Conference");
cmd.Parameters.AddWithValue("date", new DateOnly(2026, 6, 15));
cmd.Parameters.AddWithValue("time", new TimeOnly(9, 0, 0));
cmd.Parameters.AddWithValue("timestamp", DateTime.UtcNow);

await cmd.ExecuteNonQueryAsync();

// Reading date/time
await using var reader = await queryCmd.ExecuteReaderAsync();
while (await reader.ReadAsync())
{
    var date = reader.GetFieldValue<DateOnly>(1);
    var time = reader.GetFieldValue<TimeOnly>(2);
    var timestamp = reader.GetDateTime(3);
}
```

### Vectors (HNSW/Vector Search)

```csharp
using Pgvector;

// Register vector type
var dataSourceBuilder = new NpgsqlDataSourceBuilder(connectionString);
dataSourceBuilder.UseVector();
await using var dataSource = dataSourceBuilder.Build();

// Insert vector
await using var conn = await dataSource.OpenConnectionAsync();
await using var cmd = new NpgsqlCommand(@"
    INSERT INTO documents (title, embedding)
    VALUES (@title, @embedding)", conn);

cmd.Parameters.AddWithValue("title", "Sample Document");
cmd.Parameters.AddWithValue("embedding", new Vector(new float[] { 0.1f, 0.2f, 0.3f }));

await cmd.ExecuteNonQueryAsync();

// Vector similarity search
await using var searchCmd = new NpgsqlCommand(@"
    SELECT title, embedding <-> @query AS distance
    FROM documents
    ORDER BY embedding <-> @query
    LIMIT 5", conn);

searchCmd.Parameters.AddWithValue("query", new Vector(new float[] { 0.15f, 0.25f, 0.35f }));

await using var reader = await searchCmd.ExecuteReaderAsync();
while (await reader.ReadAsync())
{
    Console.WriteLine($"{reader["title"]}: distance={reader["distance"]}");
}
```

---

## Part 10: Common Issues

### Connection Pool Exhaustion

**Symptoms:**
- Timeouts waiting for connections
- "The connection pool has been exhausted" errors

**Solutions:**
```csharp
// 1. Ensure connections are properly disposed
await using var conn = new NpgsqlConnection(connectionString);  // Always use 'using'

// 2. Increase pool size
var connString = "...;Maximum Pool Size=200";

// 3. Monitor pool statistics
NpgsqlConnection.GlobalTypeMapper.UseNetTopologySuite();
Console.WriteLine($"Open connections: {NpgsqlConnection.PoolStatistics}");

// 4. Clear stuck connections
NpgsqlConnection.ClearAllPools();
```

### Timeout Issues

```csharp
// Connection timeout
var connString = "...;Timeout=30";

// Command timeout (per command)
cmd.CommandTimeout = 60;

// Global command timeout
var connString = "...;Command Timeout=60";

// Cancellation token for user-initiated cancel
using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(30));
await cmd.ExecuteNonQueryAsync(cts.Token);
```

### SSL/TLS Issues

```csharp
// Trust server certificate (development only)
var connString = "...;SSL Mode=Require;Trust Server Certificate=true";

// Verify certificate (production)
var connString = "...;SSL Mode=VerifyFull;Root Certificate=/path/to/ca.crt";
```

### Encoding Issues

```csharp
// Ensure UTF-8 encoding
var connString = "...;Encoding=UTF8";

// For Firebird
var fbConnString = "...;Charset=UTF8";
```

### Memory with Large Results

```csharp
// Stream results instead of loading all
await using var reader = await cmd.ExecuteReaderAsync(CommandBehavior.SequentialAccess);

// Use async enumerable
await foreach (var user in StreamUsersAsync())
{
    Process(user);
}
```

---

## Quick Reference

### Connection String Templates

**Npgsql (PostgreSQL protocol):**
```
Host=localhost;Port=5432;Database=scratchbird;Username=user;Password=pass;SSL Mode=Prefer;Minimum Pool Size=5;Maximum Pool Size=100
```

**MySqlConnector (MySQL protocol):**
```
Server=localhost;Port=3306;Database=scratchbird;User Id=user;Password=pass;SslMode=Required;Pooling=true;MinimumPoolSize=5;MaximumPoolSize=100
```

**FirebirdSql (Firebird protocol):**
```
DataSource=localhost;Port=3050;Database=scratchbird;User=SYSDBA;Password=masterkey;Charset=UTF8;Pooling=true;MinPoolSize=5;MaxPoolSize=100
```

### NuGet Packages

| Package | Protocol | Purpose |
|---------|----------|---------|
| Npgsql | PostgreSQL | ADO.NET provider |
| Npgsql.EntityFrameworkCore.PostgreSQL | PostgreSQL | EF Core provider |
| MySqlConnector | MySQL | ADO.NET provider |
| Pomelo.EntityFrameworkCore.MySql | MySQL | EF Core provider |
| FirebirdSql.Data.FirebirdClient | Firebird | ADO.NET provider |
| FirebirdSql.EntityFrameworkCore.Firebird | Firebird | EF Core provider |
| Dapper | Any | Micro-ORM |
| Pgvector | PostgreSQL | Vector search |

### Common Operations Cheat Sheet

| Operation | Npgsql | MySqlConnector | Firebird |
|-----------|--------|----------------|----------|
| Open connection | `await conn.OpenAsync()` | `await conn.OpenAsync()` | `await conn.OpenAsync()` |
| Execute query | `ExecuteReaderAsync()` | `ExecuteReaderAsync()` | `ExecuteReaderAsync()` |
| Execute non-query | `ExecuteNonQueryAsync()` | `ExecuteNonQueryAsync()` | `ExecuteNonQueryAsync()` |
| Get scalar | `ExecuteScalarAsync()` | `ExecuteScalarAsync()` | `ExecuteScalarAsync()` |
| Begin transaction | `BeginTransactionAsync()` | `BeginTransactionAsync()` | `BeginTransactionAsync()` |
| Add parameter | `cmd.Parameters.AddWithValue()` | `cmd.Parameters.AddWithValue()` | `cmd.Parameters.AddWithValue()` |
| Get last insert ID | `RETURNING id` in SQL | `cmd.LastInsertedId` | `RETURNING id` in SQL |

---

## See Also

- [Driver Comparison](Driver-Comparison.md) - Compare all available drivers
- [Connection Guide](../getting-started/first-connection.md) - First connection walkthrough
- [Performance Tuning](../user-guides/Performance-Tuning.md) - Optimize database performance
- [Vector Search](../user-guides/Vector-Search.md) - AI and vector search guide
