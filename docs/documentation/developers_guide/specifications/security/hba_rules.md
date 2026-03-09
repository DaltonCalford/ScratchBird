# Specification: HBA (Host-Based Authentication) Rules

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/authentication/hba |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/security/auth_manager.cpp:58-176`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/security/auth_manager.h:48-176`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_security_issues.cpp`

## Synopsis

This specification defines the Host-Based Authentication (HBA) rule system for ScratchBird, including rule format, connection type matching, database/user matching, address matching, and authentication method selection. HBA rules control which clients can connect and how they must authenticate.

## Scope

### In Scope

- HBA rule file format (pg_hba.conf)
- Connection type matching (local, host, hostssl, etc.)
- Database name matching (including special values)
- User name matching (including special values)
- IP address and CIDR matching
- Authentication method selection
- Rule precedence and first-match semantics

### Out of Scope

- Authentication plugin internals (see `auth_plugins.md`)
- SSL/TLS configuration (see `ssl_tls.md`)
- User/role management

## Background

HBA rules provide the first line of defense in ScratchBird authentication. Each incoming connection is matched against rules in order, and the first matching rule determines the authentication method required.

## Specification

### HBA Rule Format

```
# Connection  Database    User        Address         Method    [Options]
local         database    user                        method    [options]
host          database    user        address         method    [options]
hostssl       database    user        address         method    [options]
hostnossl     database    user        address         method    [options]
hostgssenc    database    user        address         method    [options]
```

### Data Structures

```cpp
// From /home/dcalford/CliWork/ScratchBird/include/scratchbird/security/auth_manager.h:51-114

enum class HBAConnectionType : uint8_t {
    LOCAL = 0,      // Unix domain socket
    HOST = 1,       // TCP/IP (with or without SSL)
    HOSTSSL = 2,    // TCP/IP with SSL required
    HOSTNOSSL = 3,  // TCP/IP without SSL
    HOSTGSSENC = 4  // TCP/IP with GSSAPI encryption
};

enum class IPMatchType : uint8_t {
    ANY = 0,        // Match any address
    SINGLE = 1,     // Single IP address
    CIDR = 2,       // CIDR notation (address/prefix)
    SAMEHOST = 3,   // Same host (127.0.0.1/::1)
    SAMENET = 4     // Same subnet
};

struct HBARule {
    // Connection type
    HBAConnectionType connection_type = HBAConnectionType::HOST;

    // Database matching
    std::string database;           // Database name, "all", "sameuser", "samerole", "@file"
    std::vector<std::string> databases;  // Expanded database list (from @file)
    bool database_is_regex = false;
    std::regex database_regex;

    // User matching
    std::string user;               // Username, "all", "+role", "@file"
    std::vector<std::string> users; // Expanded user list
    bool user_is_regex = false;
    std::regex user_regex;

    // Address matching
    IPMatchType ip_match_type = IPMatchType::ANY;
    std::string address;            // IP address or "all", "samehost", "samenet"
    std::string netmask;            // Netmask (CIDR or dotted)
    uint8_t prefix_length = 0;      // CIDR prefix length

    // Authentication
    AuthType auth_type = AuthType::REJECT;
    std::map<std::string, std::string> auth_options;

    // Rule metadata
    int line_number = 0;            // Line in pg_hba.conf
    std::string comment;            // Comment for documentation

    bool matches(const ConnectionInfo& conn, const std::string& username,
                 const std::string& database, const std::vector<std::string>& roles) const;
    
