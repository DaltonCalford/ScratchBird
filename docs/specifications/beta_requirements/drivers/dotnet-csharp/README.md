# C# / .NET Driver Specification
**Priority:** P0 (Critical - Beta Required)
**Status:** Draft
**Target Market:** ~28% of developers worldwide, Microsoft ecosystem, enterprise Windows
**Use Cases:** Enterprise applications, Windows services, ASP.NET web apps, Azure cloud, desktop applications

---


## ScratchBird V2 Scope (Native Protocol Only)

- Protocol: ScratchBird Native Wire Protocol (SBWP) v1.1 only.
- Default port: 3092.
- TLS 1.3 required.
- Emulated protocol drivers (PostgreSQL/MySQL/Firebird/TDS) are out of scope.

**Scope Note:** SQL Server migration references are informational; MSSQL/TDS emulation is post-gold.


## Wrapper Types
- JSONB: `ScratchBirdJsonb`
- RANGE: `ScratchBirdRange<T>`
- GEOMETRY: `ScratchBirdGeometry`

## Overview

C# and .NET are dominant in Windows enterprise environments and growing in cross-platform scenarios with .NET 6+. An ADO.NET compliant data provider is essential for ScratchBird adoption in Microsoft-centric organizations.

**Key Requirements:**
- ADO.NET data provider (System.Data.Common)
- .NET Standard 2.0 and .NET 6+ support
- Entity Framework Core provider
- Async/await support (Task-based asynchronous pattern)
- Connection pooling built-in
- NuGet package distribution
- Strong naming and assembly signing
- Source Link support for debugging

## Usage examples

```csharp
using var conn = new ScratchbirdConnection(
    "Host=localhost;Port=3092;Database=db;Username=user;Password=pass");
conn.Open();

using var cmd = conn.CreateCommand();
cmd.CommandText = "select 1 as one";
using var reader = cmd.ExecuteReader();

using var prep = conn.CreateCommand();
prep.CommandText = "select * from widgets where id = $1";
var p = prep.CreateParameter();
p.ParameterName = "$1";
p.Value = 42;
prep.Parameters.Add(p);
using var rows = prep.ExecuteReader();
```

---

## Specification Documents

### Required Documents

