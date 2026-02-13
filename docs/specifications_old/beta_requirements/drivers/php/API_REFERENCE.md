# API Reference (PDO)

## Wrapper Types
- JSONB: `ScratchBird\PDO\Jsonb`
- RANGE: `ScratchBird\PDO\Range`
- GEOMETRY: `ScratchBird\PDO\Geometry`


## PDO
```php
public function __construct(string $dsn, ?string $username = null, ?string $password = null, array $options = []);
public function prepare(string $statement, array $options = []): PDOStatement|false;
public function query(string $statement, mixed ...$fetch_mode_args): PDOStatement|false;
public function exec(string $statement): int|false;
public function beginTransaction(): bool;
public function commit(): bool;
public function rollBack(): bool;
public function lastInsertId(?string $name = null): string|false;
public function setAttribute(int $attribute, mixed $value): bool;
public function getAttribute(int $attribute): mixed;
public function errorInfo(): array;
public function errorCode(): ?string;
```

## PDOStatement
```php
public function bindParam(string|int $param, mixed &$var, int $type = PDO::PARAM_STR, int $length = 0, mixed $driver_options = null): bool;
public function bindValue(string|int $param, mixed $value, int $type = PDO::PARAM_STR): bool;
public function execute(?array $params = null): bool;
public function fetch(int $mode = PDO::FETCH_ASSOC, mixed ...$args): mixed;
public function fetchAll(int $mode = PDO::FETCH_ASSOC, mixed ...$args): array;
public function fetchColumn(int $column = 0): mixed;
public function rowCount(): int;
public function columnCount(): int;
public function getColumnMeta(int $column): array;
public function closeCursor(): bool;
public function setFetchMode(int $mode, mixed ...$args): bool;
```

## Usage examples
```php
$pdo = new PDO("scratchbird:host=localhost;port=3092;dbname=db", "user", "pass");
$stmt = $pdo->prepare("select * from widgets where id = $1");
$stmt->execute([42]);
$rows = $stmt->fetchAll(PDO::FETCH_ASSOC);
```
