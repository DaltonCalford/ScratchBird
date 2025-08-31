# User Defined Routines (UDR) Specification

## Overview

ScratchBird implements a comprehensive UDR system that allows external libraries to extend the database with custom functions, procedures, triggers, and even new data types. This is inspired by Firebird's UDR but enhanced with modern features.

## UDR Architecture

### Core Components

```cpp
// UDR Manager - Central registry for all external routines
class UDRManager {
    struct UDRModule {
        string name;
        string library_path;
        void* handle;  // dlopen/LoadLibrary handle
        UDRInterface* interface;
        map<string, UDRRoutine*> routines;
        SecurityContext security;
    };
    
    map<string, UDRModule> loaded_modules;
    
    void load_module(const string& path);
    void unload_module(const string& name);
    UDRRoutine* get_routine(const string& module, const string& routine);
};
```

## UDR Types

### 1. External Functions (UDF)

```sql
-- Declare external function
CREATE FUNCTION calculate_hash(input VARCHAR(1000))
RETURNS VARCHAR(64)
EXTERNAL NAME 'crypto_lib!SHA256.calculate'
ENGINE UDR;

-- Use in SQL
SELECT calculate_hash(password) FROM users;
```

### 2. External Procedures

```sql
-- External stored procedure
CREATE PROCEDURE send_email(
    to_address VARCHAR(255),
    subject VARCHAR(255),
    body TEXT
)
EXTERNAL NAME 'email_lib!EmailSender.send'
ENGINE UDR;

-- Call procedure
EXECUTE PROCEDURE send_email('user@example.com', 'Subject', 'Body');
```

### 3. External Triggers

```sql
-- External trigger
CREATE TRIGGER audit_trigger
AFTER INSERT OR UPDATE OR DELETE ON sensitive_table
EXTERNAL NAME 'audit_lib!AuditLogger.log_change'
ENGINE UDR;
```

### 4. External Aggregate Functions

```sql
-- Custom aggregate function
CREATE AGGREGATE FUNCTION geometric_mean(value DOUBLE PRECISION)
RETURNS DOUBLE PRECISION
EXTERNAL NAME 'math_lib!Statistics.geometric_mean'
ENGINE UDR;

-- Use in queries
SELECT geometric_mean(price) FROM products GROUP BY category;
```

### 5. External Table Functions

```sql
-- Table-valued function
CREATE FUNCTION read_csv(filename VARCHAR(500))
RETURNS TABLE (
    line_no INTEGER,
    col1 VARCHAR(100),
    col2 VARCHAR(100),
    col3 VARCHAR(100)
)
EXTERNAL NAME 'file_lib!CSVReader.read_file'
ENGINE UDR;

-- Use as table source
SELECT * FROM TABLE(read_csv('/data/import.csv')) WHERE line_no > 1;
```

## UDR Interface Definition

### C++ Interface

```cpp
// Base UDR interface that libraries must implement
class IUDRModule {
public:
    virtual ~IUDRModule() = default;
    
    // Module initialization
    virtual Status initialize(IUDRContext* context) = 0;
    virtual Status shutdown() = 0;
    
    // Routine discovery
    virtual vector<UDRRoutineInfo> get_routines() = 0;
    
    // Factory methods
    virtual IUDRFunction* create_function(const string& name) = 0;
    virtual IUDRProcedure* create_procedure(const string& name) = 0;
    virtual IUDRTrigger* create_trigger(const string& name) = 0;
};

// Function interface
class IUDRFunction {
public:
    virtual ~IUDRFunction() = default;
    
    // Metadata
    virtual UDRMetadata get_metadata() = 0;
    
    // Execution
    virtual Status execute(
        IUDRContext* context,
        void* input_buffer,
        void* output_buffer
    ) = 0;
    
    // Cleanup
    virtual void dispose() = 0;
};

// Context for UDR execution
class IUDRContext {
public:
    // Database access
    virtual ITransaction* get_transaction() = 0;
    virtual IStatement* prepare_statement(const string& sql) = 0;
    
    // Memory management
    virtual void* allocate(size_t size) = 0;
    virtual void deallocate(void* ptr) = 0;
    
    // Error handling
    virtual void set_error(const string& message) = 0;
    
    // Security
    virtual string get_current_user() = 0;
    virtual bool has_privilege(const string& privilege) = 0;
};
```

