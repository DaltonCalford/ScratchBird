# Security Testing Procedures

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**[← Back to Test Server Specification](README.md)**

## Overview

This document provides comprehensive security testing procedures for validating ScratchBird's authentication, authorization, and security features using the Test Server.

## Prerequisites

- Test server running with HBA enabled
- Real users created (for SCRAM testing)
- Access to server logs
- Network testing tools (`nc`, `telnet`, custom scripts)

## Test Categories

| Category | Tests | Priority |
|----------|-------|----------|
| Bootstrap Auth | 3 | High |
| SCRAM-SHA-256 | 8 | Critical |
| HBA Enforcement | 6 | Critical |
| Rate Limiting | 4 | High |
| Password Policy | 5 | Medium |
| TLS/SSL | 4 | Medium |

## Test Procedures

### Category 1: Bootstrap Authentication

**Purpose:** Verify bootstrap mode works correctly for initial setup

#### Test 1.1: Bootstrap Acceptance
**Objective:** Verify any credentials work in bootstrap mode

**Steps:**
1. Ensure no real users exist (fresh database)
2. Connect with arbitrary username: `testuser123`
3. Use arbitrary password: `wrongpassword`
4. Execute query: `SELECT CURRENT_USER`

**Expected Result:**
- Connection accepted
- Query returns: `SYSTEM`
- User has superuser privileges

**Verification:**
```bash
# Connect via TCP
echo "SELECT CURRENT_USER" | nc 127.0.0.1 3092

# Or via Unix socket
echo "SELECT CURRENT_USER" | nc -U build/ipc/scratchbird*.sock
```

#### Test 1.2: Bootstrap with Special Characters
**Objective:** Verify bootstrap handles special characters

**Steps:**
1. Connect with username: `user'; DROP TABLE users; --`
2. Use password: `pass"><script>alert(1)</script>`

**Expected Result:**
- Connection accepted
- No SQL injection possible
- No XSS vulnerability

#### Test 1.3: Bootstrap to Real User Transition
**Objective:** Verify server switches from bootstrap to real auth

**Steps:**
1. Connect with bootstrap (any credentials)
2. Create real user: `CREATE USER realuser PASSWORD 'RealPass2026!'`
3. Disconnect
4. Connect with arbitrary credentials again

**Expected Result:**
- Connection rejected (real auth now required)
- Error: "authentication failed"

### Category 2: SCRAM-SHA-256 Authentication

**Purpose:** Verify RFC 5802 / RFC 7677 compliance

#### Test 2.1: SCRAM Challenge-Response Flow
**Objective:** Verify proper SCRAM exchange

**Steps:**
1. Ensure real users exist
2. Initiate connection with SCRAM-SHA-256
3. Capture server first message (contains salt)
4. Send client first message
5. Receive server final message
6. Send client final message (proof)

**Expected Result:**
- Each message follows SCRAM format
- Salt is unique per session
- Proof verification succeeds

**Verification:**
```
Client First:  n,,n=user,r=clientnonce
Server First:  r=clientnonceservernonce,s=c2FsdA==,i=4096
Client Final:  c=biws,r=clientnonceservernonce,p=base64proof
Server Final:  v=base64verification
```

#### Test 2.2: SCRAM Salt Uniqueness
**Objective:** Verify salts are unique per session

**Steps:**
1. Connect and capture salt from server first message
2. Disconnect
3. Connect again with same user
4. Capture new salt

**Expected Result:**
- Salts are different (cryptographically random)
- No salt reuse detected

#### Test 2.3: SCRAM Replay Attack Prevention
**Objective:** Verify SCRAM prevents replay attacks

**Steps:**
1. Capture complete SCRAM exchange (valid login)
2. Replay exact same client messages
3. Attempt authentication

**Expected Result:**
- Server rejects replay (different nonce required)
- Authentication fails

#### Test 2.4: SCRAM Server Authentication
**Objective:** Verify server proves its identity to client

**Steps:**
1. Complete SCRAM authentication
2. Verify server sends `v=` (verification) message
3. Validate server signature

**Expected Result:**
- Server provides verifiable proof
- Client can detect man-in-the-middle attacks

#### Test 2.5: SCRAM Invalid Password
**Objective:** Verify authentication fails with wrong password

**Steps:**
1. Attempt login with correct user, wrong password
2. Complete full SCRAM exchange

**Expected Result:**
- SCRAM exchange completes without error
- Server final message indicates failure
- No information leaked about password

#### Test 2.6: SCRAM Non-Existent User
**Objective:** Verify timing attack resistance

**Steps:**
1. Attempt login with non-existent user
2. Measure response time
3. Attempt login with existing user, wrong password
4. Measure response time
5. Compare times

**Expected Result:**
- Times are similar (within 10%)
- No user enumeration via timing

#### Test 2.7: SCRAM Iteration Count
**Objective:** Verify iteration count is appropriate

