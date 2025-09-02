### Operators and Precedence

**What it is**

ScratchBird implements a comprehensive set of SQL operators with well-defined precedence rules based on the Pratt parsing algorithm. The engine distinguishes between operators that are parsed (accepted by the syntax analyzer) and those that are actually evaluated at runtime, allowing for incremental feature implementation.

**Why it matters**

- **Correct Expression Evaluation**: Understanding operator precedence prevents bugs from unexpected grouping (e.g., `AND` binds tighter than `OR`)
- **Query Optimization**: Knowing which operators are runtime-evaluated helps write efficient predicates
- **Type Safety**: Cast operators and type conversions follow predictable rules
- **Portability**: Standard SQL operator behavior ensures compatibility

**How to use it**

Use the precedence table below to understand how expressions are parsed. When writing complex expressions, use parentheses to make intent explicit. For runtime predicate evaluation, refer to the supported operator subset.

## Operator Precedence Table

Precedence is implemented via binding powers (bp) in the Pratt parser (`src/engine/parser_expr.cpp`). Higher values bind tighter:

| Precedence | Binding Power | Operators | Description | Associativity |
|------------|---------------|-----------|-------------|---------------|
| Highest | 90 | `CAST` | Type casting prefix | Right |
| | 80 | `::` | PostgreSQL-style cast | Left |
| | 70 | `*`, `/`, `%` | Multiplication, division, modulo | Left |
| | 60 | `+`, `-` | Addition, subtraction | Left |
| | 55 | `\|\|` | String concatenation | Left |
| | 50 | `=`, `<>`, `!=`, `<`, `>`, `<=`, `>=` | Comparison operators | Non-associative |
| | 45 | `IN`, `BETWEEN`, `LIKE`, `SIMILAR TO`, `IS`, `COLLATE` | Pattern and membership | Non-associative |
| | 30 | `AND` | Logical AND | Left |
| Lowest | 20 | `OR` | Logical OR | Left |

**Unary Operators** (prefix, no precedence value needed):
- `NOT` - Logical negation
- `-` - Numeric negation  
- `+` - Numeric positive (no-op)

## Operator Categories

### Arithmetic Operators

**Fully Parsed**:
```sql
+ - * / %  -- Standard arithmetic
```

**Examples**:
```sql
SELECT 
    10 + 5 * 2,        -- Result: 20 (multiplication first)
    (10 + 5) * 2,      -- Result: 30 (parentheses override)
    17 % 5,            -- Result: 2 (modulo)
    -42,               -- Unary negation
    10 / 3             -- Integer or decimal division depending on types
FROM dual;
```

### String Operators

**Concatenation**:
```sql
||  -- Concatenates strings
```

**Examples**:
```sql
SELECT 
    'Hello' || ' ' || 'World',           -- 'Hello World'
    first_name || ', ' || last_name,     -- Concatenate columns
    'Value: ' || CAST(amount AS VARCHAR) -- Mix types with casting
FROM users;
```

### Comparison Operators

**Standard Comparisons**:
```sql
=   -- Equal
<>  -- Not equal (SQL standard)
!=  -- Not equal (alternative)
<   -- Less than
>   -- Greater than
<=  -- Less than or equal
>=  -- Greater than or equal
```

**NULL-Safe Comparisons**:
```sql
IS NULL           -- Check for NULL
IS NOT NULL       -- Check for non-NULL
IS DISTINCT FROM  -- NULL-safe inequality
IS NOT DISTINCT FROM -- NULL-safe equality
```

**Examples**:
```sql
SELECT * FROM products
WHERE price >= 10.00
  AND status <> 'discontinued'
  AND discount_rate IS NOT NULL
  AND category = 'electronics';
```

### Pattern Matching Operators

**LIKE** - SQL pattern matching:
```sql
column LIKE pattern [ESCAPE char]
-- % matches any sequence
-- _ matches single character
```

**SIMILAR TO** - Regular expression pattern:
```sql
column SIMILAR TO pattern [ESCAPE char]
-- Uses SQL regular expression syntax
```

**Examples**:
```sql
SELECT * FROM users
WHERE email LIKE '%@gmail.com'
  AND phone LIKE '555-___-____'
  AND username SIMILAR TO '[a-z][a-z0-9]{3,15}';
```

### Membership and Range Operators

**IN** - Set membership:
```sql
value IN (value1, value2, ...)
value IN (SELECT ...)
```

**BETWEEN** - Range check:
```sql
value BETWEEN low AND high
-- Equivalent to: value >= low AND value <= high
```

**EXISTS** - Subquery existence:
```sql
EXISTS (SELECT ...)
```

**Examples**:
```sql
SELECT * FROM orders
WHERE status IN ('pending', 'processing', 'shipped')
  AND order_date BETWEEN DATE '2024-01-01' AND DATE '2024-12-31'
  AND EXISTS (SELECT 1 FROM order_items WHERE order_id = orders.id);
```

