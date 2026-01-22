# PHP Driver Specification
**Priority:** P0 (Critical - Beta Required)
**Status:** Draft
**Target Market:** ~18% of developers, 80% of websites, WordPress ecosystem
**Use Cases:** WordPress, WooCommerce, Laravel, Symfony, web applications, CMSes

---


## ScratchBird V2 Scope (Native Protocol Only)

- Protocol: ScratchBird Native Wire Protocol (SBWP) v1.1 only.
- Default port: 3092.
- TLS 1.3 required.
- Emulated protocol drivers (PostgreSQL/MySQL/Firebird/TDS) are out of scope.


## Wrapper Types
- JSONB: `ScratchBird\PDO\Jsonb`
- RANGE: `ScratchBird\PDO\Range`
- GEOMETRY: `ScratchBird\PDO\Geometry`

## Overview

PHP powers approximately 80% of all websites and is the foundation for WordPress (43% of all websites), making it absolutely critical for ScratchBird market adoption. A PDO driver with mysqli compatibility is essential for the entire PHP web ecosystem.

**Key Requirements:**
- PDO (PHP Data Objects) driver implementation
- mysqli compatibility layer (for WordPress/legacy applications)
- Support for PHP 7.4, 8.0, 8.1, 8.2, 8.3
- Prepared statement support (prevent SQL injection)
- Transaction management
- Persistent connections
- Error handling with exceptions
- WordPress compatibility verification

## Usage examples

```php
$pdo = new PDO("scratchbird:host=localhost;port=3092;dbname=db", "user", "pass");
$pdo->query("select 1 as one");

$stmt = $pdo->prepare("select * from widgets where id = :id");
$stmt->execute([":id" => 42]);
$rows = $stmt->fetchAll(PDO::FETCH_ASSOC);
```

---

## Specification Documents

### Required Documents

- [x] [SPECIFICATION.md](SPECIFICATION.md) - Detailed technical specification
  - PDO driver interface implementation
  - mysqli compatibility layer
  - Connection management and pooling
  - Prepared statements and parameter binding
  - Type mappings (PHP ↔ ScratchBird)
  - Error handling (exceptions and error codes)
  - Transaction and savepoint support

- [x] [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) - Development roadmap
  - Milestone 1: PDO driver core (connection, query, prepare)
  - Milestone 2: mysqli compatibility layer
  - Milestone 3: WordPress compatibility testing
  - Milestone 4: Laravel/Symfony framework integration
  - Milestone 5: Performance optimization
  - Resource requirements
  - Timeline estimates

- [x] [TESTING_CRITERIA.md](TESTING_CRITERIA.md) - Test requirements
  - PDO compliance tests
  - mysqli compatibility tests
  - WordPress installation and operation tests
  - Laravel framework tests
  - Performance benchmarks vs PDO_PGSQL
  - Memory leak tests (long-running processes)
  - Concurrent connection tests

- [x] [COMPATIBILITY_MATRIX.md](COMPATIBILITY_MATRIX.md) - Version support
  - PHP versions (7.4, 8.0, 8.1, 8.2, 8.3)
  - PDO versions and extensions
  - Web servers (Apache, Nginx, PHP-FPM)
  - Frameworks (Laravel 9+, 10+, Symfony 5+, 6+)
  - CMSes (WordPress 5.x, 6.x, Drupal, Joomla)
  - Operating systems (Linux, Windows, macOS)

- [x] [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md) - Migration from other drivers
  - From PDO_PGSQL (PostgreSQL)
  - From PDO_MYSQL / mysqli
  - From pdo_firebird
  - WordPress database migration
  - Connection string changes
  - Configuration differences

- [x] [API_REFERENCE.md](API_REFERENCE.md) - Complete API documentation
  - PDO class extensions
  - PDOStatement class
  - mysqli compatibility classes
  - Error classes and constants
  - Configuration options

### Shared References

- [../DRIVER_BASELINE_SPEC.md](../DRIVER_BASELINE_SPEC.md) - Shared V2 driver requirements.

### Examples Directory