**Steps:**
1. Capture server first message
2. Extract iteration count (`i=` parameter)

**Expected Result:**
- Minimum 4096 iterations (OWASP recommendation)
- Higher is acceptable

#### Test 2.8: SCRAM Channel Binding (if supported)
**Objective:** Verify channel binding for TLS connections

**Steps:**
1. Connect via TLS
2. Request SCRAM-SHA-256-PLUS
3. Verify channel binding data included

**Expected Result:**
- Channel binding data present
- Protects against stolen credential attacks

### Category 3: HBA Rule Enforcement

**Purpose:** Verify Host-Based Authentication rules

#### Test 3.1: Local Connection Acceptance
**Objective:** Verify local connections allowed

**Steps:**
1. Connect from 127.0.0.1
2. Connect via Unix socket
3. Connect from ::1 (IPv6)

**Expected Result:**
- All connections accepted
- SCRAM authentication proceeds

#### Test 3.2: Remote Connection Rejection
**Objective:** Verify remote connections blocked

**Steps:**
1. From another machine: `nc <server-ip> 3092`
2. Attempt connection

**Expected Result:**
- Connection refused or rejected
- HBA rule triggers rejection

#### Test 3.3: HBA Rule Ordering
**Objective:** Verify first matching rule wins

**Steps:**
1. Add rule: `host all all 127.0.0.1/32 reject`
2. Add rule: `host all all 127.0.0.1/32 scram-sha-256`
3. Attempt connection from 127.0.0.1

**Expected Result:**
- Connection rejected (first rule matches)

#### Test 3.4: Database-Specific Rules
**Objective:** Verify per-database HBA rules

**Steps:**
1. Add rule: `host testdb user1 127.0.0.1/32 scram-sha-256`
2. Add rule: `host proddb all 127.0.0.1/32 reject`
3. Connect to testdb as user1
4. Connect to proddb as user1

**Expected Result:**
- testdb: Accepted
- proddb: Rejected

#### Test 3.5: User-Specific Rules
**Objective:** Verify per-user HBA rules

**Steps:**
1. Add rule: `host all admin 127.0.0.1/32 scram-sha-256`
2. Add rule: `host all guest 127.0.0.1/32 reject`
3. Connect as admin
4. Connect as guest

**Expected Result:**
- admin: Accepted
- guest: Rejected

#### Test 3.6: SSL-Only Rules
**Objective:** Verify SSL enforcement

**Steps:**
1. Add rule: `hostssl all all 127.0.0.1/32 scram-sha-256`
2. Connect without SSL
3. Connect with SSL

**Expected Result:**
- Without SSL: Rejected
- With SSL: Accepted

### Category 4: Rate Limiting

**Purpose:** Verify brute force attack protection

#### Test 4.1: Failed Attempt Tracking
**Objective:** Verify failed attempts are tracked

**Steps:**
1. Attempt login with wrong password (1st)
2. Attempt login with wrong password (2nd)
3. Continue to 5th attempt
4. Check server logs

**Expected Result:**
- Each failure logged
- User account tracked

#### Test 4.2: Account Lockout
**Objective:** Verify lockout after max failures

**Steps:**
1. Fail login 5 times for user "testuser"
2. Attempt 6th login with correct password

**Expected Result:**
- 6th attempt rejected
- Error: "account temporarily locked"

#### Test 4.3: Lockout Duration
**Objective:** Verify lockout expires correctly

**Steps:**
1. Trigger lockout (5 failed attempts)
2. Wait lockout duration (default: 300 seconds)
3. Attempt login with correct password

**Expected Result:**
- Login succeeds after wait

#### Test 4.4: Successful Reset
**Objective:** Verify successful login resets counter

**Steps:**
1. Fail login 3 times
2. Login successfully
3. Fail login 2 more times
4. Attempt login with correct password

**Expected Result:**
- Counter reset on success
- Login succeeds (not locked)

### Category 5: Password Policy

**Purpose:** Verify password strength requirements

#### Test 5.1: Minimum Length Enforcement
**Objective:** Verify minimum password length

**Steps:**
1. Attempt to create user with password: "short"
2. Attempt with password: "adequate123"

**Expected Result:**
- "short": Rejected
- "adequate123": Accepted (if meets policy)

#### Test 5.2: Complexity Requirements
**Objective:** Verify password complexity

**Steps:**
1. Attempt password: "password123" (common)
2. Attempt password: "Tr0ub4dor&3" (complex)

**Expected Result:**
- Common password: Rejected
- Complex password: Accepted

#### Test 5.3: Password History
**Objective:** Verify password reuse prevention

**Steps:**
1. Create user with password: "OldPass2026!"
2. Change password to: "NewPass2026!"
3. Attempt to change back to: "OldPass2026!"

**Expected Result:**
- Reuse rejected (if history enabled)

#### Test 5.4: Password Expiration
**Objective:** Verify expiration enforcement