### Logical Operators

```sql
AND  -- Logical conjunction (both must be true)
OR   -- Logical disjunction (at least one must be true)  
NOT  -- Logical negation (inverts boolean value)
```

**Truth Tables**:
```
AND | T | F | NULL      OR | T | F | NULL     NOT
----+---+---+------     ---+---+---+------     ---+------
 T  | T | F | NULL      T  | T | T | T         T  | F
 F  | F | F | F         F  | T | F | NULL      F  | T  
NULL|NULL| F | NULL     NULL| T |NULL| NULL     NULL| NULL
```

**Examples**:
```sql
-- Precedence: AND binds tighter than OR
SELECT * FROM products
WHERE category = 'books' AND in_stock = true
   OR category = 'ebooks';  -- Parsed as: (category='books' AND in_stock) OR category='ebooks'

-- Use parentheses for clarity
SELECT * FROM products  
WHERE category IN ('books', 'ebooks') 
  AND (in_stock = true OR downloadable = true);
```

### Type Cast Operators

**PostgreSQL-style cast**:
```sql
expression::type
```

**SQL standard cast**:
```sql
CAST(expression AS type)
```

**Examples**:
```sql
SELECT 
    '42'::INTEGER,                      -- PostgreSQL style
    CAST('3.14' AS DECIMAL(5,2)),      -- SQL standard
    timestamp_col::DATE,                -- Extract date part
    CAST(json_col->>'amount' AS NUMERIC) -- JSON extraction with cast
FROM data;
```

## Runtime Evaluation vs Parsing

### Currently Runtime-Evaluated

The expression evaluator (`src/engine/expr.cpp`, `evaluate_predicate`) currently supports:

- **Boolean operators**: `AND`, `OR`, `NOT`
- **Comparisons**: `=`, `!=`, `<`, `<=`, `>`, `>=`  
- **NULL checks**: `IS NULL`, `IS NOT NULL`
- **Literals**: Integer and string literals
- **Column references**: Identifier resolution
- **Parentheses**: Grouping for precedence override

### Parser-Only (Not Yet Runtime)

These operators are parsed and normalized but not yet evaluated at runtime:

- **Pattern matching**: `LIKE`, `SIMILAR TO`
- **Membership**: `IN`, `EXISTS`
- **Range**: `BETWEEN`
- **String ops**: `||` concatenation
- **Arithmetic**: `+`, `-`, `*`, `/`, `%`
- **Advanced NULL**: `IS DISTINCT FROM`

**Note**: This distinction is important for understanding query behavior. The parser accepts these operators for syntax validation, but runtime execution may not yet support them.

## Complex Expression Examples

### Mixing Operators with Correct Precedence
```sql
-- Arithmetic and comparison
SELECT * FROM items
WHERE quantity * price > 100.00  -- Multiplication before comparison
  AND tax_rate + 1 <= 1.10;      -- Addition before comparison

-- String concatenation and pattern matching  
SELECT * FROM logs
WHERE 'Error: ' || error_code LIKE 'Error: 5%'  -- Concatenation before LIKE
  AND severity::INTEGER >= 3;                    -- Cast before comparison

-- Complex boolean logic
SELECT * FROM users
WHERE active = true 
  AND (role = 'admin' OR role = 'moderator' AND verified = true)
  -- Parses as: active AND (role='admin' OR (role='moderator' AND verified))
  -- AND binds tighter than OR
```

### Using COLLATE for String Comparisons
```sql
-- Case-insensitive comparison
SELECT * FROM products
WHERE name COLLATE "unicode_ci" = 'Widget';

-- Locale-specific sorting
SELECT name FROM users
ORDER BY name COLLATE "en_US";
```

## Implementation Details

**Parser Implementation** (`src/engine/parser_expr.cpp`):
- Uses Pratt parsing with binding powers (`lbp` function)
- Handles prefix operators via `nud` (null denotation)
- Processes infix operators via `led` (left denotation)
- Normalizes expressions to canonical forms

**Runtime Evaluator** (`src/engine/expr.cpp`):
- `evaluate_predicate`: Main entry point for boolean expressions
- `compile_predicate`: Converts to postfix notation for efficient evaluation
- `evaluate_predicate_compiled`: Evaluates compiled postfix expressions
- Uses stack-based evaluation for operator precedence

**Code Anchors**:
- Operator precedence: `src/engine/parser_expr.cpp` (lbp function, lines 62-91)
- Runtime evaluation: `src/engine/expr.cpp` (evaluate_predicate)
- Expression normalization: `src/engine/parser_expr.cpp` (expr_bp)

## See also

- [Lexical Structure](./sql-lexical.md) - Token recognition for operators
- [Data Types](./sql-data-types.md) - Type casting and conversions
- [SELECT Statements](./sql-select.md) - Using operators in queries
- [SQL Overview](./sql-overview.md) - Overall language structure