- [ ] **examples/basic_pdo.php** - Simple PDO connection and query
- [ ] **examples/prepared_statements.php** - Parameterized queries
- [ ] **examples/transactions.php** - Transaction management
- [ ] **examples/error_handling.php** - Exception handling
- [ ] **examples/mysqli_compat.php** - mysqli compatibility layer
- [ ] **examples/wordpress/** - WordPress integration
  - **wp-config.php** - WordPress configuration
  - **db.php** - Custom database drop-in
  - **README.md** - Installation instructions
- [ ] **examples/laravel/** - Laravel framework
  - **.env** - Environment configuration
  - **config/database.php** - Database configuration
  - **DatabaseServiceProvider.php** - Custom provider
- [ ] **examples/symfony/** - Symfony framework
  - **doctrine.yaml** - Doctrine DBAL configuration
- [ ] **examples/drupal/** - Drupal CMS integration
- [ ] **examples/connection_pooling.php** - Persistent connections

---

## Key Integration Points

### PDO Driver Implementation

**Critical**: PDO is the standard database abstraction layer for modern PHP applications.

Requirements:
- PDO driver registration (`pdo_scratchbird`)
- PDO::__construct override for DSN parsing
- PDOStatement implementation
- Named and positional parameter binding
- Fetch modes (FETCH_ASSOC, FETCH_OBJ, FETCH_CLASS, etc.)
- Error modes (ERRMODE_SILENT, ERRMODE_WARNING, ERRMODE_EXCEPTION)

Example PDO usage:
```php
<?php
// Basic PDO connection
try {
    $dsn = 'scratchbird:host=localhost;port=3092;dbname=mydb';
    $pdo = new PDO($dsn, 'myuser', 'mypassword', [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        PDO::ATTR_EMULATE_PREPARES => false,
    ]);

    // Simple query
    $stmt = $pdo->query('SELECT * FROM users');
    while ($row = $stmt->fetch()) {
        echo "User: {$row['id']} - {$row['name']}\n";
    }

    // Prepared statement with positional parameters
    $stmt = $pdo->prepare('SELECT * FROM users WHERE email = ?');
    $stmt->execute(['user@example.com']);
    $user = $stmt->fetch();

    // Prepared statement with named parameters
    $stmt = $pdo->prepare('INSERT INTO users (name, email) VALUES (:name, :email)');
    $stmt->execute([
        ':name' => 'Alice',
        ':email' => 'alice@example.com',
    ]);
    $userId = $pdo->lastInsertId();

    // Transaction
    $pdo->beginTransaction();
    try {
        $pdo->exec("INSERT INTO users (name, email) VALUES ('Bob', 'bob@example.com')");
        $pdo->exec("INSERT INTO users (name, email) VALUES ('Charlie', 'charlie@example.com')");
        $pdo->commit();
    } catch (Exception $e) {
        $pdo->rollBack();
        throw $e;
    }

} catch (PDOException $e) {
    echo "Error: " . $e->getMessage();
}
```

### WordPress Integration

**Critical**: WordPress powers 43% of all websites. Full compatibility is mandatory.

Requirements:
- mysqli compatibility layer (WordPress still uses mysqli in places)
- Custom database class drop-in (`wp-content/db.php`)
- Support for WordPress database schema
- WordPress multisite compatibility
- Plugin and theme compatibility
- WordPress database prefix support

Example WordPress wp-config.php:
```php
<?php
// WordPress configuration for ScratchBird
define('DB_NAME', 'wordpress_db');
define('DB_USER', 'wp_user');
define('DB_PASSWORD', 'wp_password');
define('DB_HOST', 'localhost:3092');
define('DB_CHARSET', 'utf8mb4');
define('DB_COLLATE', '');

// Use ScratchBird driver
define('DB_DRIVER', 'scratchbird');

// Custom database drop-in
// Place scratchbird-db.php in wp-content/db.php
```

Example WordPress database drop-in (`wp-content/db.php`):
```php
<?php
/**
 * ScratchBird database drop-in for WordPress
 * This file overrides WordPress's default wpdb class to use ScratchBird
 */

require_once ABSPATH . WPINC . '/class-wpdb.php';

class wpdb extends wpdb {
    public function __construct($dbuser, $dbpassword, $dbname, $dbhost) {
        // Override to use PDO with ScratchBird
        $dsn = "scratchbird:host=$dbhost;dbname=$dbname";
        try {
            $this->dbh = new PDO($dsn, $dbuser, $dbpassword, [
                PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
                PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_OBJ,
            ]);
            $this->ready = true;
        } catch (PDOException $e) {
            $this->bail($e->getMessage());
        }
    }

    // Override query methods to use PDO...
}
```

### Laravel Framework Integration

**Important**: Laravel is the most popular PHP framework.

Requirements:
- Custom database driver for Illuminate\Database
- Database service provider
- Schema builder support
- Migration support
- Eloquent ORM compatibility

Example Laravel database configuration:
```php
<?php
// config/database.php

