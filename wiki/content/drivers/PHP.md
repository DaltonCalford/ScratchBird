# PHP Driver Guide

**Status:** Complete
**Last Updated:** 2026-01-20

---

## Overview

ScratchBird supports multiple connection protocols for PHP applications:

| Protocol | Port | Extension/Driver | Best For |
|----------|------|------------------|----------|
| PostgreSQL | 5432 | PDO_PGSQL, pgsql | Most applications (recommended) |
| MySQL | 3306 | PDO_MYSQL, mysqli | MySQL compatibility |
| Firebird | 3050 | PDO_FIREBIRD, interbase | Firebird migration |
| Native | 3092 | scratchbird-php (future) | Direct access |

**Recommendation:** Use **PDO with pgsql** (PostgreSQL protocol) for most applications. It offers the best balance of features, security, and portability.

---

## Part 1: Quick Start

### Installation

```bash
# Ubuntu/Debian
sudo apt install php-pgsql php-mysql php-pdo

# CentOS/RHEL
sudo dnf install php-pgsql php-mysqlnd php-pdo

# macOS (Homebrew)
brew install php
# Extensions are typically included

# Verify extensions
php -m | grep -E "pgsql|mysql|pdo"
```

### First Connection

```php
<?php

$dsn = "pgsql:host=localhost;port=5432;dbname=scratchbird";
$user = "app_user";
$password = "secret";

try {
    $pdo = new PDO($dsn, $user, $password, [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
    ]);

    $stmt = $pdo->query("SELECT version()");
    $version = $stmt->fetchColumn();
    echo "Connected to: $version\n";

} catch (PDOException $e) {
    die("Connection failed: " . $e->getMessage());
}
```

---

## Part 2: PDO (PostgreSQL Protocol)

PDO is the recommended database abstraction layer for PHP, providing a consistent interface across databases.

### Connection Options

**Basic connection:**
```php
$dsn = "pgsql:host=localhost;port=5432;dbname=scratchbird";
$pdo = new PDO($dsn, "app_user", "secret");
```

**With options:**
```php
$dsn = "pgsql:host=localhost;port=5432;dbname=scratchbird";
$options = [
    PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
    PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
    PDO::ATTR_EMULATE_PREPARES => false,
    PDO::ATTR_STRINGIFY_FETCHES => false,
];

$pdo = new PDO($dsn, "app_user", "secret", $options);
```

**With SSL:**
```php
$dsn = "pgsql:host=localhost;port=5432;dbname=scratchbird;sslmode=require";
$pdo = new PDO($dsn, "app_user", "secret");
```

**Connection string parameters:**

| Parameter | Description |
|-----------|-------------|
| host | Server hostname |
| port | Server port (default: 5432) |
| dbname | Database name |
| sslmode | disable, allow, prefer, require, verify-ca, verify-full |
| connect_timeout | Connection timeout in seconds |
| options | Additional connection options |

### CRUD Operations

**Create (INSERT):**
```php
// Prepared statement (recommended)
$stmt = $pdo->prepare("
    INSERT INTO users (name, email, created_at)
    VALUES (:name, :email, :created_at)
");

$stmt->execute([
    ':name' => 'Alice',
    ':email' => 'alice@example.com',
    ':created_at' => date('Y-m-d H:i:s'),
]);

$userId = $pdo->lastInsertId();
echo "Created user ID: $userId\n";

// With RETURNING (PostgreSQL)
$stmt = $pdo->prepare("
    INSERT INTO users (name, email, created_at)
    VALUES (:name, :email, NOW())
    RETURNING id
");

$stmt->execute([
    ':name' => 'Bob',
    ':email' => 'bob@example.com',
]);

$userId = $stmt->fetchColumn();
```

**Read (SELECT):**
```php
// Single row
$stmt = $pdo->prepare("SELECT * FROM users WHERE id = :id");
$stmt->execute([':id' => 1]);
$user = $stmt->fetch();

if ($user) {
    echo "User: {$user['name']} <{$user['email']}>\n";
} else {
    echo "User not found\n";
}

// Multiple rows
$stmt = $pdo->prepare("SELECT * FROM users WHERE active = :active");
$stmt->execute([':active' => true]);
$users = $stmt->fetchAll();

foreach ($users as $user) {
    echo "{$user['id']}: {$user['name']}\n";
}

// Fetch modes
$stmt->fetch(PDO::FETCH_ASSOC);    // Array with column names as keys
$stmt->fetch(PDO::FETCH_OBJ);      // Anonymous object
$stmt->fetch(PDO::FETCH_NUM);      // Numeric array
$stmt->fetch(PDO::FETCH_BOTH);     // Both associative and numeric

// Fetch into class
class User {
    public int $id;
    public string $name;
    public string $email;
}

$stmt->setFetchMode(PDO::FETCH_CLASS, User::class);
$user = $stmt->fetch();
```