### C Interface (for compatibility)

```c
// C-compatible interface for broader language support
typedef struct UDR_Context UDR_Context;
typedef struct UDR_Value UDR_Value;

// Function signature for C UDFs
typedef int (*UDR_Function)(
    UDR_Context* context,
    UDR_Value* inputs,
    int input_count,
    UDR_Value* output
);

// Registration structure
typedef struct {
    const char* name;
    UDR_Function function;
    const char* input_types;
    const char* output_type;
    int deterministic;
} UDR_FunctionInfo;

// Export function that modules must provide
extern "C" {
    int UDR_Init(UDR_Context* context);
    int UDR_GetFunctions(UDR_FunctionInfo** functions, int* count);
    void UDR_Shutdown();
}
```

## Language Support

### Native Languages

```sql
-- C++ UDR
CREATE FUNCTION cpp_function(x INTEGER, y INTEGER)
RETURNS INTEGER
EXTERNAL NAME 'math_lib!MathFunctions.add'
LANGUAGE CPP
ENGINE UDR;

-- C UDR
CREATE FUNCTION c_function(text VARCHAR(100))
RETURNS VARCHAR(100)
EXTERNAL NAME 'text_lib!reverse_string'
LANGUAGE C
ENGINE UDR;

-- Rust UDR
CREATE FUNCTION rust_function(data BLOB)
RETURNS BLOB
EXTERNAL NAME 'crypto_lib!encrypt_data'
LANGUAGE RUST
ENGINE UDR;
```

### Managed Languages

```sql
-- Python UDR (via embedded Python)
CREATE FUNCTION python_function(json_text TEXT)
RETURNS TEXT
EXTERNAL NAME 'python_lib!json_processor.process'
LANGUAGE PYTHON
ENGINE UDR;

-- Java UDR (via JNI)
CREATE FUNCTION java_function(xml TEXT)
RETURNS TABLE (key VARCHAR(100), value VARCHAR(100))
EXTERNAL NAME 'com.example.XMLParser!parse'
LANGUAGE JAVA
ENGINE UDR;

-- .NET UDR (via CoreCLR hosting)
CREATE FUNCTION dotnet_function(input INTEGER)
RETURNS INTEGER
EXTERNAL NAME 'MyLibrary.dll!MyNamespace.MyClass.MyMethod'
LANGUAGE CSHARP
ENGINE UDR;
```

## Security Model

### Execution Contexts

```sql
-- Define security context for UDR
CREATE FUNCTION secure_function(sensitive_data TEXT)
RETURNS TEXT
EXTERNAL NAME 'secure_lib!process'
ENGINE UDR
SECURITY DEFINER  -- Run with definer's privileges
SANDBOXED;        -- Run in restricted environment

-- Security options
SECURITY INVOKER   -- Run with caller's privileges (default)
SECURITY DEFINER   -- Run with owner's privileges
SANDBOXED          -- Restricted file/network access
TRUSTED            -- Full system access (admin only)
```

### Permission Management

```sql
-- Grant permission to create UDRs
GRANT CREATE ROUTINE TO developer_role;

-- Grant permission to use specific UDR library
GRANT USAGE ON UDR LIBRARY 'crypto_lib' TO app_role;

-- Revoke permissions
REVOKE EXECUTE ON FUNCTION calculate_hash FROM PUBLIC;
```

### Sandboxing

```cpp
class UDRSandbox {
    // Resource limits
    struct ResourceLimits {
        size_t max_memory = 100 * 1024 * 1024;  // 100MB
        size_t max_cpu_time = 5000;             // 5 seconds
        size_t max_file_handles = 10;
        bool allow_network = false;
        bool allow_file_system = false;
        vector<string> allowed_paths;
    };
    
    void execute_sandboxed(IUDRFunction* function, ResourceLimits limits) {
        // Set up restricted environment
        setup_memory_limits(limits.max_memory);
        setup_cpu_limits(limits.max_cpu_time);
        restrict_file_access(limits.allowed_paths);
        
        if (!limits.allow_network) {
            block_network_access();
        }
        
        // Execute in sandbox
        function->execute(/*...*/);
    }
};
```