return [
    'default' => env('DB_CONNECTION', 'scratchbird'),

    'connections' => [
        'scratchbird' => [
            'driver' => 'scratchbird',
            'host' => env('DB_HOST', 'localhost'),
            'port' => env('DB_PORT', '3092'),
            'database' => env('DB_DATABASE', 'laravel'),
            'username' => env('DB_USERNAME', 'forge'),
            'password' => env('DB_PASSWORD', ''),
            'charset' => 'utf8mb4',
            'prefix' => '',
            'schema' => 'public',
            'sslmode' => 'prefer',
        ],
    ],
];
```

```php
<?php
// .env file
DB_CONNECTION=scratchbird
DB_HOST=localhost
DB_PORT=3092
DB_DATABASE=laravel_app
DB_USERNAME=laravel_user
DB_PASSWORD=secret
```

Example Laravel service provider:
```php
<?php

namespace App\Providers;

use Illuminate\Support\ServiceProvider;
use Illuminate\Database\Connection;
use PDO;

class ScratchBirdServiceProvider extends ServiceProvider
{
    public function register()
    {
        Connection::resolverFor('scratchbird', function ($connection, $database, $prefix, $config) {
            $connector = new ScratchBirdConnector();
            $pdo = $connector->connect($config);

            return new Connection($pdo, $database, $prefix, $config);
        });
    }
}
```

### mysqli Compatibility Layer

**Important**: Many legacy PHP applications and WordPress still use mysqli.

Requirements:
- mysqli class emulation
- mysqli_* procedural functions
- Result set compatibility
- Prepared statement compatibility

Example mysqli compatibility:
```php
<?php
// mysqli object-oriented interface
$mysqli = new mysqli_scratchbird('localhost', 'myuser', 'mypass', 'mydb', 3092);

if ($mysqli->connect_error) {
    die('Connect Error: ' . $mysqli->connect_error);
}

// Query
$result = $mysqli->query('SELECT * FROM users');
while ($row = $result->fetch_assoc()) {
    echo "User: {$row['name']}\n";
}

// Prepared statement
$stmt = $mysqli->prepare('SELECT * FROM users WHERE email = ?');
$stmt->bind_param('s', $email);
$email = 'user@example.com';
$stmt->execute();
$result = $stmt->get_result();
$user = $result->fetch_assoc();

$mysqli->close();
```

---

## PDO Driver API

### Driver Registration

```php
<?php
// Extension registration (in C/C++)
// or pure PHP implementation

class PDO_ScratchBird extends PDO {
    public function __construct($dsn, $username = null, $password = null, $options = []) {
        // Parse DSN
        // Connect to ScratchBird
        // Initialize PDO parent
    }
}

// Register driver
PDO::registerDriver('scratchbird', PDO_ScratchBird::class);
```

### DSN (Data Source Name) Format

```
scratchbird:host=localhost;port=3092;dbname=mydb

Parameters:
  host         - Database host (default: localhost)
  port         - Database port (default: 3092)
  dbname       - Database name (required)
  charset      - Character set (default: utf8)
  sslmode      - SSL mode (disable, allow, prefer, require)
  connect_timeout - Connection timeout in seconds
  options      - Additional connection options

Examples:
  scratchbird:host=localhost;port=3092;dbname=mydb
  scratchbird:host=db.example.com;dbname=production;sslmode=require
  scratchbird:host=localhost;dbname=test;charset=utf8mb4
```

### PDO Attributes

```php
<?php
// Set attributes
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
$pdo->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);
$pdo->setAttribute(PDO::ATTR_EMULATE_PREPARES, false);
$pdo->setAttribute(PDO::ATTR_STRINGIFY_FETCHES, false);

// ScratchBird-specific attributes
$pdo->setAttribute(PDO::SCRATCHBIRD_ATTR_DISABLE_PREPARES, false);
$pdo->setAttribute(PDO::SCRATCHBIRD_ATTR_PERSISTENT, true);
```

---

## Performance Targets

Benchmark against PDO_PGSQL (PostgreSQL PDO driver):

| Operation | Target Performance |
|-----------|-------------------|
| Connection establishment | Within 10% of PDO_PGSQL |
| Simple SELECT (1 row) | Within 10% of PDO_PGSQL |
| Bulk SELECT (10,000 rows) | Within 15% of PDO_PGSQL |
| Prepared statement execute | Within 10% of PDO_PGSQL |
| Bulk INSERT (1,000 rows) | Within 15% of PDO_PGSQL |
| Transaction commit | Within 10% of PDO_PGSQL |
| WordPress page load (10 queries) | Within 15% of MySQL |

---

## Dependencies

### Runtime Dependencies

- PHP >= 7.4 (support 7.4, 8.0, 8.1, 8.2, 8.3)
- PDO extension (built into PHP)
- (Optional) mysqli extension compatibility

### Build Dependencies (if PHP extension)

- PHP development headers (php-dev)
- C/C++ compiler
- Make

### Alternative: Pure PHP Implementation

- No compilation required
- Socket library (built-in)
- May have slightly lower performance

---

## Installation

### PECL Extension (future)

```bash
# Install via PECL
pecl install scratchbird