**Update:**
```php
$stmt = $pdo->prepare("
    UPDATE users
    SET email = :email, updated_at = NOW()
    WHERE id = :id
");

$stmt->execute([
    ':id' => 1,
    ':email' => 'alice.new@example.com',
]);

$rowsAffected = $stmt->rowCount();
echo "Updated $rowsAffected rows\n";
```

**Delete:**
```php
$stmt = $pdo->prepare("DELETE FROM users WHERE id = :id");
$stmt->execute([':id' => 1]);

$rowsDeleted = $stmt->rowCount();
echo "Deleted $rowsDeleted rows\n";
```

### Prepared Statements

**Named parameters:**
```php
$stmt = $pdo->prepare("
    SELECT * FROM products
    WHERE category = :category
    AND price BETWEEN :min_price AND :max_price
    ORDER BY name
");

$stmt->execute([
    ':category' => 'electronics',
    ':min_price' => 100.00,
    ':max_price' => 500.00,
]);
```

**Positional parameters:**
```php
$stmt = $pdo->prepare("
    SELECT * FROM products
    WHERE category = ?
    AND price BETWEEN ? AND ?
");

$stmt->execute(['electronics', 100.00, 500.00]);
```

**Bound parameters (with types):**
```php
$stmt = $pdo->prepare("
    INSERT INTO products (name, price, active)
    VALUES (:name, :price, :active)
");

$stmt->bindValue(':name', 'Laptop', PDO::PARAM_STR);
$stmt->bindValue(':price', 999.99, PDO::PARAM_STR);  // Note: no PARAM_FLOAT
$stmt->bindValue(':active', true, PDO::PARAM_BOOL);

$stmt->execute();

// With bindParam (by reference)
$name = '';
$price = 0;

$stmt->bindParam(':name', $name, PDO::PARAM_STR);
$stmt->bindParam(':price', $price, PDO::PARAM_STR);

$products = [
    ['name' => 'Phone', 'price' => 599.99],
    ['name' => 'Tablet', 'price' => 399.99],
];

foreach ($products as $product) {
    $name = $product['name'];
    $price = $product['price'];
    $stmt->execute();
}
```

### Transactions

**Basic transaction:**
```php
try {
    $pdo->beginTransaction();

    // Debit account
    $stmt = $pdo->prepare("UPDATE accounts SET balance = balance - :amount WHERE id = :id");
    $stmt->execute([':amount' => 100.00, ':id' => 1]);

    // Credit account
    $stmt = $pdo->prepare("UPDATE accounts SET balance = balance + :amount WHERE id = :id");
    $stmt->execute([':amount' => 100.00, ':id' => 2]);

    $pdo->commit();
    echo "Transfer completed\n";

} catch (PDOException $e) {
    $pdo->rollBack();
    echo "Transfer failed: " . $e->getMessage() . "\n";
    throw $e;
}
```

**Savepoints:**
```php
$pdo->beginTransaction();

try {
    // Create order
    $stmt = $pdo->prepare("INSERT INTO orders (customer_id) VALUES (:customer_id) RETURNING id");
    $stmt->execute([':customer_id' => 1]);
    $orderId = $stmt->fetchColumn();

    // Savepoint before items
    $pdo->exec("SAVEPOINT before_items");

    try {
        $stmt = $pdo->prepare("INSERT INTO order_items (order_id, product_id) VALUES (:order_id, :product_id)");
        $stmt->execute([':order_id' => $orderId, ':product_id' => 999]);
    } catch (PDOException $e) {
        // Rollback to savepoint, keep the order
        $pdo->exec("ROLLBACK TO SAVEPOINT before_items");
        echo "Item insert failed, order kept\n";
    }

    $pdo->commit();

} catch (PDOException $e) {
    $pdo->rollBack();
    throw $e;
}
```

### Batch Operations

