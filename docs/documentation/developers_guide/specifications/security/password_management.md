# Specification: Password Management

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/password |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/security/password_policy.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/security/password_policy.h:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_security_issues.cpp`

## Synopsis

This specification defines password management in ScratchBird, including password policies, complexity requirements, password history, expiration, account lockout, and common password checking.

## Scope

### In Scope

- Password policy configuration
- Password complexity validation
- Password history and reuse prevention
- Password expiration and grace periods
- Account lockout on failed attempts
- Common password checking
- Password state management

### Out of Scope

- Password hashing algorithms (handled in SCRAM/auth)
- Encryption at rest
- Session management

## Background

ScratchBird implements a comprehensive password management system that enforces organizational security policies while maintaining usability. The system supports configurable policies with sensible defaults.

## Specification

### Data Structures

```cpp
// From /home/dcalford/CliWork/ScratchBird/include/scratchbird/security/password_policy.h:44-85

struct PasswordPolicy {
    // Expiration settings
    uint32_t expiration_days = 90;            // Days until password expires (0 = never)
    uint32_t grace_period_days = 7;           // Days after expiration before lockout
    uint32_t warning_days = 14;               // Days before expiration to warn user

    // History settings
    uint32_t history_count = 5;               // Number of old passwords to remember
    bool prevent_reuse = true;                // Prevent reuse of old passwords

    // Complexity requirements (optional)
    bool enforce_complexity = false;
    uint32_t min_length = 8;                  // Minimum password length
    uint32_t min_uppercase = 0;               // Minimum uppercase characters
    uint32_t min_lowercase = 0;               // Minimum lowercase characters
    uint32_t min_digits = 0;                  // Minimum numeric characters
    uint32_t min_special = 0;                 // Minimum special characters

    // Account lockout
    uint32_t max_failed_attempts = 5;         // Lock after N failed attempts
    uint32_t lockout_duration_minutes = 30;   // Minutes to lock account

    static PasswordPolicy defaultPolicy();
    static PasswordPolicy strictPolicy();
};

struct PasswordState {
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point expires_at;
    std::chrono::system_clock::time_point last_changed;
    std::vector<std::string> password_history;
    uint32_t failed_attempts = 0;
    std::chrono::system_clock::time_point locked_until;
    bool force_change = false;
    bool never_expires = false;

    bool isExpired() const;
    bool isInGracePeriod(uint32_t grace_days) const;
    bool shouldWarn(uint32_t warning_days) const;
    bool isLocked() const;
    int32_t daysUntilExpiration() const;
};

enum class PasswordValidationError : uint8_t {
    NONE = 0,
    TOO_SHORT,
    MISSING_UPPERCASE,
    MISSING_LOWERCASE,
    MISSING_DIGIT,
    MISSING_SPECIAL,
    REUSED_PASSWORD,
    COMMON_PASSWORD,
    CONTAINS_USERNAME,
};

struct PasswordValidationResult {
    bool valid = true;
    std::vector<PasswordValidationError> errors;
    std::string message;

    operator bool() const { return valid; }
    static PasswordValidationResult success();
    static PasswordValidationResult failure(PasswordValidationError error, std::string_view msg);
    void addError(PasswordValidationError error, std::string_view msg);
};
```

### Password Policy Manager

```cpp
// From /home/dcalford/CliWork/ScratchBird/include/scratchbird/security/password_policy.h:180-266

class PasswordPolicyManager {
public:
    static PasswordPolicyManager& getInstance();
    
    void setPolicy(const PasswordPolicy& policy);
    const PasswordPolicy& getPolicy() const;
    
    // Validation
    PasswordValidationResult validatePassword(
        std::string_view password,
        std::string_view username,
        const std::vector<std::string>& password_history = {}) const;
    
    bool meetsComplexityRequirements(std::string_view password) const;
    bool isPasswordReused(
        std::string_view password_hash,
        const std::vector<std::string>& history) const;
    
    // State Management
    PasswordState createPasswordState(bool never_expires = false) const;
    PasswordState updatePasswordState(
        const PasswordState& current,
        std::string_view old_password_hash) const;
    
    // Login Handling
    enum class LoginCheckResult : uint8_t {
        SUCCESS,
        PASSWORD_EXPIRED,
        PASSWORD_EXPIRED_GRACE,
        PASSWORD_EXPIRED_LOCKED,
        PASSWORD_CHANGE_REQUIRED,
        ACCOUNT_LOCKED,
        INVALID_CREDENTIALS,
    };
    
    LoginCheckResult checkLogin(
        const PasswordState& state,
        bool credentials_valid) const;
    
    PasswordState recordFailedAttempt(PasswordState state) const;
    PasswordState recordSuccessfulLogin(PasswordState state) const;
    PasswordState forcePasswordChange(PasswordState state) const;
    PasswordState unlockAccount(PasswordState state) const;
    