    static core::Status parse(const std::string& line, HBARule& rule,
                               core::ErrorContext* ctx = nullptr);
};
```

### Connection Types

| Type | Description | SSL Required | Network |
|------|-------------|--------------|---------|
| `local` | Unix domain socket | N/A | Local only |
| `host` | TCP/IP connection | Optional | Any |
| `hostssl` | TCP/IP with SSL | Required | Any |
| `hostnossl` | TCP/IP without SSL | Forbidden | Any |
| `hostgssenc` | TCP/IP with GSSAPI encryption | N/A | Any |

### Database Matching

| Pattern | Matches |
|---------|---------|
| `all` | All databases |
| `sameuser` | Database with same name as user |
| `samerole` | Database where user has membership |
| `replication` | Physical replication connections |
| `@filename` | Databases listed in file |
| `dbname` | Specific database name |
| `/regex/` | Database names matching regex |

### User Matching

| Pattern | Matches |
|---------|---------|
| `all` | All users |
| `+rolename` | Members of role |
| `@filename` | Users listed in file |
| `username` | Specific user |
| `/regex/` | Usernames matching regex |

### Address Matching

| Pattern | IPv4 | IPv6 | Description |
|---------|------|------|-------------|
| `all` | ✅ | ✅ | Any address |
| `samehost` | ✅ | ✅ | Local addresses only |
| `samenet` | ✅ | ✅ | Same subnet as server |
| `ipaddress` | ✅ | ✅ | Single IP address |
| `ip/mask` | ✅ | ✅ | CIDR notation |
| `ip mask` | ✅ | ❌ | IP with netmask |

### Authentication Methods

| Method | Description | Arguments |
|--------|-------------|-----------|
| `trust` | Allow without authentication | None |
| `reject` | Reject connection | None |
| `password` | Plaintext password | - |
| `md5` | MD5 hashed password | - |
| `scram-sha-256` | SCRAM-SHA-256 | - |
| `scram-sha-512` | SCRAM-SHA-512 | - |
| `ident` | Ident protocol (RFC 1413) | `map=name` |
| `peer` | Unix peer credentials | `map=name` |
| `ldap` | LDAP authentication | `ldapserver=...` |
| `gss` | GSSAPI/Kerberos | `include_realm=0/1`, `map=name` |
| `sspi` | Windows SSPI | `include_realm=0/1` |
| `pam` | PAM authentication | `pamservice=name` |
| `radius` | RADIUS authentication | `radiusserver=...` |
| `cert` | Client certificate | `map=name`, `verify-full` |
| `jwt` | JWT Bearer token | `jwks_url=...` |
| `oauth` | OAuth token | `issuer=...` |

### Interface Contracts

#### Function: `HBARule::parse()`

```cpp
// Source: auth_manager.cpp
static core::Status parse(const std::string& line, HBARule& rule,
                          core::ErrorContext* ctx = nullptr);
```

**Preconditions:**
- Line is non-empty
- Line is not a comment-only line

**Postconditions:**
- On success: rule is populated with parsed values
- On failure: ctx contains error details

**Algorithm:**
```
Input: line
Output: HBARule or error