```php
// Batch insert with prepared statement
$stmt = $pdo->prepare("INSERT INTO users (name, email) VALUES (:name, :email)");

$users = [
    ['name' => 'User 1', 'email' => 'user1@example.com'],
    ['name' => 'User 2', 'email' => 'user2@example.com'],
    ['name' => 'User 3', 'email' => 'user3@example.com'],
];

$pdo->beginTransaction();

foreach ($users as $user) {
    $stmt->execute([
        ':name' => $user['name'],
        ':email' => $user['email'],
    ]);
}

$pdo->commit();

// Batch insert with single query (faster for many rows)
$values = [];
$params = [];
$i = 0;

foreach ($users as $user) {
    $values[] = "(:name$i, :email$i)";
    $params[":name$i"] = $user['name'];
    $params[":email$i"] = $user['email'];
    $i++;
}

$sql = "INSERT INTO users (name, email) VALUES " . implode(', ', $values);
$stmt = $pdo->prepare($sql);
$stmt->execute($params);
```

### COPY Protocol

```php
// For bulk inserts, use COPY (PostgreSQL)
$pdo->pgsqlCopyFromArray(
    'users',                          // Table name
    ['Alice,alice@example.com', 'Bob,bob@example.com'],  // Data
    ',',                              // Delimiter
    '\\N',                            // NULL as
    'name,email'                      // Columns
);

// From file
$pdo->pgsqlCopyFromFile('users', '/tmp/users.csv', ',', '\\N', 'name,email');
```

---

## Part 3: PDO (MySQL Protocol)

### Connection Setup

```php
$dsn = "mysql:host=localhost;port=3306;dbname=scratchbird;charset=utf8mb4";
$options = [
    PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
    PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
    PDO::MYSQL_ATTR_INIT_COMMAND => "SET NAMES utf8mb4",
];

$pdo = new PDO($dsn, "app_user", "secret", $options);
```

### MySQL-Specific Features

```php
// Last insert ID (works directly)
$stmt = $pdo->prepare("INSERT INTO users (name, email) VALUES (:name, :email)");
$stmt->execute([':name' => 'Alice', ':email' => 'alice@example.com']);
$userId = $pdo->lastInsertId();

// Multiple statements
$pdo->setAttribute(PDO::MYSQL_ATTR_MULTI_STATEMENTS, true);
// Note: Be careful with SQL injection when using multiple statements

// SSL connection
$dsn = "mysql:host=localhost;port=3306;dbname=scratchbird";
$options = [
    PDO::MYSQL_ATTR_SSL_CA => '/path/to/ca.pem',
    PDO::MYSQL_ATTR_SSL_VERIFY_SERVER_CERT => false,
];
```

---

## Part 4: mysqli (MySQL Protocol)

For applications requiring MySQL-specific features.

### Connection Setup

```php
$mysqli = new mysqli("localhost", "app_user", "secret", "scratchbird", 3306);

if ($mysqli->connect_error) {
    die("Connection failed: " . $mysqli->connect_error);
}

// Set charset
$mysqli->set_charset("utf8mb4");

echo "Connected to: " . $mysqli->server_info . "\n";
```

### mysqli CRUD

```php
// INSERT
$stmt = $mysqli->prepare("INSERT INTO users (name, email) VALUES (?, ?)");
$stmt->bind_param("ss", $name, $email);

$name = "Alice";
$email = "alice@example.com";
$stmt->execute();

$userId = $mysqli->insert_id;
echo "Created user ID: $userId\n";

// SELECT
$stmt = $mysqli->prepare("SELECT id, name, email FROM users WHERE id = ?");
$stmt->bind_param("i", $id);

$id = 1;
$stmt->execute();

$result = $stmt->get_result();
$user = $result->fetch_assoc();

if ($user) {
    echo "User: {$user['name']}\n";
}

// UPDATE
$stmt = $mysqli->prepare("UPDATE users SET email = ? WHERE id = ?");
$stmt->bind_param("si", $email, $id);

$email = "alice.new@example.com";
$id = 1;
$stmt->execute();

echo "Updated {$stmt->affected_rows} rows\n";

// DELETE
$stmt = $mysqli->prepare("DELETE FROM users WHERE id = ?");
$stmt->bind_param("i", $id);

$id = 1;
$stmt->execute();

echo "Deleted {$stmt->affected_rows} rows\n";
```

### mysqli Transactions

```php
$mysqli->begin_transaction();

try {
    $mysqli->query("UPDATE accounts SET balance = balance - 100 WHERE id = 1");
    $mysqli->query("UPDATE accounts SET balance = balance + 100 WHERE id = 2");

    $mysqli->commit();
    echo "Transfer completed\n";

} catch (mysqli_sql_exception $e) {
    $mysqli->rollback();
    echo "Transfer failed: " . $e->getMessage() . "\n";
}
```