    // Hashing
    std::string hashPassword(std::string_view password) const;
    bool verifyPassword(std::string_view password, std::string_view hash) const;
    std::string generateSalt() const;
};
```

### Interface Contracts

#### Function: `validatePassword()`

```cpp
// Source: password_policy.cpp:42-121
PasswordValidationResult validatePassword(
    std::string_view password,
    std::string_view username,
    const std::vector<std::string>& password_history) const;
```

**Preconditions:**
- Policy is configured
- Password is provided

**Postconditions:**
- Returns result with validation status and any errors

**Algorithm:**
```
Input: password, username, password_history
Output: ValidationResult

1. INITIALIZE result as success

2. CHECK minimum length
   if password.length < policy.min_length:
       result.addError(TOO_SHORT)

3. CHECK complexity (if enforced)
   Count: uppercase, lowercase, digits, special
   
   if uppercase < policy.min_uppercase:
       result.addError(MISSING_UPPERCASE)
   if lowercase < policy.min_lowercase:
       result.addError(MISSING_LOWERCASE)
   if digits < policy.min_digits:
       result.addError(MISSING_DIGIT)
   if special < policy.min_special:
       result.addError(MISSING_SPECIAL)

4. CHECK username containment
   if lowercase(password) contains lowercase(username):
       result.addError(CONTAINS_USERNAME)

5. CHECK password reuse (if enabled)
   hash = hashPassword(password)
   if hash in password_history[:history_count]:
       result.addError(REUSED_PASSWORD)

6. CHECK common passwords
   if CommonPasswordChecker.isCommon(password):
       result.addError(COMMON_PASSWORD)

7. RETURN result
```

#### Function: `checkLogin()`

```cpp
// Login check flow
LoginCheckResult checkLogin(const PasswordState& state, bool credentials_valid) const;
```

**State Machine:**

```
                    ┌─────────────────┐
                    │  CREDENTIALS    │
                    │    CHECK        │
                    └────────┬────────┘
                             │
           Invalid           │ Valid
              ┌──────────────┴──────────────┐
              ▼                             ▼
┌─────────────────────────┐    ┌─────────────────────────┐
│ INVALID_CREDENTIALS     │    │   Check account state   │
│ increment failed count  │    │                         │
└─────────────────────────┘    └───────────┬─────────────┘
                                           │
                  ┌────────────────────────┼────────────────────────┐
                  │                        │                        │
                  ▼                        ▼                        ▼
         ┌──────────────┐        ┌──────────────┐        ┌──────────────┐
         │   LOCKED     │        │ FORCE_CHANGE │        │   NORMAL     │
         │              │        │              │        │              │
         │ Return:      │        │ Return:      │        │ Check expiry │
         │ ACCOUNT_LOCK │        │ PASSWORD_    │        │              │
         │              │        │ CHANGE_REQ   │        └──────┬───────┘
         └──────────────┘        └──────────────┘               │
                                                                 │
                              ┌──────────────────────────────────┼──────────────────┐
                              │                                  │                  │
                              ▼                                  ▼                  ▼
                     ┌──────────────┐              ┌─────────────────┐    ┌────────────────┐
                     │   EXPIRED    │              │  IN_GRACE_PERIOD│    │    VALID       │
                     │              │              │                 │    │                │
                     │ Check grace  │              │ Return:         │    │ Reset failed   │
                     │              │              │ PASSWORD_EXP_   │    │ Return:        │
                     └──────┬───────┘              │ GRACE           │    │ SUCCESS        │
                            │                      └─────────────────┘    └────────────────┘
              ┌─────────────┴─────────────┐
              │                           │
              ▼                           ▼
    ┌─────────────────┐      ┌──────────────────────┐
    │ IN_GRACE_PERIOD │      │  PAST_GRACE_PERIOD   │
    │                 │      │                      │
    │ Return:         │      │ Return:              │
    │ PASSWORD_EXPIRED│      │ PASSWORD_EXPIRED_    │
    └─────────────────┘      │ LOCKED               │
                             └──────────────────────┘
```

### Password State Lifecycle

```
New Password Set
│
├─ Create PasswordState
│   ├─ created_at = now()
│   ├─ expires_at = now() + expiration_days
│   └─ failed_attempts = 0
│
├─ Usage Period
│   ├─ Successful logins reset failed_attempts
│   └─ Failed logins increment failed_attempts
│
├─ Warning Period (warning_days before expiry)
│   └─ Show expiration warning to user
│
├─ Expiration
│   │
│   ├─ Grace Period (grace_period_days)
│   │   ├─ Allow login
│   │   ├─ Require password change
│   │   └─ Show urgent warning
│   │
│   └─ Lockout (after grace period)
│       └─ Account locked
│
└─ Password Change
    ├─ Validate new password against policy
    ├─ Update PasswordState
    ├─ Add old hash to history
    └─ Reset expiration