**Steps:**
1. Create user with expired password flag
2. Attempt login
3. Attempt query execution

**Expected Result:**
- Login may succeed
- Query rejected with "password expired" error

#### Test 5.5: bcrypt Hash Verification
**Objective:** Verify password hashing

**Steps:**
1. Create user with known password
2. Query system catalog for password hash
3. Verify hash format: `$2a$10$...`

**Expected Result:**
- Hash uses bcrypt algorithm
- Salt is present
- Cost factor >= 10

### Category 6: TLS/SSL Security

**Purpose:** Verify transport layer security

#### Test 6.1: Certificate Validation
**Objective:** Verify TLS certificate handling

**Steps:**
1. Connect with valid client certificate
2. Connect with invalid/expired certificate
3. Connect without certificate (if required)

**Expected Result:**
- Valid: Accepted
- Invalid: Rejected
- Missing (if required): Rejected

#### Test 6.2: Protocol Version Enforcement
**Objective:** Verify minimum TLS version

**Steps:**
1. Attempt connection with TLS 1.0
2. Attempt connection with TLS 1.1
3. Attempt connection with TLS 1.2
4. Attempt connection with TLS 1.3

**Expected Result:**
- TLS 1.0/1.1: Rejected (if configured)
- TLS 1.2/1.3: Accepted

#### Test 6.3: Cipher Suite Strength
**Objective:** Verify strong cipher suites

**Steps:**
1. Connect with weak cipher: `RC4-SHA`
2. Connect with strong cipher: `ECDHE-RSA-AES256-GCM-SHA384`

**Expected Result:**
- Weak cipher: Rejected
- Strong cipher: Accepted

#### Test 6.4: Certificate Pinning (if supported)
**Objective:** Verify certificate pinning

**Steps:**
1. Connect with pinned certificate
2. Connect with different valid certificate

**Expected Result:**
- Pinned: Accepted
- Different: Rejected

## Automated Security Testing

### Script for SCRAM Testing

```bash
#!/bin/bash
# scram_test.sh - Automated SCRAM-SHA-256 testing

HOST="127.0.0.1"
PORT="3092"
USER="testuser"
PASS="TestPass2026!"

echo "=== SCRAM-SHA-256 Security Test ==="

# Test 1: Valid authentication
echo "Test 1: Valid SCRAM authentication..."
# (Implementation depends on client library)
# Expected: SUCCESS

# Test 2: Invalid password
echo "Test 2: Invalid password rejection..."
# Expected: FAILURE

# Test 3: Non-existent user
echo "Test 3: Non-existent user handling..."
# Expected: FAILURE (same timing as invalid password)

echo "=== Tests Complete ==="
```

### Script for Rate Limit Testing

```bash
#!/bin/bash
# rate_limit_test.sh - Automated rate limiting test

HOST="127.0.0.1"
PORT="3092"
USER="testuser"
WRONG_PASS="wrongpassword"

echo "=== Rate Limiting Test ==="

# Attempt 5 failed logins
for i in {1..5}; do
    echo "Attempt $i: Invalid password..."
    # Connect with wrong password
    echo "SELECT 1" | nc $HOST $PORT
    sleep 1
done

# 6th attempt should be locked
echo "Attempt 6: Should be locked..."
echo "SELECT 1" | nc $HOST $PORT

# Wait 5 minutes and try again
echo "Waiting 5 minutes for lockout to expire..."
sleep 300
echo "Attempt after wait: Should succeed..."
# Connect with correct password

echo "=== Test Complete ==="
```

## Security Checklist

- [ ] Bootstrap mode works correctly
- [ ] Bootstrap mode transitions to real auth
- [ ] SCRAM-SHA-256 challenge-response flow correct
- [ ] SCRAM salts are unique
- [ ] SCRAM prevents replay attacks
- [ ] SCRAM timing attack resistant
- [ ] HBA local connections accepted
- [ ] HBA remote connections rejected
- [ ] HBA rule ordering correct
- [ ] Rate limiting tracks failures
- [ ] Account lockout triggers correctly
- [ ] Lockout expires correctly
- [ ] Password policy enforced
- [ ] bcrypt hashing used
- [ ] TLS connections accepted (if enabled)
- [ ] Weak TLS rejected (if configured)

## Reporting Security Issues

If you discover security vulnerabilities during testing:

1. Document the issue with reproduction steps
2. Note the severity and potential impact
3. Report to the security team
4. Do not disclose publicly until fixed

## See Also

- [Test Server Specification](README.md) - Overview
- [Test Server Operations](OPERATIONS.md) - Operational procedures
- [SCRAM-SHA-256 RFC 5802](https://tools.ietf.org/html/rfc5802)
- [SCRAM-SHA-256 RFC 7677](https://tools.ietf.org/html/rfc7677)
- [OWASP Password Storage](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html)

---

**Last Updated:** February 2026  
**Security Classification:** Internal Use