---

## Part 5: Native pgsql Extension

For PostgreSQL-specific features not available in PDO.

### Connection Setup

```php
$conn = pg_connect("host=localhost port=5432 dbname=scratchbird user=app_user password=secret");

if (!$conn) {
    die("Connection failed\n");
}

echo "Connected to: " . pg_dbname($conn) . "\n";
```

### pgsql Operations

```php
// Query
$result = pg_query($conn, "SELECT * FROM users");

while ($row = pg_fetch_assoc($result)) {
    echo "{$row['id']}: {$row['name']}\n";
}

// Prepared statement
pg_prepare($conn, "get_user", "SELECT * FROM users WHERE id = $1");
$result = pg_execute($conn, "get_user", [1]);
$user = pg_fetch_assoc($result);

// Insert with RETURNING
$result = pg_query_params(
    $conn,
    "INSERT INTO users (name, email) VALUES ($1, $2) RETURNING id",
    ["Alice", "alice@example.com"]
);
$row = pg_fetch_assoc($result);
$userId = $row['id'];

// COPY
pg_copy_from($conn, 'users', [
    "Alice\talice@example.com\n",
    "Bob\tbob@example.com\n",
], "\t", "\\N", "name\temail");

// LISTEN/NOTIFY
pg_query($conn, "LISTEN channel_name");

while (true) {
    $notify = pg_get_notify($conn);
    if ($notify) {
        echo "Received: {$notify['message']}\n";
    }
    usleep(100000);
}
```

### pgsql Async Queries

```php
// Send query asynchronously
pg_send_query($conn, "SELECT * FROM large_table");

// Do other work while query runs
// ...

// Get result when ready
while (pg_connection_busy($conn)) {
    usleep(10000);
}

$result = pg_get_result($conn);
while ($row = pg_fetch_assoc($result)) {
    // Process row
}
```

---

## Part 6: Laravel Integration

### Configuration

**config/database.php:**
```php
'connections' => [
    'scratchbird' => [
        'driver' => 'pgsql',
        'host' => env('DB_HOST', 'localhost'),
        'port' => env('DB_PORT', '5432'),
        'database' => env('DB_DATABASE', 'scratchbird'),
        'username' => env('DB_USERNAME', 'app_user'),
        'password' => env('DB_PASSWORD', 'secret'),
        'charset' => 'utf8',
        'prefix' => '',
        'prefix_indexes' => true,
        'search_path' => 'public',
        'sslmode' => 'prefer',
    ],
],
```

**.env:**
```env
DB_CONNECTION=scratchbird
DB_HOST=localhost
DB_PORT=5432
DB_DATABASE=scratchbird
DB_USERNAME=app_user
DB_PASSWORD=secret
```

### Eloquent Models

```php
<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;

class User extends Model
{
    protected $connection = 'scratchbird';
    protected $table = 'users';

    protected $fillable = ['name', 'email'];

    protected $casts = [
        'created_at' => 'datetime',
        'active' => 'boolean',
    ];

    public function orders()
    {
        return $this->hasMany(Order::class);
    }
}

class Order extends Model
{
    protected $connection = 'scratchbird';
    protected $fillable = ['user_id', 'amount', 'status'];

    public function user()
    {
        return $this->belongsTo(User::class);
    }
}
```

### Eloquent CRUD

```php
use App\Models\User;
use App\Models\Order;

// Create
$user = User::create([
    'name' => 'Alice',
    'email' => 'alice@example.com',
]);

// Read
$user = User::find(1);
$user = User::where('email', 'alice@example.com')->first();
$users = User::where('active', true)->get();

// With relationships
$user = User::with('orders')->find(1);

// Update
$user = User::find(1);
$user->email = 'alice.new@example.com';
$user->save();

// Or mass update
User::where('id', 1)->update(['email' => 'alice.new@example.com']);

// Delete
$user = User::find(1);
$user->delete();

// Or direct delete
User::destroy(1);
User::where('active', false)->delete();
```

### Query Builder

```php
use Illuminate\Support\Facades\DB;

// Select
$users = DB::connection('scratchbird')
    ->table('users')
    ->where('active', true)
    ->orderBy('name')
    ->limit(10)
    ->get();

// Insert
DB::table('users')->insert([
    'name' => 'Alice',
    'email' => 'alice@example.com',
    'created_at' => now(),
]);

// Insert and get ID
$id = DB::table('users')->insertGetId([
    'name' => 'Bob',
    'email' => 'bob@example.com',
]);

// Update
DB::table('users')
    ->where('id', 1)
    ->update(['email' => 'alice.new@example.com']);

// Raw queries
$users = DB::select('SELECT * FROM users WHERE active = ?', [true]);

DB::statement('UPDATE users SET active = false WHERE last_login < ?', [
    now()->subMonths(6),
]);
```