1. SKIP comments (text after #)
2. SPLIT line by whitespace
3. VALIDATE minimum fields:
   - local: needs 4 fields
   - host*: needs 5 fields
4. PARSE connection_type from first field
5. PARSE database field:
   - If starts with @: read from file
   - If starts and ends with /: compile regex
   - Else: literal value
6. PARSE user field:
   - If starts with @: read from file
   - If starts with +: role membership
   - If regex: compile pattern
   - Else: literal value
7. PARSE address (for host* types):
   - all: IPMatchType::ANY
   - samehost: IPMatchType::SAMEHOST
   - samenet: IPMatchType::SAMENET
   - CIDR: parse prefix length
   - IP+mask: parse dotted netmask
8. PARSE auth method and options
9. RETURN rule
```

#### Function: `HBARule::matches()`

```cpp
// Source: auth_manager.cpp:106-107
bool matches(const ConnectionInfo& conn, const std::string& username,
             const std::string& database, const std::vector<std::string>& roles) const;
```

**Preconditions:**
- Connection info is populated
- Username is provided
- Database is provided

**Postconditions:**
- Returns true if rule matches connection

**Algorithm:**
```
Input: conn, username, database, roles
Output: Boolean match

1. CHECK connection_type matches
   - LOCAL: must be Unix socket
   - HOST: any TCP
   - HOSTSSL: TCP with SSL
   - HOSTNOSSL: TCP without SSL
   - HOSTGSSENC: TCP with GSSAPI

2. CHECK database matches
   - "all": always true
   - "sameuser": database == username
   - "samerole": database in user's roles
   - File list: database in file contents
   - Regex: regex_match(database)
   - Literal: database == value

3. CHECK user matches
   - "all": always true
   - "+role": user has role membership
   - File list: username in file contents
   - Regex: regex_match(username)
   - Literal: username == value

4. CHECK address matches (for TCP connections)
   - ANY: always true
   - SAMEHOST: address is 127.0.0.1 or ::1
   - SAMENET: same subnet
   - CIDR: ip_matches_cidr(address, network, prefix)
   - SINGLE: address == specified_ip

5. RETURN (connection AND database AND user AND address)
```

### IP Matching Algorithm

```cpp
// Source: auth_manager.cpp:72-92
static bool matchIPv4(const struct in_addr& addr, const struct in_addr& network,
                      uint8_t prefix_length)
{
    uint32_t mask = prefix_length == 0 ? 0 : htonl(~((1 << (32 - prefix_length)) - 1));
    return (addr.s_addr & mask) == (network.s_addr & mask);
}

static bool matchIPv6(const struct in6_addr& addr, const struct in6_addr& network,
                      uint8_t prefix_length)
{
    for (int i = 0; i < 16; i++) {
        int bits = std::min(8, static_cast<int>(prefix_length) - i * 8);
        if (bits <= 0) break;

        uint8_t mask = bits == 8 ? 0xFF : static_cast<uint8_t>(~((1 << (8 - bits)) - 1));
        if ((addr.s6_addr[i] & mask) != (network.s6_addr[i] & mask)) {
            return false;
        }
    }
    return true;
}
```

### Rule Precedence

```
Rules are evaluated in file order (line number order).
FIRST matching rule wins - no further evaluation.

Best practice: Most specific rules first, general rules last.

Example ordering:
1. hostssl database user 10.0.0.5/32 scram-sha-256  # Specific IP
2. hostssl database user 10.0.0.0/24 scram-sha-256  # Subnet
3. host    database user all       scram-sha-256    # All addresses
4. local    all      all           peer              # Local fallback
```

### Decision Tree

```
Connection Request
│
├─ Parse connection info (type, address, SSL status)
│
├─ For each HBA rule in order:
│   ├─ Check connection_type matches? ──No──► Continue to next rule
│   │
│   ├─ Check database matches? ──No──► Continue to next rule
│   │   ├─ all: match
│   │   ├─ sameuser: db == user
│   │   ├─ samerole: role check
│   │   ├─ @file: file lookup
│   │   └─ literal/regex: pattern match
│   │
│   ├─ Check user matches? ──No──► Continue to next rule
│   │   ├─ all: match
│   │   ├─ +role: membership check
│   │   ├─ @file: file lookup
│   │   └─ literal/regex: pattern match
│   │
│   ├─ Check address matches? ──No──► Continue to next rule
│   │   ├─ all: match
│   │   ├─ samehost: localhost check
│   │   ├─ samenet: subnet check
│   │   ├─ CIDR: prefix match
│   │   └─ single: exact match
│   │
│   └─ MATCH FOUND! Return auth method
│
└─ No rule matches ──► Connection rejected
```

## Example pg_hba.conf

```conf
# TYPE    DATABASE    USER        ADDRESS         METHOD      OPTIONS

# Local connections - trust for Unix socket
local     all         all                         peer

# Replication connections
local     replication all                         peer
hostssl   replication all         10.0.0.0/24     scram-sha-256

# Admin access from management network
hostssl   all         +admin      10.0.1.0/24     scram-sha-256

# Application servers
hostssl   app_db      app_user    10.0.2.0/24     scram-sha-256

# Read-only reporting
hostssl   all         +reporting  10.0.3.0/24     scram-sha-256

# LDAP authentication for corporate users
hostssl   all         @corp_users all             ldap ldapserver=ldap.corp.com

# Certificate auth for service accounts
hostssl   all         +services   all             cert

# Reject all other connections
host      all         all         all             reject
```

## Invariants

1. **First Match Wins**: Only the first matching rule determines authentication
   - Verification: Sequential evaluation stops on first match

2. **Local Only for Peer**: `peer` method only works with `local` connections
   - Verification: Connection type check in matcher

3. **SSL Required for Cert**: `cert` method requires `hostssl` or SSL connection
   - Verification: SSL status checked during auth

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `SYNTAX_ERROR` | Invalid rule format | Fix pg_hba.conf syntax |
| `INVALID_ADDRESS` | Bad IP or CIDR | Correct address format |
| `FILE_NOT_FOUND` | @file doesn't exist | Create file or fix path |
| `REGEX_ERROR` | Invalid regex pattern | Fix regex syntax |

## Reload Behavior

```
SIGHUP or explicit reload:
1. Parse new pg_hba.conf
2. If parse errors: Keep old config, log errors
3. If parse success: Atomically swap configs
4. Existing connections: Use old auth (already established)
5. New connections: Use new rules
```

## Related Specifications

- `authentication_flow.md` - Complete authentication flow
- `auth_plugins.md` - Authentication plugin details
- `ssl_tls.md` - SSL/TLS configuration

## Appendix

### HBA File Format Grammar

```
line ::= (comment | blank | rule)

comment ::= '#' text '\n'

blank ::= whitespace '\n'

rule ::= connection_type whitespace database whitespace user 
         [whitespace address] whitespace method [options] '\n'

connection_type ::= 'local' | 'host' | 'hostssl' | 'hostnossl' | 'hostgssenc'

database ::= 'all' | 'sameuser' | 'samerole' | 'replication' 
           | '@' filename | '/' regex '/' | identifier

user ::= 'all' | '+' rolename | '@' filename | '/' regex '/' | identifier

address ::= 'all' | 'samehost' | 'samenet' 
          | ipv4_address ['/' prefix_length]
          | ipv6_address ['/' prefix_length]
          | ipv4_address ipv4_mask

method ::= 'trust' | 'reject' | 'password' | 'md5' 
         | 'scram-sha-256' | 'scram-sha-512'
         | 'ident' | 'peer' | 'ldap' | 'gss' | 'sspi'
         | 'pam' | 'radius' | 'cert' | 'jwt' | 'oauth'

options ::= [option [whitespace options]]
option ::= key '=' value
```

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
