# Redis RESP Command Specification

**Status:** Draft (Beta)

## 1. Purpose

Define the Redis Serialization Protocol (RESP) and core command grammar for
key-value operations. This spec targets protocol emulation and parser design.

## 2. RESP Basics

- **Simple String**: `+OK\r\n`
- **Error**: `-ERR message\r\n`
- **Integer**: `:1000\r\n`
- **Bulk String**: `$<len>\r\n<data>\r\n`
- **Array**: `*<count>\r\n<elem>...`

## 3. EBNF (RESP)

```ebnf
resp          = simple_string | error | integer | bulk_string | array ;

simple_string = "+" text "\r\n" ;
error         = "-" text "\r\n" ;
integer       = ":" number "\r\n" ;

bulk_string   = "$" number "\r\n" [ data "\r\n" ] ;
array         = "*" number "\r\n" { resp } ;
```

## 4. Command Grammar

```ebnf
command       = array ;
command_name  = bulk_string ;
args          = bulk_string { bulk_string } ;
```

## 5. Core Command Families

- **Strings**: `GET`, `SET`, `MGET`, `MSET`, `INCR`, `DECR`
- **Hashes**: `HGET`, `HSET`, `HGETALL`, `HDEL`
- **Lists**: `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LRANGE`
- **Sets**: `SADD`, `SREM`, `SMEMBERS`, `SINTER`
- **Sorted Sets**: `ZADD`, `ZRANGE`, `ZREM`, `ZRANK`
- **Keys**: `DEL`, `EXPIRE`, `TTL`, `KEYS`, `SCAN`

## 6. Semantics

- Commands are case-insensitive.
- `SET` supports options (`EX`, `PX`, `NX`, `XX`).
- TTL is in seconds unless using `PEXPIRE` (milliseconds).
- RESP arrays represent commands with first element as the command name.

## 7. Usage Guidance

**When to use RESP**
- High-throughput key-value operations.
- Simple data structures (lists, sets, hashes).

**Avoid when**
- Complex joins or relational data modeling is required.

## 8. Examples

**Command encoding**
```
*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n
```

**SET with expiry**
```
*5\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n$2\r\nEX\r\n$2\r\n60\r\n
```