### Transactions

```php
use Illuminate\Support\Facades\DB;

// Closure-based (recommended)
DB::transaction(function () {
    DB::table('accounts')->where('id', 1)->decrement('balance', 100);
    DB::table('accounts')->where('id', 2)->increment('balance', 100);
});

// Manual control
DB::beginTransaction();

try {
    DB::table('accounts')->where('id', 1)->decrement('balance', 100);
    DB::table('accounts')->where('id', 2)->increment('balance', 100);

    DB::commit();
} catch (\Exception $e) {
    DB::rollBack();
    throw $e;
}
```

### Migrations

```php
<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    protected $connection = 'scratchbird';

    public function up(): void
    {
        Schema::create('users', function (Blueprint $table) {
            $table->id();
            $table->string('name', 100);
            $table->string('email', 255)->unique();
            $table->boolean('active')->default(true);
            $table->timestamps();

            $table->index('active');
        });

        Schema::create('orders', function (Blueprint $table) {
            $table->id();
            $table->foreignId('user_id')->constrained()->onDelete('cascade');
            $table->decimal('amount', 10, 2);
            $table->string('status', 50)->default('pending');
            $table->timestamps();

            $table->index(['user_id', 'status']);
        });
    }

    public function down(): void
    {
        Schema::dropIfExists('orders');
        Schema::dropIfExists('users');
    }
};
```

---

## Part 7: Symfony Integration

### Configuration

**config/packages/doctrine.yaml:**
```yaml
doctrine:
    dbal:
        connections:
            scratchbird:
                driver: 'pdo_pgsql'
                host: '%env(DB_HOST)%'
                port: '%env(DB_PORT)%'
                dbname: '%env(DB_NAME)%'
                user: '%env(DB_USER)%'
                password: '%env(DB_PASSWORD)%'
                charset: UTF8

    orm:
        entity_managers:
            scratchbird:
                connection: scratchbird
                mappings:
                    App:
                        is_bundle: false
                        dir: '%kernel.project_dir%/src/Entity'
                        prefix: 'App\Entity'
                        alias: App
```

**.env:**
```env
DB_HOST=localhost
DB_PORT=5432
DB_NAME=scratchbird
DB_USER=app_user
DB_PASSWORD=secret
```

### Doctrine Entities

```php
<?php

namespace App\Entity;

use Doctrine\ORM\Mapping as ORM;
use Doctrine\Common\Collections\ArrayCollection;
use Doctrine\Common\Collections\Collection;

#[ORM\Entity]
#[ORM\Table(name: 'users')]
class User
{
    #[ORM\Id]
    #[ORM\GeneratedValue]
    #[ORM\Column]
    private ?int $id = null;

    #[ORM\Column(length: 100)]
    private string $name;

    #[ORM\Column(length: 255, unique: true)]
    private string $email;

    #[ORM\Column]
    private bool $active = true;

    #[ORM\Column]
    private \DateTimeImmutable $createdAt;

    #[ORM\OneToMany(mappedBy: 'user', targetEntity: Order::class)]
    private Collection $orders;

    public function __construct()
    {
        $this->orders = new ArrayCollection();
        $this->createdAt = new \DateTimeImmutable();
    }

    public function getId(): ?int
    {
        return $this->id;
    }

    public function getName(): string
    {
        return $this->name;
    }

    public function setName(string $name): self
    {
        $this->name = $name;
        return $this;
    }

    public function getEmail(): string
    {
        return $this->email;
    }

    public function setEmail(string $email): self
    {
        $this->email = $email;
        return $this;
    }

    public function getOrders(): Collection
    {
        return $this->orders;
    }
}

#[ORM\Entity]
#[ORM\Table(name: 'orders')]
class Order
{
    #[ORM\Id]
    #[ORM\GeneratedValue]
    #[ORM\Column]
    private ?int $id = null;

    #[ORM\ManyToOne(inversedBy: 'orders')]
    #[ORM\JoinColumn(nullable: false)]
    private User $user;

    #[ORM\Column(type: 'decimal', precision: 10, scale: 2)]
    private string $amount;

    #[ORM\Column(length: 50)]
    private string $status = 'pending';
}
```

### Repository Pattern