```

### Common Password Checking

```cpp
// From password_policy.h:277-297

class CommonPasswordChecker {
public:
    static CommonPasswordChecker& getInstance();
    bool isCommonPassword(std::string_view password) const;
    core::Status loadFromFile(std::view filename, core::ErrorContext* ctx = nullptr);
    void addCommonPassword(std::string_view password);
    size_t count() const;

private:
    CommonPasswordChecker();
    std::vector<std::string> common_passwords_;
};
```

**Default Common Passwords:**
- Top 1000 most common passwords
- Keyboard patterns (qwerty, 123456, etc.)
- Common substitutions (p@ssw0rd, etc.)

### Built-in Policies

| Policy | Expiration | History | Complexity | Length |
|--------|------------|---------|------------|--------|
| Default | 90 days | 5 | No | 8 |
| Strict | 60 days | 10 | Yes | 12 |
| NIST (Modern) | Never | 0 | No | 15 |
| PCI-DSS | 90 days | 4 | Yes | 12 |

**Default Policy:**
```cpp
PasswordPolicy PasswordPolicy::defaultPolicy() {
    return PasswordPolicy{};  // Uses defaults above
}
```

**Strict Policy:**
```cpp
PasswordPolicy PasswordPolicy::strictPolicy() {
    PasswordPolicy p;
    p.expiration_days = 60;
    p.history_count = 10;
    p.enforce_complexity = true;
    p.min_length = 12;
    p.min_uppercase = 1;
    p.min_lowercase = 1;
    p.min_digits = 1;
    p.min_special = 1;
    p.max_failed_attempts = 3;
    return p;
}
```

### Password Hashing

ScratchBird uses industry-standard password hashing:

```
Password Hashing:
┌─────────────────────────────────────────────────────────────┐
│ PBKDF2-SHA256 (for password storage)                        │
│ ├─ Salt: 16 bytes random                                    │
│ ├─ Iterations: 100,000+                                     │
│ └─ Output: 32 bytes                                         │
├─────────────────────────────────────────────────────────────┤
│ SCRAM-SHA-256 (for authentication)                          │
│ ├─ Salt: 16 bytes                                           │
│ ├─ Iterations: 4096+ (configurable)                         │
│ └─ Stored: SaltedKey, ServerKey, iterations                 │
└─────────────────────────────────────────────────────────────┘
```

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `TOO_SHORT` | Password < min_length | Use longer password |
| `MISSING_UPPERCASE` | No uppercase letters | Add uppercase |
| `MISSING_LOWERCASE` | No lowercase letters | Add lowercase |
| `MISSING_DIGIT` | No numeric characters | Add digits |
| `MISSING_SPECIAL` | No special characters | Add special chars |
| `REUSED_PASSWORD` | In recent history | Choose different password |
| `COMMON_PASSWORD` | In common list | Choose unique password |
| `CONTAINS_USERNAME` | Contains username | Avoid username in password |
| `ACCOUNT_LOCKED` | Too many failed attempts | Wait for lockout period |
| `PASSWORD_EXPIRED` | Past expiration | Change password |

## Invariants

1. **History Size Limit**: Password history never exceeds policy.history_count
   - Verification: Trim on password change

2. **Lockout Duration**: Account locked until locked_until time
   - Verification: Check isLocked() before authentication

3. **Grace Period Extends Expiration**: Grace period adds to expiration
   - Verification: isInGracePeriod() uses expires_at + grace_days

4. **Force Change Priority**: FORCE_CHANGE takes precedence over expiration
   - Verification: Check order in checkLogin()

## SQL Interface

```sql
-- Set password policy (superuser only)
ALTER SYSTEM SET password_policy = 'strict';

-- Configure specific settings
ALTER SYSTEM SET password_min_length = 12;
ALTER SYSTEM SET password_expiration_days = 60;
ALTER SYSTEM SET password_history_count = 10;

-- Force password change for user
ALTER USER username PASSWORD EXPIRE;

-- Unlock locked account
ALTER USER username ACCOUNT UNLOCK;

-- Set never-expire password (service accounts)
ALTER USER username VALID UNTIL 'infinity';
```

## Related Specifications

- `authentication_flow.md` - Authentication with password
- `auth_plugins.md` - SCRAM authentication
- `ssl_tls.md` - Secure password transmission

## Appendix

### Password Security Best Practices

1. **Length over Complexity**: Longer passwords are stronger than complex short ones
2. **No Forced Rotation**: NIST recommends against forced rotation without reason
3. **Check Breaches**: Verify passwords against known breach databases
4. **Rate Limiting**: Prevent brute force with account lockout
5. **Secure Transport**: Always use SSL/TLS for password transmission

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