- [x] [SPECIFICATION.md](SPECIFICATION.md) - Detailed technical specification
  - ADO.NET provider model compliance
  - DbConnection, DbCommand, DbDataReader implementation
  - DbProviderFactory implementation
  - Type mappings (C#/.NET ↔ ScratchBird)
  - Connection string format
  - Transaction support (DbTransaction)
  - Parameter binding and SQL injection prevention
  - Async/await patterns

- [x] [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) - Development roadmap
  - Milestone 1: Core ADO.NET provider (Connection, Command, DataReader)
  - Milestone 2: Entity Framework Core provider
  - Milestone 3: Bulk copy and performance optimization
  - Milestone 4: Azure integration and Managed Identity
  - Milestone 5: Dapper micro-ORM optimization
  - Resource requirements
  - Timeline estimates

- [x] [TESTING_CRITERIA.md](TESTING_CRITERIA.md) - Test requirements
  - ADO.NET compliance tests
  - Entity Framework Core test suite compatibility
  - Performance benchmarks vs Npgsql (.NET PostgreSQL driver)
  - Connection pool stress tests
  - Async/await correctness tests
  - Thread safety and concurrent access tests
  - Memory leak tests

- [x] [COMPATIBILITY_MATRIX.md](COMPATIBILITY_MATRIX.md) - Version support
  - .NET versions (.NET Framework 4.6.2+, .NET Core 3.1, .NET 5, 6, 7, 8)
  - .NET Standard 2.0, 2.1
  - Entity Framework versions (EF6, EF Core 6, 7, 8)
  - Operating systems (Windows, Linux, macOS)
  - Visual Studio versions (2019, 2022)
  - Rider IDE compatibility

- [x] [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md) - Migration from other providers
  - From Npgsql (PostgreSQL .NET provider)
  - From MySql.Data / MySqlConnector
  - From System.Data.SqlClient (SQL Server, post-gold reference)
  - From FirebirdSql.Data.FirebirdClient
  - Connection string migration
  - Code changes required

- [x] [API_REFERENCE.md](API_REFERENCE.md) - Complete API documentation (XML docs)
  - ScratchBirdConnection
  - ScratchBirdCommand
  - ScratchBirdDataReader
  - ScratchBirdParameter
  - ScratchBirdTransaction
  - ScratchBirdConnectionStringBuilder
  - Exception classes

### Shared References

- [../DRIVER_BASELINE_SPEC.md](../DRIVER_BASELINE_SPEC.md) - Shared V2 driver requirements.

### Examples Directory

- [ ] **examples/BasicConnection.cs** - Simple ADO.NET connection
- [ ] **examples/ParameterizedQuery.cs** - Safe parameterized queries
- [ ] **examples/TransactionExample.cs** - Transaction management
- [ ] **examples/AsyncOperations.cs** - Async/await usage
- [ ] **examples/BulkCopy.cs** - Bulk data operations
- [ ] **examples/ef-core/** - Entity Framework Core
  - **Program.cs** - EF Core DbContext setup
  - **Models.cs** - Entity models
  - **Migrations/** - Code-first migrations
  - **appsettings.json** - Configuration
- [ ] **examples/aspnet-core-api/** - ASP.NET Core Web API
  - **Startup.cs** - Service configuration
  - **Controllers/UsersController.cs** - API controller
  - **Models/** - Data models
  - **appsettings.json** - Connection strings
- [ ] **examples/dapper/** - Dapper micro-ORM
- [ ] **examples/azure-functions/** - Azure Functions integration

---

## Key Integration Points

### Entity Framework Core Provider

**Critical**: Entity Framework Core is used in 70%+ of .NET projects for data access.

Requirements:
- Custom EF Core provider implementation
- Type mappings for all ScratchBird types
- Query translation (LINQ to SQL)
- Migration support (code-first, database-first)
- Scaffolding support (database to entities)
- Value converters for custom types
- Query optimization

Example EF Core usage:
```csharp
using Microsoft.EntityFrameworkCore;

public class ApplicationDbContext : DbContext
{
    public DbSet<User> Users { get; set; }
    public DbSet<Post> Posts { get; set; }

    protected override void OnConfiguring(DbContextOptionsBuilder optionsBuilder)
    {
        optionsBuilder.UseScratchBird(
            "Host=localhost;Port=3092;Database=mydb;Username=myuser;Password=mypass"
        );
    }

    protected override void OnModelCreating(ModelBuilder modelBuilder)
    {
        modelBuilder.Entity<User>(entity =>
        {
            entity.HasKey(e => e.Id);
            entity.Property(e => e.Email).IsRequired();
            entity.HasIndex(e => e.Email).IsUnique();
        });

        modelBuilder.Entity<Post>(entity =>
        {
            entity.HasOne(p => p.Author)
                  .WithMany(u => u.Posts)
                  .HasForeignKey(p => p.AuthorId);
        });
    }
}

public class User
{
    public int Id { get; set; }
    public string Name { get; set; }
    public string Email { get; set; }
    public List<Post> Posts { get; set; }
}

public class Post
{
    public int Id { get; set; }
    public string Title { get; set; }
    public string Content { get; set; }
    public int AuthorId { get; set; }
    public User Author { get; set; }
}

// Usage
using (var context = new ApplicationDbContext())
{
    var users = await context.Users
        .Include(u => u.Posts)
        .Where(u => u.Email.Contains("@example.com"))
        .ToListAsync();
}
```

### ASP.NET Core Integration

Requirements:
- Dependency injection support
- Connection string configuration (appsettings.json)
- Health checks integration
- Logging integration (ILogger)
- Options pattern support

Example ASP.NET Core setup:
```csharp
// Program.cs (ASP.NET Core 6+)
using Microsoft.EntityFrameworkCore;
using ScratchBird.Data;

var builder = WebApplication.CreateBuilder(args);

// Add DbContext with ScratchBird
builder.Services.AddDbContext<ApplicationDbContext>(options =>
    options.UseScratchBird(
        builder.Configuration.GetConnectionString("DefaultConnection")
    )
);

// Add health checks
builder.Services.AddHealthChecks()
    .AddDbContextCheck<ApplicationDbContext>();

var app = builder.Build();

app.MapGet("/users", async (ApplicationDbContext db) =>
{
    return await db.Users.ToListAsync();
});

app.MapHealthChecks("/health");

app.Run();
```

```json
// appsettings.json
{
  "ConnectionStrings": {
    "DefaultConnection": "Host=localhost;Port=3092;Database=mydb;Username=myuser;Password=mypass"
  },
  "Logging": {
    "LogLevel": {
      "Default": "Information",
      "ScratchBird": "Debug"
    }
  }
}
```

### Basic ADO.NET Usage

Example:
```csharp
using ScratchBird.Data;
using System;
using System.Data;
using System.Threading.Tasks;

public class BasicExample
{
    public static async Task Main()
    {
        string connectionString = "Host=localhost;Port=3092;Database=mydb;Username=myuser;Password=mypass";

        using (var connection = new ScratchBirdConnection(connectionString))
        {
            await connection.OpenAsync();

            // Simple query
            using (var command = new ScratchBirdCommand("SELECT * FROM users", connection))
            using (var reader = await command.ExecuteReaderAsync())
            {
                while (await reader.ReadAsync())
                {
                    int id = reader.GetInt32(0);
                    string name = reader.GetString(1);
                    Console.WriteLine($"User: {id} - {name}");
                }
            }

            // Parameterized query (prevents SQL injection)
            string sql = "SELECT * FROM users WHERE email = @email";
            using (var command = new ScratchBirdCommand(sql, connection))
            {
                command.Parameters.AddWithValue("@email", "user@example.com");

                using (var reader = await command.ExecuteReaderAsync())
                {
                    if (await reader.ReadAsync())
                    {
                        Console.WriteLine($"Found user: {reader.GetString("name")}");
                    }
                }
            }

            // Transaction example
            using (var transaction = await connection.BeginTransactionAsync())
            {
                try
                {
                    var insertCommand = new ScratchBirdCommand(
                        "INSERT INTO users (name, email) VALUES (@name, @email)",
                        connection,
                        transaction
                    );

                    insertCommand.Parameters.AddWithValue("@name", "Alice");
                    insertCommand.Parameters.AddWithValue("@email", "alice@example.com");
                    await insertCommand.ExecuteNonQueryAsync();

                    insertCommand.Parameters["@name"].Value = "Bob";
                    insertCommand.Parameters["@email"].Value = "bob@example.com";
                    await insertCommand.ExecuteNonQueryAsync();

                    await transaction.CommitAsync();
                }
                catch
                {
                    await transaction.RollbackAsync();
                    throw;
                }
            }
        }
    }
}
```

### Dapper Integration

**Important**: Dapper is a lightweight micro-ORM widely used in .NET applications.

Requirements:
- Full Dapper compatibility
- Custom type handlers for ScratchBird-specific types
- Bulk operations support

Example:
```csharp
using Dapper;
using ScratchBird.Data;

public class DapperExample
{
    public async Task<IEnumerable<User>> GetUsersAsync(string connectionString)
    {
        using (var connection = new ScratchBirdConnection(connectionString))
        {
            await connection.OpenAsync();

            // Simple query
            var users = await connection.QueryAsync<User>(
                "SELECT * FROM users WHERE active = @active",
                new { active = true }
            );

            return users;
        }
    }

    public async Task InsertUserAsync(string connectionString, User user)
    {
        using (var connection = new ScratchBirdConnection(connectionString))
        {
            await connection.OpenAsync();

            var sql = "INSERT INTO users (name, email) VALUES (@Name, @Email) RETURNING id";
            user.Id = await connection.ExecuteScalarAsync<int>(sql, user);
        }
    }
}
```

---

## ADO.NET Provider API

### Connection Class

```csharp
namespace ScratchBird.Data
{
    public sealed class ScratchBirdConnection : DbConnection
    {
        public ScratchBirdConnection();
        public ScratchBirdConnection(string connectionString);

        public override string ConnectionString { get; set; }
        public override string Database { get; }
        public override string DataSource { get; }
        public override string ServerVersion { get; }
        public override ConnectionState State { get; }

        public override void Open();
        public override Task OpenAsync(CancellationToken cancellationToken);
        public override void Close();

        public new ScratchBirdCommand CreateCommand();
        protected override DbCommand CreateDbCommand();

        public new ScratchBirdTransaction BeginTransaction();
        public new ScratchBirdTransaction BeginTransaction(IsolationLevel isolationLevel);
        protected override DbTransaction BeginDbTransaction(IsolationLevel isolationLevel);

        public override void ChangeDatabase(string databaseName);

        // Connection pooling statistics
        public static void ClearPool(ScratchBirdConnection connection);
        public static void ClearAllPools();
    }
}
```

### Command Class

```csharp
namespace ScratchBird.Data
{
    public sealed class ScratchBirdCommand : DbCommand
    {
        public ScratchBirdCommand();
        public ScratchBirdCommand(string cmdText);
        public ScratchBirdCommand(string cmdText, ScratchBirdConnection connection);
        public ScratchBirdCommand(string cmdText, ScratchBirdConnection connection, ScratchBirdTransaction transaction);

        public override string CommandText { get; set; }
        public override int CommandTimeout { get; set; }
        public override CommandType CommandType { get; set; }
        public override bool DesignTimeVisible { get; set; }
        public override UpdateRowSource UpdatedRowSource { get; set; }

        public new ScratchBirdConnection Connection { get; set; }
        protected override DbConnection DbConnection { get; set; }

        public new ScratchBirdTransaction Transaction { get; set; }
        protected override DbTransaction DbTransaction { get; set; }

        public new ScratchBirdParameterCollection Parameters { get; }
        protected override DbParameterCollection DbParameterCollection { get; }

        public override void Prepare();

        public override int ExecuteNonQuery();
        public override Task<int> ExecuteNonQueryAsync(CancellationToken cancellationToken);

        public override object ExecuteScalar();
        public override Task<object> ExecuteScalarAsync(CancellationToken cancellationToken);

        public new ScratchBirdDataReader ExecuteReader();
        public new ScratchBirdDataReader ExecuteReader(CommandBehavior behavior);
        protected override DbDataReader ExecuteDbDataReader(CommandBehavior behavior);
        public override Task<DbDataReader> ExecuteDbDataReaderAsync(CommandBehavior behavior, CancellationToken cancellationToken);

        public override void Cancel();

        protected override DbParameter CreateDbParameter();
        public new ScratchBirdParameter CreateParameter();
    }
}
```

### Connection String Builder

```csharp
namespace ScratchBird.Data
{
    public sealed class ScratchBirdConnectionStringBuilder : DbConnectionStringBuilder
    {
        public string Host { get; set; }
        public int Port { get; set; } = 3092;
        public string Database { get; set; }
        public string Username { get; set; }
        public string Password { get; set; }
        public bool SSL { get; set; }
        public string SSLMode { get; set; }
        public int Timeout { get; set; } = 30;
        public int CommandTimeout { get; set; } = 30;
        public bool Pooling { get; set; } = true;
        public int MinPoolSize { get; set; } = 0;
        public int MaxPoolSize { get; set; } = 100;
        public int ConnectionLifetime { get; set; } = 0;
        public bool Enlist { get; set; } = true;

        public override string ToString();
    }
}
```

### Example Connection Strings

```
Host=localhost;Port=3092;Database=mydb;Username=myuser;Password=mypass
Server=localhost;Database=mydb;User Id=myuser;Password=mypass;SSL Mode=Require
Host=localhost;Database=mydb;Integrated Security=true
```

---

## Performance Targets

Benchmark against Npgsql (PostgreSQL .NET provider):

| Operation | Target Performance |
|-----------|-------------------|
| Connection establishment | Within 10% of Npgsql |
| Simple SELECT (1 row) | Within 10% of Npgsql |
| Bulk SELECT (10,000 rows) | Within 15% of Npgsql |
| Parameterized INSERT | Within 10% of Npgsql |
| Bulk INSERT (10,000 rows) | Within 15% of Npgsql |
| Transaction commit | Within 10% of Npgsql |
| EF Core LINQ query | Within 15% of Npgsql |
| Async/await operations | Within 10% of Npgsql |

---

## Dependencies

### Runtime Dependencies

- .NET Standard 2.0 / .NET 6+
- System.Data.Common
- (Optional) Microsoft.Extensions.Logging.Abstractions

### Development Dependencies

- .NET SDK 6.0 or later
- Entity Framework Core (for EF provider)
- xUnit or NUnit (testing)
- BenchmarkDotNet (performance testing)

### Build Requirements

- Visual Studio 2022 or later
- JetBrains Rider
- .NET SDK 6.0+

---

## NuGet Package Structure

```xml
<!-- Main ADO.NET provider -->
<PackageReference Include="ScratchBird.Data" Version="1.0.0" />

<!-- Entity Framework Core provider -->
<PackageReference Include="ScratchBird.EntityFrameworkCore" Version="1.0.0" />

<!-- Design-time tools (scaffolding, migrations) -->
<PackageReference Include="ScratchBird.EntityFrameworkCore.Design" Version="1.0.0">
  <PrivateAssets>all</PrivateAssets>
  <IncludeAssets>runtime; build; native; contentfiles; analyzers; buildtransitive</IncludeAssets>
</PackageReference>
```

---

## Documentation Requirements

### User Documentation

- [ ] Quick start guide (5-minute tutorial)
- [ ] Installation instructions (NuGet)
- [ ] Connection string format and options
- [ ] Connection pooling best practices
- [ ] Transaction management guide
- [ ] Async/await patterns and best practices
- [ ] Error handling and exception types
- [ ] Performance tuning tips
- [ ] Azure deployment guide

### API Documentation

- [ ] Complete XML documentation for all public APIs
- [ ] IntelliSense support
- [ ] API reference documentation site
- [ ] Type mapping reference

### Integration Guides

- [ ] Entity Framework Core integration guide
- [ ] ASP.NET Core integration guide
- [ ] Dapper integration guide
- [ ] Azure Functions integration
- [ ] Windows Services integration
- [ ] Blazor (WebAssembly/Server) integration
- [ ] MAUI (cross-platform apps) integration

---

## Release Criteria

### Functional Completeness

- [ ] ADO.NET provider model compliance
- [ ] DbProviderFactory registration
- [ ] Connection, Command, DataReader, Transaction, Parameter
- [ ] Async/await support for all operations
- [ ] Connection pooling
- [ ] All standard .NET data types supported
- [ ] Entity Framework Core provider
- [ ] Bulk copy operations
- [ ] SSL/TLS support

### Quality Metrics

- [ ] >90% code coverage
- [ ] ADO.NET compliance tests passing
- [ ] Entity Framework Core tests passing
- [ ] Performance benchmarks met (within 10-15% of Npgsql)
- [ ] Thread safety tests passing
- [ ] Memory leak tests passing (24-hour stress test)
- [ ] Source Link enabled for debugging

### Documentation

- [ ] Complete XML documentation for IntelliSense
- [ ] User guide complete
- [ ] 15+ code examples
- [ ] Migration guide from Npgsql/MySql.Data
- [ ] Troubleshooting guide
- [ ] EF Core provider documentation

### Packaging

- [ ] NuGet packages published
- [ ] Strong-named assemblies
- [ ] Package signing
- [ ] Source packages
- [ ] Symbol packages (debugging)
- [ ] Automated CI/CD

---

## Community and Support

### Package Locations

- NuGet Gallery: https://www.nuget.org/packages/ScratchBird.Data/
- GitHub: https://github.com/scratchbird/scratchbird-dotnet
- Documentation: https://scratchbird.dev/docs/dotnet/
- API Reference: https://scratchbird.dev/api/dotnet/

### Issue Tracking

- GitHub Issues: https://github.com/scratchbird/scratchbird-dotnet/issues
- Response time target: <3 days
- Bug fix SLA: Critical <7 days, High <14 days

### Community Channels

- Discord: ScratchBird #dotnet-driver
- Stack Overflow: Tag `scratchbird-dotnet` or `scratchbird`
- .NET Foundation (potential membership)

---

## References

1. **ADO.NET Documentation**
   https://docs.microsoft.com/en-us/dotnet/framework/data/adonet/

2. **Npgsql** (reference implementation)
   https://www.npgsql.org/

3. **Entity Framework Core Documentation**
   https://docs.microsoft.com/en-us/ef/core/

4. **ASP.NET Core Data Access**
   https://docs.microsoft.com/en-us/aspnet/core/data/

5. **Dapper Documentation**
   https://github.com/DapperLib/Dapper

---

## Next Steps

1. **Create detailed SPECIFICATION.md** with complete ADO.NET provider API
2. **Develop IMPLEMENTATION_PLAN.md** with milestones and timeline
3. **Write TESTING_CRITERIA.md** with ADO.NET compliance requirements
4. **Begin prototype** with basic Connection and Command classes
5. **Set up CI/CD** with GitHub Actions and NuGet publishing
6. **Create EF Core provider** (critical for .NET adoption)

---

**Document Version:** 1.0 (Template)
**Last Updated:** 2026-01-03
**Status:** Draft
**Assigned To:** TBD