```php
<?php

namespace App\Repository;

use App\Entity\User;
use Doctrine\Bundle\DoctrineBundle\Repository\ServiceEntityRepository;
use Doctrine\Persistence\ManagerRegistry;

class UserRepository extends ServiceEntityRepository
{
    public function __construct(ManagerRegistry $registry)
    {
        parent::__construct($registry, User::class);
    }

    public function findActiveUsers(): array
    {
        return $this->createQueryBuilder('u')
            ->where('u.active = :active')
            ->setParameter('active', true)
            ->orderBy('u.name', 'ASC')
            ->getQuery()
            ->getResult();
    }

    public function findByEmail(string $email): ?User
    {
        return $this->findOneBy(['email' => $email]);
    }

    public function findUsersWithOrders(): array
    {
        return $this->createQueryBuilder('u')
            ->leftJoin('u.orders', 'o')
            ->addSelect('o')
            ->getQuery()
            ->getResult();
    }
}
```

### Controller Example

```php
<?php

namespace App\Controller;

use App\Entity\User;
use App\Repository\UserRepository;
use Doctrine\ORM\EntityManagerInterface;
use Symfony\Bundle\FrameworkBundle\Controller\AbstractController;
use Symfony\Component\HttpFoundation\JsonResponse;
use Symfony\Component\HttpFoundation\Request;
use Symfony\Component\Routing\Annotation\Route;

#[Route('/api/users')]
class UserController extends AbstractController
{
    #[Route('', methods: ['GET'])]
    public function index(UserRepository $repository): JsonResponse
    {
        $users = $repository->findActiveUsers();
        return $this->json($users);
    }

    #[Route('/{id}', methods: ['GET'])]
    public function show(User $user): JsonResponse
    {
        return $this->json($user);
    }

    #[Route('', methods: ['POST'])]
    public function create(
        Request $request,
        EntityManagerInterface $em
    ): JsonResponse {
        $data = $request->toArray();

        $user = new User();
        $user->setName($data['name']);
        $user->setEmail($data['email']);

        $em->persist($user);
        $em->flush();

        return $this->json($user, 201);
    }

    #[Route('/{id}', methods: ['PUT'])]
    public function update(
        User $user,
        Request $request,
        EntityManagerInterface $em
    ): JsonResponse {
        $data = $request->toArray();

        $user->setName($data['name'] ?? $user->getName());
        $user->setEmail($data['email'] ?? $user->getEmail());

        $em->flush();

        return $this->json($user);
    }

    #[Route('/{id}', methods: ['DELETE'])]
    public function delete(User $user, EntityManagerInterface $em): JsonResponse
    {
        $em->remove($user);
        $em->flush();

        return $this->json(null, 204);
    }
}
```

---

## Part 8: Connection Pooling

PHP doesn't have built-in connection pooling, but you can use persistent connections or external poolers.

### Persistent Connections

**PDO:**
```php
$options = [
    PDO::ATTR_PERSISTENT => true,
    PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
];

$pdo = new PDO($dsn, $user, $password, $options);
```

**mysqli:**
```php
$mysqli = new mysqli("p:localhost", "app_user", "secret", "scratchbird");
// Note: "p:" prefix enables persistent connections
```

**pgsql:**
```php
$conn = pg_pconnect("host=localhost dbname=scratchbird user=app_user password=secret");
```

### External Connection Poolers

For high-traffic applications, use external poolers like PgBouncer:

**config/database.php (Laravel):**
```php
'scratchbird' => [
    'driver' => 'pgsql',
    'host' => env('DB_HOST', 'localhost'),
    'port' => env('PGBOUNCER_PORT', '6432'),  // PgBouncer port
    'database' => env('DB_DATABASE', 'scratchbird'),
    // ... rest of config
],
```

---

## Part 9: Error Handling

### PDO Exception Handling

```php
try {
    $pdo = new PDO($dsn, $user, $password, [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
    ]);

    $stmt = $pdo->prepare("INSERT INTO users (email) VALUES (:email)");
    $stmt->execute([':email' => 'duplicate@example.com']);

} catch (PDOException $e) {
    $code = $e->getCode();
    $message = $e->getMessage();

    // PostgreSQL error codes
    switch ($code) {
        case '23505':  // unique_violation
            echo "Duplicate email address\n";
            break;
        case '23503':  // foreign_key_violation
            echo "Foreign key constraint failed\n";
            break;
        case '23502':  // not_null_violation
            echo "Required field is missing\n";
            break;
        case '08006':  // connection_failure
            echo "Database connection lost\n";
            break;
        default:
            echo "Database error [$code]: $message\n";
    }
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
| 40001 | serialization_failure | Transaction conflict |
| 40P01 | deadlock_detected | Deadlock detected |

### Retry Logic

```php
function withRetry(callable $fn, int $maxRetries = 3): mixed
{
    $lastException = null;

    for ($i = 0; $i < $maxRetries; $i++) {
        try {
            return $fn();
        } catch (PDOException $e) {
            $lastException = $e;

            if (!isRetryable($e)) {
                throw $e;
            }

            $delay = (2 ** $i) * 100000; // Exponential backoff in microseconds
            usleep($delay);
        }
    }

    throw $lastException;
}