## Advanced Features

### Hot Reload

```sql
-- Reload UDR library without restart
ALTER UDR LIBRARY 'math_lib' RELOAD;

-- Version management
CREATE FUNCTION versioned_func(x INTEGER)
RETURNS INTEGER
EXTERNAL NAME 'lib_v2!function'
ENGINE UDR
VERSION '2.0.0';
```

### Debugging Support

```sql
-- Enable debugging for UDR
SET SESSION udr_debug = true;
SET SESSION udr_trace_calls = true;

-- Debug information in system tables
SELECT * FROM system.udr_calls 
WHERE function_name = 'calculate_hash'
ORDER BY call_time DESC;
```

### Performance Monitoring

```sql
-- Monitor UDR performance
SELECT 
    function_name,
    call_count,
    total_time_ms,
    avg_time_ms,
    max_time_ms
FROM system.udr_statistics
WHERE total_time_ms > 1000
ORDER BY total_time_ms DESC;
```

## Example: Complete UDR Library

### C++ Implementation

```cpp
// crypto_lib.cpp
#include "scratchbird_udr.h"
#include <openssl/sha.h>

class CryptoLib : public IUDRModule {
public:
    Status initialize(IUDRContext* context) override {
        // Initialize OpenSSL
        return Status::OK;
    }
    
    vector<UDRRoutineInfo> get_routines() override {
        return {
            {"SHA256.calculate", UDRType::FUNCTION},
            {"AES.encrypt", UDRType::FUNCTION},
            {"AES.decrypt", UDRType::FUNCTION}
        };
    }
    
    IUDRFunction* create_function(const string& name) override {
        if (name == "SHA256.calculate") {
            return new SHA256Function();
        }
        // ... other functions
        return nullptr;
    }
};

class SHA256Function : public IUDRFunction {
public:
    UDRMetadata get_metadata() override {
        return {
            .inputs = {{"input", DataType::VARCHAR}},
            .output = {"hash", DataType::VARCHAR},
            .deterministic = true,
            .null_on_null = true
        };
    }
    
    Status execute(IUDRContext* context, void* input, void* output) override {
        auto* input_str = static_cast<UDRString*>(input);
        auto* output_str = static_cast<UDRString*>(output);
        
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, input_str->data, input_str->length);
        SHA256_Final(hash, &sha256);
        
        // Convert to hex string
        char hex[SHA256_DIGEST_LENGTH * 2 + 1];
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            sprintf(hex + (i * 2), "%02x", hash[i]);
        }
        
        output_str->data = context->allocate(strlen(hex) + 1);
        strcpy(output_str->data, hex);
        output_str->length = strlen(hex);
        
        return Status::OK;
    }
};

// Export function
extern "C" IUDRModule* CreateUDRModule() {
    return new CryptoLib();
}
```

### SQL Registration

```sql
-- Register the library
CREATE UDR LIBRARY crypto_lib
FILE '/usr/lib/scratchbird/udr/crypto_lib.so'
LANGUAGE CPP;

-- Create functions from library
CREATE FUNCTION sha256(input VARCHAR(8000))
RETURNS VARCHAR(64)
EXTERNAL NAME 'crypto_lib!SHA256.calculate'
ENGINE UDR;

-- Use the function
SELECT id, sha256(password) as password_hash
FROM users;
```

## Configuration

```sql
-- System-wide UDR settings
ALTER SYSTEM SET udr_enabled = true;
ALTER SYSTEM SET udr_library_path = '/usr/lib/scratchbird/udr';
ALTER SYSTEM SET udr_max_memory = '1GB';
ALTER SYSTEM SET udr_timeout = 30;  -- seconds

-- Per-session settings
SET udr_debug = true;
SET udr_trace_level = 'verbose';
```

## Benefits

1. **Extensibility**: Add any functionality without modifying core
2. **Performance**: Native code execution
3. **Language Choice**: Use best language for the task
4. **Reusability**: Share libraries across databases
5. **Security**: Sandboxing and permission control
6. **Hot Reload**: Update without downtime
7. **Integration**: Connect to external systems