# Enable in php.ini
extension=pdo_scratchbird.so
```

### Composer Package (pure PHP alternative)

```bash
# Install via Composer
composer require scratchbird/pdo-scratchbird

# Use in PHP
require_once 'vendor/autoload.php';
```

### Manual Installation

```bash
# Clone repository
git clone https://github.com/scratchbird/php-pdo-scratchbird.git
cd php-pdo-scratchbird

# If C extension:
phpize
./configure
make
sudo make install

# Add to php.ini
echo "extension=pdo_scratchbird.so" | sudo tee -a /etc/php/8.2/cli/php.ini
```

---

## Documentation Requirements

### User Documentation

- [ ] Quick start guide (5-minute tutorial)
- [ ] Installation instructions (PECL, Composer, manual)
- [ ] DSN format and connection options
- [ ] Prepared statements and SQL injection prevention
- [ ] Transaction management guide
- [ ] Error handling guide
- [ ] Performance tuning tips (persistent connections, etc.)
- [ ] WordPress migration guide
- [ ] Laravel migration guide

### API Documentation

- [ ] Complete PHPDoc for all classes and methods
- [ ] PDO attribute reference
- [ ] Error code reference
- [ ] Type mapping reference

### Integration Guides

- [ ] WordPress integration and migration guide
- [ ] WooCommerce compatibility guide
- [ ] Laravel framework integration
- [ ] Symfony framework integration
- [ ] Drupal CMS integration
- [ ] Joomla CMS integration
- [ ] CodeIgniter framework
- [ ] CakePHP framework

---

## Release Criteria

### Functional Completeness

- [ ] PDO driver fully implemented
- [ ] mysqli compatibility layer (for WordPress)
- [ ] Connection and persistent connections
- [ ] Prepared statements (named and positional parameters)
- [ ] Transaction management
- [ ] All PDO fetch modes supported
- [ ] Error handling (exceptions and error codes)
- [ ] All standard PHP/PDO types supported

### Quality Metrics

- [ ] >90% test coverage
- [ ] PDO compliance tests passing
- [ ] WordPress installation and operation successful
- [ ] WordPress plugin compatibility (top 20 plugins)
- [ ] Laravel tests passing
- [ ] Performance benchmarks met (within 10-15% of PDO_PGSQL)
- [ ] Memory leak tests passing
- [ ] Concurrent connection tests passing

### Documentation

- [ ] Complete PHPDoc for all public APIs
- [ ] User guide complete
- [ ] 15+ code examples
- [ ] WordPress migration guide with examples
- [ ] Laravel migration guide
- [ ] Troubleshooting guide

### Packaging

- [ ] PECL package (if C extension)
- [ ] Composer package
- [ ] Packagist.org listing
- [ ] GitHub releases
- [ ] Automated CI/CD

---

## Community and Support

### Package Locations

- PECL: https://pecl.php.net/package/pdo_scratchbird
- Packagist: https://packagist.org/packages/scratchbird/pdo-scratchbird
- GitHub: https://github.com/scratchbird/php-pdo-scratchbird
- Documentation: https://scratchbird.dev/docs/php/

### Issue Tracking

- GitHub Issues: https://github.com/scratchbird/php-pdo-scratchbird/issues
- Response time target: <3 days
- Bug fix SLA: Critical <7 days, High <14 days

### Community Channels

- Discord: ScratchBird #php-driver
- WordPress Forums: ScratchBird subforum
- Stack Overflow: Tag `scratchbird-php` or `scratchbird`

---

## References

1. **PDO Documentation**
   https://www.php.net/manual/en/book.pdo.php

2. **mysqli Documentation**
   https://www.php.net/manual/en/book.mysqli.php

3. **WordPress Database Class**
   https://developer.wordpress.org/reference/classes/wpdb/

4. **Laravel Database**
   https://laravel.com/docs/database

5. **Symfony Doctrine DBAL**
   https://www.doctrine-project.org/projects/dbal.html

---

## Next Steps

1. **Create detailed SPECIFICATION.md** with complete PDO implementation
2. **Develop IMPLEMENTATION_PLAN.md** with milestones and timeline
3. **Write TESTING_CRITERIA.md** with PDO compliance and WordPress tests
4. **Begin prototype** with basic PDO driver or pure PHP implementation
5. **WordPress compatibility testing** (highest priority)
6. **Laravel framework adapter** (second priority)

---

**Document Version:** 1.0 (Template)
**Last Updated:** 2026-01-03
**Status:** Draft
**Assigned To:** TBD