function isRetryable(PDOException $e): bool
{
    $retryableCodes = [
        '40001',  // serialization_failure
        '40P01',  // deadlock_detected
        '08006',  // connection_failure
        '08001',  // sqlclient_unable_to_establish_sqlconnection
        '57P01',  // admin_shutdown
    ];

    return in_array($e->getCode(), $retryableCodes, true);
}

// Usage
$user = withRetry(function () use ($pdo) {
    $stmt = $pdo->prepare("SELECT * FROM users WHERE id = ?");
    $stmt->execute([1]);
    return $stmt->fetch();
});
```

---

## Part 10: Special Data Types

### JSON/JSONB

```php
// Insert JSON
$settings = ['theme' => 'dark', 'language' => 'en', 'notify' => true];

$stmt = $pdo->prepare("
    INSERT INTO user_settings (user_id, settings)
    VALUES (:user_id, :settings::jsonb)
");

$stmt->execute([
    ':user_id' => 1,
    ':settings' => json_encode($settings),
]);

// Query JSON
$stmt = $pdo->prepare("
    SELECT settings->>'theme' as theme,
           (settings->>'notify')::boolean as notify
    FROM user_settings
    WHERE user_id = :user_id
");

$stmt->execute([':user_id' => 1]);
$result = $stmt->fetch();

// Query with JSON condition
$stmt = $pdo->prepare("
    SELECT * FROM user_settings
    WHERE settings @> :filter::jsonb
");

$stmt->execute([':filter' => json_encode(['theme' => 'dark'])]);
```

### Arrays

```php
// Insert array (PostgreSQL)
$tags = ['php', 'database', 'scratchbird'];

$stmt = $pdo->prepare("
    INSERT INTO articles (title, tags)
    VALUES (:title, :tags::text[])
");

$stmt->execute([
    ':title' => 'Getting Started with PHP',
    ':tags' => '{' . implode(',', $tags) . '}',
]);

// Query array
$stmt = $pdo->prepare("
    SELECT title, tags
    FROM articles
    WHERE :tag = ANY(tags)
");

$stmt->execute([':tag' => 'php']);

// Parse array result
$result = $stmt->fetch();
$tags = trim($result['tags'], '{}');
$tagsArray = explode(',', $tags);
```

### UUID

```php
// Generate UUID in PHP
$uuid = sprintf(
    '%04x%04x-%04x-%04x-%04x-%04x%04x%04x',
    mt_rand(0, 0xffff), mt_rand(0, 0xffff),
    mt_rand(0, 0xffff),
    mt_rand(0, 0x0fff) | 0x4000,
    mt_rand(0, 0x3fff) | 0x8000,
    mt_rand(0, 0xffff), mt_rand(0, 0xffff), mt_rand(0, 0xffff)
);

// Or use ramsey/uuid
use Ramsey\Uuid\Uuid;
$uuid = Uuid::uuid4()->toString();

// Insert
$stmt = $pdo->prepare("INSERT INTO sessions (id, user_id) VALUES (:id::uuid, :user_id)");
$stmt->execute([':id' => $uuid, ':user_id' => 1]);

// Query
$stmt = $pdo->prepare("SELECT * FROM sessions WHERE id = :id::uuid");
$stmt->execute([':id' => $uuid]);
```

### Date/Time

```php
// Insert timestamps
$stmt = $pdo->prepare("
    INSERT INTO events (name, event_date, start_time, created_at)
    VALUES (:name, :event_date, :start_time, :created_at)
");

$stmt->execute([
    ':name' => 'Conference',
    ':event_date' => '2026-06-15',
    ':start_time' => '09:00:00',
    ':created_at' => date('Y-m-d H:i:s'),
]);

// With DateTime objects
$eventDate = new DateTime('2026-06-15');
$createdAt = new DateTimeImmutable();

$stmt->execute([
    ':name' => 'Workshop',
    ':event_date' => $eventDate->format('Y-m-d'),
    ':start_time' => '14:00:00',
    ':created_at' => $createdAt->format('Y-m-d H:i:s'),
]);
```

---

## Part 11: Common Issues

### Connection Issues

**Symptoms:**
- "Connection refused" errors
- Timeouts

**Solutions:**
```php
// Increase timeout
$dsn = "pgsql:host=localhost;port=5432;dbname=scratchbird;connect_timeout=10";

// Check connection before use
if (!$pdo) {
    // Reconnect
    $pdo = new PDO($dsn, $user, $password, $options);
}
```

### Memory Issues

```php
// Use unbuffered queries for large results
$pdo->setAttribute(PDO::MYSQL_ATTR_USE_BUFFERED_QUERY, false);

// Or fetch row by row
$stmt = $pdo->query("SELECT * FROM large_table");
while ($row = $stmt->fetch()) {
    processRow($row);
}

// With PostgreSQL, use cursors
$pdo->beginTransaction();
$pdo->query("DECLARE my_cursor CURSOR FOR SELECT * FROM large_table");

while (true) {
    $stmt = $pdo->query("FETCH 100 FROM my_cursor");
    $rows = $stmt->fetchAll();

    if (empty($rows)) {
        break;
    }

    foreach ($rows as $row) {
        processRow($row);
    }
}

$pdo->query("CLOSE my_cursor");
$pdo->commit();
```

### Character Encoding

```php
// PostgreSQL
$dsn = "pgsql:host=localhost;port=5432;dbname=scratchbird;options='--client_encoding=UTF8'";

// MySQL
$options = [
    PDO::MYSQL_ATTR_INIT_COMMAND => "SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci",
];
```

### SQL Injection Prevention

```php
// WRONG - Never do this!
$email = $_POST['email'];
$sql = "SELECT * FROM users WHERE email = '$email'";  // VULNERABLE!

// CORRECT - Always use prepared statements
$stmt = $pdo->prepare("SELECT * FROM users WHERE email = :email");
$stmt->execute([':email' => $_POST['email']]);
```

---

## Quick Reference

### Connection String Templates

**PDO PostgreSQL:**
```
pgsql:host=localhost;port=5432;dbname=scratchbird;sslmode=prefer
```

**PDO MySQL:**
```
mysql:host=localhost;port=3306;dbname=scratchbird;charset=utf8mb4
```

### PDO Attributes

| Attribute | Description |
|-----------|-------------|
| PDO::ATTR_ERRMODE | Error reporting mode |
| PDO::ATTR_DEFAULT_FETCH_MODE | Default fetch mode |
| PDO::ATTR_EMULATE_PREPARES | Emulate prepared statements |
| PDO::ATTR_PERSISTENT | Use persistent connections |
| PDO::ATTR_TIMEOUT | Connection timeout |

### Fetch Modes

| Mode | Description |
|------|-------------|
| PDO::FETCH_ASSOC | Associative array |
| PDO::FETCH_NUM | Numeric array |
| PDO::FETCH_OBJ | Anonymous object |
| PDO::FETCH_CLASS | Custom class |
| PDO::FETCH_COLUMN | Single column |

### Common Operations

| Operation | PDO | mysqli |
|-----------|-----|--------|
| Connect | `new PDO($dsn, ...)` | `new mysqli(...)` |
| Prepare | `$pdo->prepare($sql)` | `$mysqli->prepare($sql)` |
| Execute | `$stmt->execute($params)` | `$stmt->execute()` |
| Fetch one | `$stmt->fetch()` | `$result->fetch_assoc()` |
| Fetch all | `$stmt->fetchAll()` | `$result->fetch_all()` |
| Last ID | `$pdo->lastInsertId()` | `$mysqli->insert_id` |
| Row count | `$stmt->rowCount()` | `$stmt->affected_rows` |
| Begin tx | `$pdo->beginTransaction()` | `$mysqli->begin_transaction()` |
| Commit | `$pdo->commit()` | `$mysqli->commit()` |
| Rollback | `$pdo->rollBack()` | `$mysqli->rollback()` |

---

## See Also

- [Driver Comparison](Driver-Comparison.md) - Compare all available drivers
- [Connection Guide](../getting-started/first-connection.md) - First connection walkthrough
- [Performance Tuning](../user-guides/Performance-Tuning.md) - Optimize database performance
- [Vector Search](../user-guides/Vector-Search.md) - AI and vector search guide
