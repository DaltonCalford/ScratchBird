# ScratchBird UDR (User Defined Routines) Documentation

**Database Engine**: ScratchBird Alpha 0.6.0  
**Document Version**: 1.0  
**Date**: July 2025  
**SQL Dialect**: 4 (Enhanced) - Default  

## Table of Contents

1. [UDR Overview](#udr-overview)
2. [UDR Architecture](#udr-architecture)
3. [External Functions](#external-functions)
4. [External Procedures](#external-procedures)
5. [Language Bindings](#language-bindings)
6. [UDR Development](#udr-development)
7. [UDR Security](#udr-security)
8. [Performance Considerations](#performance-considerations)
9. [Deployment and Management](#deployment-and-management)
10. [Error Handling](#error-handling)
11. [Advanced Features](#advanced-features)
12. [Best Practices](#best-practices)
13. [Examples and Tutorials](#examples-and-tutorials)
14. [Troubleshooting](#troubleshooting)

---

## UDR Overview

### What are UDRs?
User Defined Routines (UDRs) are external functions and procedures written in languages other than PSQL that can be called from within ScratchBird database operations. UDRs extend the database engine's capabilities by allowing custom logic implementation in high-performance compiled languages.

### Key Features
- **Multi-language Support**: C++, C, Python, Java, Pascal, and more
- **High Performance**: Compiled code execution with minimal overhead
- **Type Safety**: Strong type checking and parameter validation
- **Security Sandboxing**: Controlled execution environment
- **Memory Management**: Automatic resource cleanup and leak prevention
- **Exception Handling**: Comprehensive error propagation and handling

### UDR vs PSQL Comparison

| Feature | UDR | PSQL |
|---------|-----|------|
| **Performance** | Very High (compiled) | Moderate (interpreted) |
| **Language** | C++, Python, Java, etc. | SQL-like procedural |
| **Complexity** | High (requires compilation) | Low (SQL-based) |
| **Portability** | Platform-dependent | Platform-independent |
| **Debugging** | External debuggers | Built-in SQL debugging |
| **Resource Access** | System APIs available | Database-only access |
| **Memory Usage** | Developer-controlled | Engine-managed |

---

## UDR Architecture

### ScratchBird UDR Engine
The ScratchBird UDR engine provides a plugin-based architecture for external routine execution:

```
┌─────────────────────────────────────────┐
│           ScratchBird Engine            │
├─────────────────────────────────────────┤
│              UDR Interface              │
├─────────────────────────────────────────┤
│    Language Bridge    │   Type System   │
├─────────────────────────────────────────┤
│  Memory Manager  │  Security Sandbox   │
├─────────────────────────────────────────┤
│           External Libraries            │
│  (libpython, libjvm, native .so/.dll)  │
└─────────────────────────────────────────┘
```

### Core Components

#### UDR Manager
- **Location**: `src/jrd/UdrManager.h/.cpp`
- **Purpose**: Manages UDR lifecycle, loading, and execution
- **Responsibilities**: Library loading, function resolution, parameter marshalling

#### Type Bridge
- **Location**: `src/common/classes/UdrCppEngine.h`
- **Purpose**: Type conversion between SQL types and native language types
- **Features**: Automatic marshalling, NULL handling, array support

#### Security Engine
- **Location**: `src/jrd/UdrSecurity.h/.cpp`
- **Purpose**: Sandboxed execution environment
- **Controls**: File system access, network operations, memory limits

#### Memory Manager
- **Location**: `src/common/classes/UdrMemoryManager.h`
- **Purpose**: Automatic memory management for UDR execution
- **Features**: Leak detection, automatic cleanup, pool allocation

---

## External Functions

### Function Declaration Syntax

#### Basic External Function
```sql
CREATE OR ALTER FUNCTION function_name (
    parameter1 data_type [NOT NULL],
    parameter2 data_type [NOT NULL],
    ...
) RETURNS return_type [NOT NULL]
EXTERNAL NAME 'library_name!function_entry_point'
ENGINE engine_name;
```

#### ScratchBird-Specific Syntax
```sql
-- C++ function example
CREATE OR ALTER FUNCTION calculate_tax (
    income DECIMAL(10,2) NOT NULL,
    tax_rate DOUBLE PRECISION NOT NULL
) RETURNS DECIMAL(10,2) NOT NULL
EXTERNAL NAME 'libtaxcalc.so!calculate_tax'
ENGINE UDR_CPP;

-- Python function example  
CREATE OR ALTER FUNCTION analyze_sentiment (
    text_input VARCHAR(8000) NOT NULL
) RETURNS VARCHAR(50) NOT NULL
EXTERNAL NAME 'sentiment_analyzer.py!analyze_sentiment'
ENGINE UDR_PYTHON;

-- Java function example
CREATE OR ALTER FUNCTION hash_password (
    password VARCHAR(255) NOT NULL,
    salt VARCHAR(50) NOT NULL
) RETURNS VARCHAR(255) NOT NULL
EXTERNAL NAME 'com.company.security.PasswordUtils!hashPassword'
ENGINE UDR_JAVA;
```

### Advanced Function Features

#### Array Input/Output
```sql
-- Function accepting and returning arrays
CREATE OR ALTER FUNCTION sort_numbers (
    input_array INTEGER[] NOT NULL
) RETURNS INTEGER[] NOT NULL
EXTERNAL NAME 'libarray_utils.so!sort_integer_array'
ENGINE UDR_CPP;

-- Usage
SELECT sort_numbers(ARRAY[5, 2, 8, 1, 9]);
-- Returns: {1,2,5,8,9}
```

#### Blob Handling
```sql
-- Image processing function
CREATE OR ALTER FUNCTION resize_image (
    image_data BLOB NOT NULL,
    new_width INTEGER NOT NULL,
    new_height INTEGER NOT NULL
) RETURNS BLOB NOT NULL
EXTERNAL NAME 'libimageproc.so!resize_image'
ENGINE UDR_CPP;
```

#### JSON Processing
```sql
-- JSON validation and transformation
CREATE OR ALTER FUNCTION validate_json (
    json_text VARCHAR(32000) NOT NULL
) RETURNS BOOLEAN NOT NULL
EXTERNAL NAME 'libjsonutils.so!validate_json_string'
ENGINE UDR_CPP;

CREATE OR ALTER FUNCTION transform_json (
    input_json VARCHAR(32000) NOT NULL,
    transformation_rules VARCHAR(8000) NOT NULL
) RETURNS VARCHAR(32000) NOT NULL
EXTERNAL NAME 'json_transformer.py!transform_json'
ENGINE UDR_PYTHON;
```

### Function Examples by Language

#### C++ Function Implementation
```cpp
// File: libtaxcalc.cpp
#include "UdrCppEngine.h"
#include <decimal>

extern "C" FB_UDR_MESSAGE(CalculateTaxInput,
    (FB_DECIMAL(10,2), income)
    (FB_DOUBLE, tax_rate)
);

extern "C" FB_UDR_MESSAGE(CalculateTaxOutput,
    (FB_DECIMAL(10,2), result)
);

FB_UDR_IMPLEMENT_FUNCTION(calculate_tax, CalculateTaxInput, CalculateTaxOutput)
{
    decimal income = in->income.get();
    double rate = in->tax_rate.get();
    
    decimal tax = income * (rate / 100.0);
    out->result.set(tax);
}
```

#### Python Function Implementation
```python
# File: sentiment_analyzer.py
import scratchbird_udr as sb
from textblob import TextBlob

@sb.udr_function
def analyze_sentiment(text_input: str) -> str:
    """Analyze sentiment of input text"""
    try:
        blob = TextBlob(text_input)
        sentiment = blob.sentiment.polarity
        
        if sentiment > 0.1:
            return "POSITIVE"
        elif sentiment < -0.1:
            return "NEGATIVE"
        else:
            return "NEUTRAL"
    except Exception as e:
        sb.raise_error(f"Sentiment analysis failed: {str(e)}")
```

#### Java Function Implementation
```java
// File: PasswordUtils.java
package com.company.security;

import com.scratchbird.udr.*;
import java.security.MessageDigest;
import java.security.SecureRandom;

public class PasswordUtils {
    
    @UDRFunction
    public static String hashPassword(String password, String salt) 
        throws UDRException {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            md.update(salt.getBytes());
            byte[] hash = md.digest(password.getBytes());
            
            StringBuilder hexString = new StringBuilder();
            for (byte b : hash) {
                String hex = Integer.toHexString(0xff & b);
                if (hex.length() == 1) {
                    hexString.append('0');
                }
                hexString.append(hex);
            }
            return hexString.toString();
        } catch (Exception e) {
            throw new UDRException("Hash generation failed: " + e.getMessage());
        }
    }
}
```

---

## External Procedures

### Procedure Declaration Syntax

#### Basic External Procedure
```sql
CREATE OR ALTER PROCEDURE procedure_name (
    input_param1 data_type [NOT NULL],
    input_param2 data_type [NOT NULL]
) RETURNS (
    output_param1 data_type [NOT NULL],
    output_param2 data_type [NOT NULL]
)
EXTERNAL NAME 'library_name!procedure_entry_point'
ENGINE engine_name;
```

#### Advanced Procedure Examples
```sql
-- Data processing procedure
CREATE OR ALTER PROCEDURE process_sales_data (
    start_date DATE NOT NULL,
    end_date DATE NOT NULL,
    region VARCHAR(50) NOT NULL
) RETURNS (
    total_sales DECIMAL(15,2) NOT NULL,
    transaction_count INTEGER NOT NULL,
    avg_transaction DECIMAL(10,2) NOT NULL,
    top_product VARCHAR(100)
)
EXTERNAL NAME 'libsalesprocessor.so!process_sales_data'
ENGINE UDR_CPP;

-- Machine learning prediction procedure
CREATE OR ALTER PROCEDURE predict_customer_churn (
    customer_id INTEGER NOT NULL,
    feature_vector DOUBLE PRECISION[] NOT NULL
) RETURNS (
    churn_probability DOUBLE PRECISION NOT NULL,
    risk_category VARCHAR(20) NOT NULL,
    recommended_action VARCHAR(500)
)
EXTERNAL NAME 'ml_predictor.py!predict_customer_churn'
ENGINE UDR_PYTHON;
```

### Selectable Procedures
```sql
-- Procedure that returns multiple rows
CREATE OR ALTER PROCEDURE generate_fibonacci (
    max_value INTEGER NOT NULL
) RETURNS (
    position INTEGER NOT NULL,
    fibonacci_value BIGINT NOT NULL
)
EXTERNAL NAME 'libmath_utils.so!generate_fibonacci'
ENGINE UDR_CPP;

-- Usage as selectable procedure
SELECT position, fibonacci_value
FROM generate_fibonacci(100)
ORDER BY position;
```

### Procedure Implementation Examples

#### C++ Selectable Procedure
```cpp
// File: libmath_utils.cpp
#include "UdrCppEngine.h"

FB_UDR_IMPLEMENT_PROCEDURE(generate_fibonacci, 
    (FB_INTEGER, max_value),
    (FB_INTEGER, position)(FB_BIGINT, fibonacci_value))
{
    int max_val = in->max_value.get();
    long long a = 0, b = 1;
    int pos = 1;
    
    // First Fibonacci number
    out->position.set(pos);
    out->fibonacci_value.set(a);
    status->suspend();
    
    pos++;
    out->position.set(pos);
    out->fibonacci_value.set(b);
    status->suspend();
    
    // Generate subsequent numbers
    while (true) {
        long long next = a + b;
        if (next > max_val) break;
        
        pos++;
        out->position.set(pos);
        out->fibonacci_value.set(next);
        status->suspend();
        
        a = b;
        b = next;
    }
}
```

#### Python Data Processing Procedure
```python
# File: ml_predictor.py
import scratchbird_udr as sb
import numpy as np
import joblib
from typing import List, Tuple

# Load pre-trained model
churn_model = joblib.load('/models/customer_churn_model.pkl')

@sb.udr_procedure
def predict_customer_churn(customer_id: int, feature_vector: List[float]) -> sb.Result:
    """Predict customer churn probability"""
    try:
        # Convert to numpy array
        features = np.array(feature_vector).reshape(1, -1)
        
        # Make prediction
        churn_prob = churn_model.predict_proba(features)[0][1]
        
        # Determine risk category
        if churn_prob >= 0.8:
            risk_category = "HIGH"
            action = "Immediate retention campaign with personalized offers"
        elif churn_prob >= 0.5:
            risk_category = "MEDIUM"
            action = "Proactive engagement with loyalty program benefits"
        elif churn_prob >= 0.3:
            risk_category = "LOW"
            action = "Regular check-in and satisfaction survey"
        else:
            risk_category = "MINIMAL"
            action = "Standard communication and service quality maintenance"
        
        return sb.Result(
            churn_probability=float(churn_prob),
            risk_category=risk_category,
            recommended_action=action
        )
        
    except Exception as e:
        sb.raise_error(f"Churn prediction failed for customer {customer_id}: {str(e)}")
```

---

## Language Bindings

### C++ Engine (UDR_CPP)

#### Installation and Setup
```bash
# Install development headers
sudo apt-get install libscratchbird-udr-dev g++

# Compile UDR library
g++ -shared -fPIC -o libudr_example.so udr_example.cpp \
    -lscratchbird_udr -std=c++17 -O2
```

#### C++ Type Mappings
| SQL Type | C++ Type | UDR Macro |
|----------|----------|-----------|
| SMALLINT | int16_t | FB_SMALLINT |
| INTEGER | int32_t | FB_INTEGER |
| BIGINT | int64_t | FB_BIGINT |
| FLOAT | float | FB_FLOAT |
| DOUBLE PRECISION | double | FB_DOUBLE |
| DECIMAL(p,s) | FBDecimal<p,s> | FB_DECIMAL(p,s) |
| CHAR(n) | FBChar<n> | FB_CHAR(n) |
| VARCHAR(n) | FBVarChar<n> | FB_VARCHAR(n) |
| BLOB | FBBlob | FB_BLOB |
| BOOLEAN | bool | FB_BOOLEAN |
| DATE | FBDate | FB_DATE |
| TIME | FBTime | FB_TIME |
| TIMESTAMP | FBTimestamp | FB_TIMESTAMP |

#### Advanced C++ Features
```cpp
// Exception handling
try {
    // UDR logic here
} catch (const FBException& e) {
    status->setException(e);
    return;
} catch (const std::exception& e) {
    status->setException(FBException::create(e.what()));
    return;
}

// Memory management
FBAutoPtr<char> buffer = status->allocateMemory<char>(size);
// Automatically freed when scope ends

// Transaction control
if (status->getTransaction()->getIsolationLevel() == iso_read_committed) {
    // Transaction-specific logic
}
```

### Python Engine (UDR_PYTHON)

#### Installation and Setup
```bash
# Install Python UDR support
pip install scratchbird-udr-python

# Configure Python path in scratchbird.conf
UDRPythonPath = /usr/local/lib/python3.9/site-packages
```

#### Python Type Mappings
| SQL Type | Python Type | 
|----------|-------------|
| SMALLINT, INTEGER, BIGINT | int |
| FLOAT, DOUBLE PRECISION | float |
| DECIMAL | decimal.Decimal |
| CHAR, VARCHAR | str |
| BLOB | bytes |
| BOOLEAN | bool |
| DATE | datetime.date |
| TIME | datetime.time |
| TIMESTAMP | datetime.datetime |
| Array types | list |

#### Python UDR Framework
```python
import scratchbird_udr as sb
import logging

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

@sb.udr_function
@sb.exception_handler
def secure_calculation(x: float, y: float) -> float:
    """Example function with comprehensive error handling"""
    
    # Validation
    if y == 0:
        sb.raise_error("Division by zero not allowed")
    
    # Business logic
    result = x / y
    
    # Logging
    logger.info(f"Calculated {x} / {y} = {result}")
    
    return result

@sb.udr_procedure
def data_export(table_name: str, output_format: str) -> sb.ResultSet:
    """Export data in various formats"""
    
    # Security check
    if not sb.has_privilege("SELECT", table_name):
        sb.raise_error("Insufficient privileges")
    
    # Implementation
    try:
        if output_format.upper() == "CSV":
            return export_to_csv(table_name)
        elif output_format.upper() == "JSON":
            return export_to_json(table_name)
        else:
            sb.raise_error(f"Unsupported format: {output_format}")
    except Exception as e:
        sb.raise_error(f"Export failed: {str(e)}")
```

### Java Engine (UDR_JAVA)

#### Installation and Setup
```bash
# Install Java UDR support
sudo apt-get install libscratchbird-udr-java openjdk-11-jdk

# Configure classpath in scratchbird.conf
UDRJavaClassPath = /usr/local/lib/scratchbird/udr-java.jar:/path/to/your/udrs.jar
```

#### Java Type Mappings
| SQL Type | Java Type |
|----------|-----------|
| SMALLINT | short |
| INTEGER | int |
| BIGINT | long |
| FLOAT | float |
| DOUBLE PRECISION | double |
| DECIMAL | java.math.BigDecimal |
| CHAR, VARCHAR | String |
| BLOB | byte[] |
| BOOLEAN | boolean |
| DATE | java.sql.Date |
| TIME | java.sql.Time |
| TIMESTAMP | java.sql.Timestamp |

#### Java UDR Framework
```java
import com.scratchbird.udr.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.logging.Logger;

public class CacheManager {
    private static final Logger logger = Logger.getLogger(CacheManager.class.getName());
    private static final ConcurrentHashMap<String, Object> cache = new ConcurrentHashMap<>();
    
    @UDRFunction
    public static String getCachedValue(String key) throws UDRException {
        try {
            Object value = cache.get(key);
            return value != null ? value.toString() : null;
        } catch (Exception e) {
            throw new UDRException("Cache access failed: " + e.getMessage());
        }
    }
    
    @UDRProcedure
    public static void setCachedValue(String key, String value, int ttlSeconds) 
        throws UDRException {
        try {
            cache.put(key, value);
            
            // Schedule expiration
            ScheduledExecutorService executor = Executors.newSingleThreadScheduledExecutor();
            executor.schedule(() -> cache.remove(key), ttlSeconds, TimeUnit.SECONDS);
            
            logger.info("Cached value for key: " + key);
        } catch (Exception e) {
            throw new UDRException("Cache write failed: " + e.getMessage());
        }
    }
}
```

---

## UDR Development

### Development Workflow

1. **Design Phase**
   - Define function/procedure interface
   - Choose appropriate language
   - Plan error handling strategy

2. **Implementation Phase**
   - Write and test UDR code
   - Handle type conversions
   - Implement security measures

3. **Testing Phase**
   - Unit test UDR logic
   - Integration test with ScratchBird
   - Performance testing

4. **Deployment Phase**
   - Compile/package UDR
   - Deploy to database server
   - Register with database

5. **Maintenance Phase**
   - Monitor performance
   - Handle updates
   - Debug issues

### Development Environment Setup

#### C++ Development
```bash
# Install required tools
sudo apt-get install build-essential cmake libscratchbird-dev

# Create project structure
mkdir myudr_project
cd myudr_project
mkdir src include lib

# CMakeLists.txt example
cmake_minimum_required(VERSION 3.10)
project(MyUDR)

set(CMAKE_CXX_STANDARD 17)
find_package(ScratchBird REQUIRED)

add_library(myudr SHARED src/myudr.cpp)
target_link_libraries(myudr ScratchBird::udr)
```

#### Python Development
```bash
# Create virtual environment
python -m venv udr_dev_env
source udr_dev_env/bin/activate

# Install dependencies
pip install scratchbird-udr-python pytest

# Project structure
mkdir my_python_udr
cd my_python_udr
touch __init__.py functions.py procedures.py test_udrs.py
```

### Testing Framework

#### Unit Testing for C++
```cpp
// test_myudr.cpp
#include <gtest/gtest.h>
#include "myudr.h"

TEST(MyUDRTest, BasicCalculation) {
    // Test UDR logic outside database
    double result = calculate_tax_logic(1000.0, 10.0);
    EXPECT_DOUBLE_EQ(result, 100.0);
}

TEST(MyUDRTest, ErrorHandling) {
    // Test error conditions
    EXPECT_THROW(calculate_tax_logic(-1000.0, 10.0), std::invalid_argument);
}
```

#### Integration Testing
```sql
-- test_integration.sql

-- Test basic function
SELECT calculate_tax(1000.00, 10.0) as tax_amount;
-- Expected: 100.00

-- Test error handling  
SELECT calculate_tax(-1000.00, 10.0) as tax_amount;
-- Expected: Exception

-- Test array handling
SELECT sort_numbers(ARRAY[5, 2, 8, 1, 9]) as sorted;
-- Expected: {1,2,5,8,9}

-- Test procedure
SELECT * FROM process_sales_data('2025-01-01', '2025-01-31', 'NORTH');
-- Expected: Result set with sales data
```

---

## UDR Security

### Security Architecture

#### Sandbox Environment
ScratchBird UDRs execute in a controlled sandbox environment with configurable restrictions:

```ini
# scratchbird.conf security settings
[UDR_Security]
EnableFileSystemAccess = false
EnableNetworkAccess = false
MaxMemoryUsageMB = 256
MaxExecutionTimeMS = 30000
AllowedLibraries = /usr/local/lib/scratchbird/udr/
TrustedPythonModules = numpy,pandas,scikit-learn
```

#### Access Control
```sql
-- Grant UDR execution privileges
GRANT EXECUTE ON FUNCTION calculate_tax TO ROLE ACCOUNTING_USERS;

-- Grant UDR administration privileges
GRANT ALTER ANY FUNCTION TO ROLE UDR_ADMINISTRATORS;

-- Check UDR privileges
SELECT RDB$USER, RDB$PRIVILEGE, RDB$RELATION_NAME
FROM RDB$USER_PRIVILEGES
WHERE RDB$RELATION_NAME = 'CALCULATE_TAX';
```

### Security Best Practices

#### Input Validation
```cpp
// C++ input validation
FB_UDR_IMPLEMENT_FUNCTION(secure_calculation, Input, Output) {
    // Validate inputs
    if (in->amount.isNull()) {
        status->setException(FBException::create("Amount cannot be NULL"));
        return;
    }
    
    double amount = in->amount.get();
    if (amount < 0 || amount > 1000000) {
        status->setException(FBException::create("Amount out of valid range"));
        return;
    }
    
    // Safe calculation
    double result = performCalculation(amount);
    out->result.set(result);
}
```

#### Resource Limits
```python
# Python resource management
import scratchbird_udr as sb
import resource
import signal

@sb.udr_function
@sb.resource_limited(memory_mb=100, time_seconds=10)
def process_large_dataset(data: List[float]) -> float:
    """Function with resource limits"""
    
    # Set memory limit
    resource.setrlimit(resource.RLIMIT_AS, (100 * 1024 * 1024, -1))
    
    # Set time limit
    signal.alarm(10)
    
    try:
        # Process data
        result = sum(data) / len(data)
        return result
    finally:
        signal.alarm(0)  # Cancel alarm
```

#### Secure File Access
```java
// Java secure file operations
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.AccessControlException;

@UDRFunction
public static String readConfigFile(String configName) throws UDRException {
    // Validate file path
    String basePath = "/etc/scratchbird/configs/";
    String fullPath = basePath + configName;
    
    // Security check - prevent path traversal
    if (!fullPath.startsWith(basePath)) {
        throw new UDRException("Invalid file path");
    }
    
    try {
        // Check permissions
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            sm.checkRead(fullPath);
        }
        
        return new String(Files.readAllBytes(Paths.get(fullPath)));
    } catch (AccessControlException e) {
        throw new UDRException("File access denied: " + e.getMessage());
    } catch (Exception e) {
        throw new UDRException("File read error: " + e.getMessage());
    }
}
```

---

## Performance Considerations

### Optimization Strategies

#### Memory Management
```cpp
// Efficient memory usage in C++
FB_UDR_IMPLEMENT_FUNCTION(efficient_string_processing, Input, Output) {
    // Use stack allocation for small objects
    char buffer[256];
    
    // Use smart pointers for dynamic allocation
    auto largeBuffer = std::make_unique<char[]>(in->size.get());
    
    // Reuse objects when possible
    static thread_local std::string workspace;
    workspace.clear();
    workspace.reserve(in->maxSize.get());
    
    // Process data efficiently
    processString(workspace, in->inputText.get());
    out->result.set(workspace.c_str());
}
```

#### Caching Strategies
```python
# Python caching with LRU
from functools import lru_cache
import scratchbird_udr as sb

@lru_cache(maxsize=1000)
def expensive_calculation(input_value: float) -> float:
    """Cached expensive calculation"""
    # Simulate expensive computation
    import time
    time.sleep(0.1)
    return input_value * 2.718281828

@sb.udr_function
def cached_math_function(x: float) -> float:
    """Function using cached calculations"""
    return expensive_calculation(x)
```

#### Batch Processing
```java
// Java batch processing
@UDRProcedure
public static ResultSet processBatchData(String tableName, int batchSize) 
    throws UDRException {
    
    List<ResultRow> results = new ArrayList<>();
    
    try (Connection conn = getConnection();
         PreparedStatement stmt = conn.prepareStatement(
             "SELECT * FROM " + tableName + " LIMIT ?")) {
        
        stmt.setInt(1, batchSize);
        
        try (ResultSet rs = stmt.executeQuery()) {
            while (rs.next()) {
                // Process in batches for better performance
                ResultRow row = processRecord(rs);
                results.add(row);
                
                // Yield periodically for better concurrency
                if (results.size() % 100 == 0) {
                    Thread.yield();
                }
            }
        }
    }
    
    return new UDRResultSet(results);
}
```

### Performance Monitoring

#### Built-in Monitoring
```sql
-- Monitor UDR performance
SELECT f.RDB$FUNCTION_NAME,
       m.MON$RECORD_SEQ_READS,
       m.MON$MEMORY_USED,
       m.MON$EXECUTION_TIME_MS
FROM RDB$FUNCTIONS f
JOIN MON$FUNCTION_STATS m ON f.RDB$FUNCTION_NAME = m.MON$FUNCTION_NAME
WHERE f.RDB$ENGINE_NAME LIKE 'UDR_%'
ORDER BY m.MON$EXECUTION_TIME_MS DESC;
```

#### Custom Performance Tracking
```cpp
// C++ performance tracking
#include <chrono>

FB_UDR_IMPLEMENT_FUNCTION(tracked_function, Input, Output) {
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        // Function implementation
        performBusinessLogic(in, out);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // Log performance metrics
        logPerformanceMetric("tracked_function", duration.count());
        
    } catch (...) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        logErrorMetric("tracked_function", duration.count());
        throw;
    }
}
```

---

## Deployment and Management

### Deployment Process

#### Library Deployment
```bash
# 1. Compile UDR library
make -j$(nproc) release

# 2. Copy to UDR directory
sudo cp libmyudr.so /usr/local/lib/scratchbird/udr/

# 3. Set proper permissions
sudo chmod 755 /usr/local/lib/scratchbird/udr/libmyudr.so
sudo chown root:scratchbird /usr/local/lib/scratchbird/udr/libmyudr.so

# 4. Update library cache
sudo ldconfig

# 5. Register with database
sb_isql -u SYSDBA -p masterkey database.fdb -i register_udrs.sql
```

#### Registration Script
```sql
-- register_udrs.sql

-- Drop existing UDRs (if updating)
DROP FUNCTION IF EXISTS calculate_tax;
DROP PROCEDURE IF EXISTS process_sales_data;

-- Register new UDRs
CREATE OR ALTER FUNCTION calculate_tax (
    income DECIMAL(10,2) NOT NULL,
    tax_rate DOUBLE PRECISION NOT NULL
) RETURNS DECIMAL(10,2) NOT NULL
EXTERNAL NAME 'libmyudr.so!calculate_tax'
ENGINE UDR_CPP;

CREATE OR ALTER PROCEDURE process_sales_data (
    start_date DATE NOT NULL,
    end_date DATE NOT NULL,
    region VARCHAR(50) NOT NULL
) RETURNS (
    total_sales DECIMAL(15,2) NOT NULL,
    transaction_count INTEGER NOT NULL,
    avg_transaction DECIMAL(10,2) NOT NULL
)
EXTERNAL NAME 'libmyudr.so!process_sales_data'
ENGINE UDR_CPP;

-- Grant execution privileges
GRANT EXECUTE ON FUNCTION calculate_tax TO PUBLIC;
GRANT EXECUTE ON PROCEDURE process_sales_data TO ROLE SALES_ANALYSTS;

COMMIT;
```

### Version Management

#### UDR Versioning Strategy
```sql
-- Version 1.0 UDR
CREATE OR ALTER FUNCTION calculate_tax_v1 (
    income DECIMAL(10,2) NOT NULL,
    tax_rate DOUBLE PRECISION NOT NULL
) RETURNS DECIMAL(10,2) NOT NULL
EXTERNAL NAME 'libmyudr_v1.so!calculate_tax'
ENGINE UDR_CPP;

-- Version 2.0 UDR with enhanced features
CREATE OR ALTER FUNCTION calculate_tax_v2 (
    income DECIMAL(10,2) NOT NULL,
    tax_rate DOUBLE PRECISION NOT NULL,
    deductions DECIMAL(10,2) DEFAULT 0,
    tax_year INTEGER DEFAULT 2025
) RETURNS DECIMAL(10,2) NOT NULL
EXTERNAL NAME 'libmyudr_v2.so!calculate_tax'
ENGINE UDR_CPP;

-- Alias for current version
CREATE OR ALTER FUNCTION calculate_tax (
    income DECIMAL(10,2) NOT NULL,
    tax_rate DOUBLE PRECISION NOT NULL
) RETURNS DECIMAL(10,2) NOT NULL
AS
BEGIN
    RETURN calculate_tax_v2(income, tax_rate, 0, 2025);
END;
```

### Monitoring and Maintenance

#### UDR Health Monitoring
```sql
-- Create monitoring view
CREATE OR ALTER VIEW UDR_HEALTH_STATUS AS
SELECT 
    f.RDB$FUNCTION_NAME as UDR_NAME,
    f.RDB$ENGINE_NAME as ENGINE,
    CASE 
        WHEN f.RDB$VALID_BLR = 1 THEN 'VALID'
        ELSE 'INVALID'
    END as STATUS,
    f.RDB$MODULE_NAME as LIBRARY_PATH,
    f.RDB$ENTRYPOINT as ENTRY_POINT,
    (SELECT COUNT(*) 
     FROM MON$CALL_STACK cs 
     WHERE cs.MON$OBJECT_NAME = f.RDB$FUNCTION_NAME) as ACTIVE_CALLS
FROM RDB$FUNCTIONS f
WHERE f.RDB$ENGINE_NAME LIKE 'UDR_%'
UNION ALL
SELECT 
    p.RDB$PROCEDURE_NAME,
    p.RDB$ENGINE_NAME,
    CASE 
        WHEN p.RDB$VALID_BLR = 1 THEN 'VALID'
        ELSE 'INVALID'
    END,
    p.RDB$MODULE_NAME,
    p.RDB$ENTRYPOINT,
    (SELECT COUNT(*) 
     FROM MON$CALL_STACK cs 
     WHERE cs.MON$OBJECT_NAME = p.RDB$PROCEDURE_NAME)
FROM RDB$PROCEDURES p
WHERE p.RDB$ENGINE_NAME LIKE 'UDR_%';

-- Check UDR status
SELECT * FROM UDR_HEALTH_STATUS;
```

#### Automated Testing
```bash
#!/bin/bash
# udr_health_check.sh

UDR_TEST_DB="/data/test_database.fdb"
LOG_FILE="/var/log/scratchbird/udr_health.log"

echo "$(date): Starting UDR health check" >> $LOG_FILE

# Test each UDR function
sb_isql -u SYSDBA -p masterkey $UDR_TEST_DB << EOF >> $LOG_FILE 2>&1

-- Test calculate_tax function
SELECT 'Testing calculate_tax' as test_name, 
       calculate_tax(1000.00, 10.0) as result;

-- Test array functions
SELECT 'Testing sort_numbers' as test_name,
       sort_numbers(ARRAY[5,2,8,1,9]) as result;

-- Test procedures
SELECT 'Testing process_sales_data' as test_name,
       p.total_sales, p.transaction_count
FROM process_sales_data('2025-01-01', '2025-01-31', 'TEST') p;

EOF

if [ $? -eq 0 ]; then
    echo "$(date): UDR health check completed successfully" >> $LOG_FILE
else
    echo "$(date): UDR health check FAILED" >> $LOG_FILE
    # Send alert
    mail -s "UDR Health Check Failed" admin@company.com < $LOG_FILE
fi
```

---

## Error Handling

### Exception Management

#### C++ Exception Handling
```cpp
#include "UdrCppEngine.h"
#include <stdexcept>

FB_UDR_IMPLEMENT_FUNCTION(robust_calculation, Input, Output) {
    try {
        // Validate inputs
        if (in->value.isNull()) {
            throw std::invalid_argument("Input value cannot be NULL");
        }
        
        double value = in->value.get();
        if (value < 0) {
            throw std::domain_error("Input value must be non-negative");
        }
        
        // Perform calculation
        double result = sqrt(value);
        out->result.set(result);
        
    } catch (const std::invalid_argument& e) {
        status->setException(FBException::create(
            isc_udr_invalid_argument, e.what()));
    } catch (const std::domain_error& e) {
        status->setException(FBException::create(
            isc_udr_domain_error, e.what()));
    } catch (const std::exception& e) {
        status->setException(FBException::create(
            isc_udr_generic_error, "Unexpected error: " + std::string(e.what())));
    } catch (...) {
        status->setException(FBException::create(
            isc_udr_unknown_error, "Unknown error occurred"));
    }
}
```

#### Python Exception Handling
```python
import scratchbird_udr as sb
import logging
import traceback

logger = logging.getLogger(__name__)

@sb.udr_function
def safe_division(dividend: float, divisor: float) -> float:
    """Division function with comprehensive error handling"""
    
    try:
        # Input validation
        if divisor == 0:
            sb.raise_error("Division by zero is not allowed", 
                          error_code=sb.ErrorCodes.MATH_ERROR)
        
        if abs(divisor) < 1e-10:
            sb.raise_error("Divisor too close to zero", 
                          error_code=sb.ErrorCodes.PRECISION_ERROR)
        
        # Calculation
        result = dividend / divisor
        
        # Result validation
        if not (-1e308 < result < 1e308):
            sb.raise_error("Result overflow", 
                          error_code=sb.ErrorCodes.OVERFLOW_ERROR)
        
        logger.info(f"Division successful: {dividend} / {divisor} = {result}")
        return result
        
    except sb.UDRError:
        # Re-raise UDR errors
        raise
    except Exception as e:
        # Log unexpected errors
        logger.error(f"Unexpected error in safe_division: {str(e)}")
        logger.error(traceback.format_exc())
        
        # Convert to UDR error
        sb.raise_error(f"Calculation failed: {str(e)}", 
                      error_code=sb.ErrorCodes.INTERNAL_ERROR)
```

#### Java Exception Handling
```java
import com.scratchbird.udr.*;
import java.util.logging.Logger;
import java.util.logging.Level;

public class DataProcessor {
    private static final Logger logger = Logger.getLogger(DataProcessor.class.getName());
    
    @UDRFunction
    public static String processData(String input) throws UDRException {
        try {
            // Validate input
            if (input == null || input.trim().isEmpty()) {
                throw new UDRException("Input cannot be null or empty", 
                                     UDRErrorCodes.INVALID_ARGUMENT);
            }
            
            if (input.length() > 10000) {
                throw new UDRException("Input too large (max 10000 characters)", 
                                     UDRErrorCodes.INPUT_TOO_LARGE);
            }
            
            // Process data
            String result = performComplexProcessing(input);
            
            logger.info("Data processing completed successfully");
            return result;
            
        } catch (UDRException e) {
            // Re-throw UDR exceptions
            logger.log(Level.WARNING, "UDR error: " + e.getMessage());
            throw e;
        } catch (OutOfMemoryError e) {
            logger.log(Level.SEVERE, "Out of memory error", e);
            throw new UDRException("Insufficient memory for processing", 
                                 UDRErrorCodes.OUT_OF_MEMORY);
        } catch (Exception e) {
            logger.log(Level.SEVERE, "Unexpected error", e);
            throw new UDRException("Processing failed: " + e.getMessage(), 
                                 UDRErrorCodes.INTERNAL_ERROR);
        }
    }
}
```

### Error Codes and Messages

#### Standard UDR Error Codes
```sql
-- Query UDR error information
SELECT 
    error_code,
    error_name,
    error_description
FROM SYSTEM.UDR_ERROR_CODES
ORDER BY error_code;

-- Common error codes:
-- 1001: Invalid argument
-- 1002: Null argument not allowed
-- 1003: Value out of range
-- 1004: Type conversion error
-- 1005: Memory allocation error
-- 1006: Timeout error
-- 1007: Permission denied
-- 1008: Resource not available
-- 1009: Internal error
-- 1010: Unknown error
```

#### Custom Error Handling Framework
```cpp
// Custom error handling framework
class UDRErrorHandler {
private:
    static std::map<int, std::string> errorMessages;
    
public:
    enum ErrorCodes {
        SUCCESS = 0,
        INVALID_INPUT = 1001,
        CALCULATION_ERROR = 1002,
        RESOURCE_ERROR = 1003,
        SECURITY_ERROR = 1004
    };
    
    static void throwError(ErrorCodes code, const std::string& context = "") {
        std::string message = getErrorMessage(code);
        if (!context.empty()) {
            message += " (Context: " + context + ")";
        }
        
        FBException::create(static_cast<ISC_STATUS>(code), message.c_str());
    }
    
    static std::string getErrorMessage(ErrorCodes code) {
        auto it = errorMessages.find(code);
        return (it != errorMessages.end()) ? it->second : "Unknown error";
    }
};

// Initialize error messages
std::map<int, std::string> UDRErrorHandler::errorMessages = {
    {INVALID_INPUT, "Invalid input parameter provided"},
    {CALCULATION_ERROR, "Error occurred during calculation"},
    {RESOURCE_ERROR, "Unable to access required resource"},
    {SECURITY_ERROR, "Security validation failed"}
};
```

---

## Advanced Features

### Streaming Data Processing

#### Stream Processing UDR
```cpp
// Stream processing for large datasets
FB_UDR_IMPLEMENT_PROCEDURE(stream_processor,
    (FB_VARCHAR(255), input_query)(FB_INTEGER, batch_size),
    (FB_BIGINT, record_count)(FB_VARCHAR(1000), status_message))
{
    std::string query = in->input_query.get();
    int batchSize = in->batch_size.get();
    
    try {
        // Open cursor for streaming
        auto cursor = openCursor(query);
        long long recordCount = 0;
        
        while (cursor->hasNext()) {
            // Process in batches
            std::vector<Record> batch;
            for (int i = 0; i < batchSize && cursor->hasNext(); ++i) {
                batch.push_back(cursor->next());
            }
            
            // Process batch
            processBatch(batch);
            recordCount += batch.size();
            
            // Report progress periodically
            if (recordCount % 10000 == 0) {
                out->record_count.set(recordCount);
                out->status_message.set("Processing...");
                status->suspend(); // Yield control
            }
        }
        
        // Final result
        out->record_count.set(recordCount);
        out->status_message.set("Completed successfully");
        status->suspend();
        
    } catch (...) {
        out->record_count.set(-1);
        out->status_message.set("Processing failed");
        status->suspend();
    }
}
```

### Asynchronous Processing

#### Async UDR with Future/Promise
```cpp
#include <future>
#include <thread>

FB_UDR_IMPLEMENT_FUNCTION(async_calculation, Input, Output) {
    std::string taskId = generateTaskId();
    double inputValue = in->value.get();
    
    // Start async processing
    auto future = std::async(std::launch::async, [inputValue]() {
        // Simulate long-running calculation
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return inputValue * 3.14159;
    });
    
    // Store future for later retrieval
    AsyncTaskManager::instance().storeFuture(taskId, std::move(future));
    
    // Return task ID for polling
    out->task_id.set(taskId.c_str());
}

FB_UDR_IMPLEMENT_FUNCTION(get_async_result, Input, Output) {
    std::string taskId = in->task_id.get();
    
    auto future = AsyncTaskManager::instance().getFuture(taskId);
    if (!future) {
        status->setException(FBException::create("Invalid task ID"));
        return;
    }
    
    if (future->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        double result = future->get();
        out->result.set(result);
        out->is_ready.set(true);
        
        // Clean up completed task
        AsyncTaskManager::instance().removeTask(taskId);
    } else {
        out->is_ready.set(false);
    }
}
```

### Machine Learning Integration

#### Scikit-learn UDR
```python
import scratchbird_udr as sb
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.externals import joblib
import os

# Global model cache
_models = {}

@sb.udr_function
def predict_classification(model_name: str, features: List[float]) -> str:
    """Make prediction using trained ML model"""
    
    try:
        # Load model if not cached
        if model_name not in _models:
            model_path = f"/models/{model_name}.pkl"
            if not os.path.exists(model_path):
                sb.raise_error(f"Model not found: {model_name}")
            
            _models[model_name] = joblib.load(model_path)
        
        model = _models[model_name]
        
        # Convert to numpy array and predict
        X = np.array(features).reshape(1, -1)
        prediction = model.predict(X)[0]
        
        return str(prediction)
        
    except Exception as e:
        sb.raise_error(f"Prediction failed: {str(e)}")

@sb.udr_procedure
def train_classification_model(
    model_name: str,
    training_query: str,
    target_column: str
) -> sb.Result:
    """Train a new classification model"""
    
    try:
        # Execute query to get training data
        df = sb.execute_query(training_query)
        
        # Separate features and target
        X = df.drop(columns=[target_column])
        y = df[target_column]
        
        # Train model
        model = RandomForestClassifier(n_estimators=100, random_state=42)
        model.fit(X, y)
        
        # Save model
        model_path = f"/models/{model_name}.pkl"
        joblib.dump(model, model_path)
        
        # Cache model
        _models[model_name] = model
        
        # Calculate accuracy
        accuracy = model.score(X, y)
        
        return sb.Result(
            success=True,
            message=f"Model trained successfully with accuracy: {accuracy:.3f}",
            model_path=model_path
        )
        
    except Exception as e:
        return sb.Result(
            success=False,
            message=f"Training failed: {str(e)}",
            model_path=None
        )
```

### Web Service Integration

#### REST API UDR
```java
import com.scratchbird.udr.*;
import java.net.http.*;
import java.net.URI;
import java.time.Duration;
import com.fasterxml.jackson.databind.ObjectMapper;

public class WebServiceUDR {
    private static final HttpClient httpClient = HttpClient.newBuilder()
        .connectTimeout(Duration.ofSeconds(10))
        .build();
    
    private static final ObjectMapper objectMapper = new ObjectMapper();
    
    @UDRFunction
    public static String callRestAPI(String url, String method, String payload) 
        throws UDRException {
        try {
            HttpRequest.Builder requestBuilder = HttpRequest.newBuilder()
                .uri(URI.create(url))
                .timeout(Duration.ofSeconds(30))
                .header("Content-Type", "application/json");
            
            // Set HTTP method and body
            switch (method.toUpperCase()) {
                case "GET":
                    requestBuilder.GET();
                    break;
                case "POST":
                    requestBuilder.POST(HttpRequest.BodyPublishers.ofString(payload));
                    break;
                case "PUT":
                    requestBuilder.PUT(HttpRequest.BodyPublishers.ofString(payload));
                    break;
                default:
                    throw new UDRException("Unsupported HTTP method: " + method);
            }
            
            HttpRequest request = requestBuilder.build();
            HttpResponse<String> response = httpClient.send(request, 
                HttpResponse.BodyHandlers.ofString());
            
            if (response.statusCode() >= 200 && response.statusCode() < 300) {
                return response.body();
            } else {
                throw new UDRException("HTTP error: " + response.statusCode() + 
                                     " - " + response.body());
            }
            
        } catch (Exception e) {
            throw new UDRException("API call failed: " + e.getMessage());
        }
    }
    
    @UDRFunction
    public static String parseJsonField(String jsonString, String fieldPath) 
        throws UDRException {
        try {
            JsonNode jsonNode = objectMapper.readTree(jsonString);
            
            // Support nested field access with dot notation
            String[] pathElements = fieldPath.split("\\.");
            JsonNode currentNode = jsonNode;
            
            for (String element : pathElements) {
                currentNode = currentNode.get(element);
                if (currentNode == null) {
                    return null;
                }
            }
            
            return currentNode.asText();
            
        } catch (Exception e) {
            throw new UDRException("JSON parsing failed: " + e.getMessage());
        }
    }
}
```

---

## Best Practices

### Development Best Practices

#### Code Organization
```cpp
// Good: Organized header structure
// myudr.h
#ifndef MYUDR_H
#define MYUDR_H

#include "UdrCppEngine.h"
#include <memory>
#include <string>

namespace MyCompany {
namespace UDR {
    
    // Forward declarations
    class Calculator;
    class DataProcessor;
    
    // UDR function declarations
    extern "C" {
        FB_UDR_FUNCTION(calculate_tax);
        FB_UDR_PROCEDURE(process_sales_data);
    }
    
    // Helper classes
    class Calculator {
    public:
        static double calculateTax(double income, double rate);
        static bool validateInput(double value);
    };
    
} // namespace UDR
} // namespace MyCompany

#endif // MYUDR_H
```

#### Error Handling Patterns
```python
# Good: Consistent error handling pattern
import scratchbird_udr as sb
from typing import Optional
import logging

logger = logging.getLogger(__name__)

def validate_and_execute(func):
    """Decorator for consistent validation and error handling"""
    def wrapper(*args, **kwargs):
        try:
            # Log function entry
            logger.debug(f"Executing {func.__name__} with args: {args}")
            
            # Execute function
            result = func(*args, **kwargs)
            
            # Log success
            logger.debug(f"{func.__name__} completed successfully")
            return result
            
        except sb.UDRError:
            # Re-raise UDR errors
            raise
        except Exception as e:
            logger.error(f"Error in {func.__name__}: {str(e)}")
            sb.raise_error(f"Function {func.__name__} failed: {str(e)}")
    
    return wrapper

@sb.udr_function
@validate_and_execute
def safe_function(input_value: float) -> float:
    """Function with standardized error handling"""
    if input_value < 0:
        sb.raise_error("Input must be non-negative")
    
    return math.sqrt(input_value)
```

#### Documentation Standards
```java
/**
 * UDR for financial calculations
 * 
 * @author John Doe
 * @version 1.2.0
 * @since 2025-01-01
 */
public class FinancialUDR {
    
    /**
     * Calculates compound interest
     * 
     * @param principal The initial principal amount
     * @param rate Annual interest rate (as decimal, e.g., 0.05 for 5%)
     * @param time Time period in years
     * @param frequency Compounding frequency per year
     * @return The compound interest amount
     * @throws UDRException if any parameter is invalid
     * 
     * @example
     * SELECT compound_interest(1000.00, 0.05, 10, 12) as interest;
     * -- Returns: compound interest for $1000 at 5% for 10 years, compounded monthly
     */
    @UDRFunction
    public static double compoundInterest(double principal, double rate, 
                                        double time, int frequency) 
        throws UDRException {
        
        // Validate inputs
        if (principal <= 0) {
            throw new UDRException("Principal must be positive");
        }
        if (rate < 0) {
            throw new UDRException("Interest rate cannot be negative");
        }
        if (time <= 0) {
            throw new UDRException("Time period must be positive");
        }
        if (frequency <= 0) {
            throw new UDRException("Compounding frequency must be positive");
        }
        
        // Calculate compound interest
        double amount = principal * Math.pow(1 + rate / frequency, frequency * time);
        return amount - principal;
    }
}
```

### Performance Best Practices

#### Memory Management
```cpp
// Good: RAII and smart pointers
class UDRResource {
private:
    std::unique_ptr<char[]> buffer;
    size_t size;
    
public:
    explicit UDRResource(size_t sz) : size(sz) {
        buffer = std::make_unique<char[]>(size);
    }
    
    // Rule of 5
    ~UDRResource() = default;
    UDRResource(const UDRResource&) = delete;
    UDRResource& operator=(const UDRResource&) = delete;
    UDRResource(UDRResource&&) = default;
    UDRResource& operator=(UDRResource&&) = default;
    
    char* data() { return buffer.get(); }
    size_t getSize() const { return size; }
};

FB_UDR_IMPLEMENT_FUNCTION(memory_efficient_function, Input, Output) {
    // Use RAII for automatic cleanup
    UDRResource resource(in->buffer_size.get());
    
    // Process data
    processData(resource.data(), resource.getSize());
    
    // Automatic cleanup when function exits
}
```

#### Threading Safety
```java
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantReadWriteLock;

public class ThreadSafeUDR {
    // Thread-safe collections
    private static final ConcurrentHashMap<String, Object> cache = 
        new ConcurrentHashMap<>();
    
    // Read-write lock for complex operations
    private static final ReadWriteLock lock = new ReentrantReadWriteLock();
    
    @UDRFunction
    public static String getCachedData(String key) throws UDRException {
        // Thread-safe read
        Object value = cache.get(key);
        return value != null ? value.toString() : null;
    }
    
    @UDRFunction
    public static void updateCache(String key, String value) throws UDRException {
        // Thread-safe write with validation
        lock.writeLock().lock();
        try {
            if (validateValue(value)) {
                cache.put(key, value);
            } else {
                throw new UDRException("Invalid value for cache update");
            }
        } finally {
            lock.writeLock().unlock();
        }
    }
    
    private static boolean validateValue(String value) {
        lock.readLock().lock();
        try {
            // Perform validation that requires read access
            return value != null && value.length() > 0;
        } finally {
            lock.readLock().unlock();
        }
    }
}
```

### Security Best Practices

#### Input Sanitization
```python
import re
import html
import scratchbird_udr as sb

def sanitize_input(input_string: str, max_length: int = 1000) -> str:
    """Sanitize user input for security"""
    
    if not input_string:
        return ""
    
    # Length check
    if len(input_string) > max_length:
        sb.raise_error(f"Input too long (max {max_length} characters)")
    
    # Remove or escape dangerous characters
    sanitized = html.escape(input_string)
    
    # Remove SQL injection attempts
    dangerous_patterns = [
        r'(union|select|insert|update|delete|drop|exec|execute)',
        r'(script|javascript|vbscript)',
        r'(--|\/\*|\*\/)'
    ]
    
    for pattern in dangerous_patterns:
        if re.search(pattern, sanitized, re.IGNORECASE):
            sb.raise_error("Input contains potentially dangerous content")
    
    return sanitized

@sb.udr_function
def secure_search(search_term: str) -> str:
    """Secure search function with input sanitization"""
    
    # Sanitize input
    clean_term = sanitize_input(search_term, max_length=100)
    
    # Perform search
    return perform_search(clean_term)
```

---

## Examples and Tutorials

### Complete UDR Development Example

This example demonstrates creating a complete UDR library for financial calculations.

#### Project Structure
```
financial_udrs/
├── src/
│   ├── financial_udrs.cpp
│   ├── financial_udrs.h
│   └── utils.cpp
├── python/
│   ├── financial_python.py
│   └── requirements.txt
├── java/
│   └── FinancialUDRs.java
├── CMakeLists.txt
├── Makefile
└── deployment/
    ├── install.sh
    └── register_functions.sql
```

#### C++ Implementation
```cpp
// financial_udrs.h
#ifndef FINANCIAL_UDRS_H
#define FINANCIAL_UDRS_H

#include "UdrCppEngine.h"
#include <cmath>

extern "C" {
    // Interest calculations
    FB_UDR_FUNCTION(simple_interest);
    FB_UDR_FUNCTION(compound_interest);
    FB_UDR_FUNCTION(present_value);
    FB_UDR_FUNCTION(future_value);
    
    // Risk calculations
    FB_UDR_FUNCTION(value_at_risk);
    FB_UDR_FUNCTION(sharpe_ratio);
    
    // Portfolio analysis
    FB_UDR_PROCEDURE(portfolio_analysis);
}

#endif
```

```cpp
// financial_udrs.cpp
#include "financial_udrs.h"
#include <vector>
#include <algorithm>

// Simple Interest: I = PRT
FB_UDR_IMPLEMENT_FUNCTION(simple_interest,
    (FB_DOUBLE, principal)(FB_DOUBLE, rate)(FB_DOUBLE, time),
    (FB_DOUBLE, interest))
{
    try {
        double p = in->principal.get();
        double r = in->rate.get();
        double t = in->time.get();
        
        // Validation
        if (p <= 0) throw std::invalid_argument("Principal must be positive");
        if (r < 0) throw std::invalid_argument("Rate cannot be negative");
        if (t <= 0) throw std::invalid_argument("Time must be positive");
        
        double interest = p * r * t;
        out->interest.set(interest);
        
    } catch (const std::exception& e) {
        status->setException(FBException::create(e.what()));
    }
}

// Compound Interest: A = P(1 + r/n)^(nt) - P
FB_UDR_IMPLEMENT_FUNCTION(compound_interest,
    (FB_DOUBLE, principal)(FB_DOUBLE, rate)(FB_DOUBLE, time)(FB_INTEGER, frequency),
    (FB_DOUBLE, interest))
{
    try {
        double p = in->principal.get();
        double r = in->rate.get();
        double t = in->time.get();
        int n = in->frequency.get();
        
        // Validation
        if (p <= 0) throw std::invalid_argument("Principal must be positive");
        if (r < 0) throw std::invalid_argument("Rate cannot be negative");
        if (t <= 0) throw std::invalid_argument("Time must be positive");
        if (n <= 0) throw std::invalid_argument("Frequency must be positive");
        
        double amount = p * pow(1 + r / n, n * t);
        double interest = amount - p;
        out->interest.set(interest);
        
    } catch (const std::exception& e) {
        status->setException(FBException::create(e.what()));
    }
}

// Portfolio Analysis Procedure
FB_UDR_IMPLEMENT_PROCEDURE(portfolio_analysis,
    (FB_VARCHAR(100), portfolio_name),
    (FB_DOUBLE, total_value)(FB_DOUBLE, expected_return)(FB_DOUBLE, volatility)(FB_VARCHAR(20), risk_level))
{
    try {
        std::string portfolio = in->portfolio_name.get();
        
        // Simulate portfolio analysis
        // In real implementation, this would query portfolio data
        double totalValue = calculatePortfolioValue(portfolio);
        double expectedReturn = calculateExpectedReturn(portfolio);
        double vol = calculateVolatility(portfolio);
        
        std::string riskLevel;
        if (vol < 0.1) riskLevel = "LOW";
        else if (vol < 0.2) riskLevel = "MEDIUM";
        else riskLevel = "HIGH";
        
        out->total_value.set(totalValue);
        out->expected_return.set(expectedReturn);
        out->volatility.set(vol);
        out->risk_level.set(riskLevel.c_str());
        
        status->suspend();
        
    } catch (const std::exception& e) {
        status->setException(FBException::create(e.what()));
    }
}
```

#### Python Implementation
```python
# financial_python.py
import scratchbird_udr as sb
import numpy as np
import pandas as pd
from scipy import stats
from typing import List, Tuple

@sb.udr_function
def black_scholes_option_price(
    spot_price: float,
    strike_price: float,
    time_to_expiry: float,
    risk_free_rate: float,
    volatility: float,
    option_type: str
) -> float:
    """Calculate Black-Scholes option price"""
    
    try:
        # Validation
        if spot_price <= 0:
            sb.raise_error("Spot price must be positive")
        if strike_price <= 0:
            sb.raise_error("Strike price must be positive")
        if time_to_expiry <= 0:
            sb.raise_error("Time to expiry must be positive")
        if volatility <= 0:
            sb.raise_error("Volatility must be positive")
        
        # Black-Scholes calculation
        d1 = (np.log(spot_price / strike_price) + 
              (risk_free_rate + 0.5 * volatility**2) * time_to_expiry) / (volatility * np.sqrt(time_to_expiry))
        d2 = d1 - volatility * np.sqrt(time_to_expiry)
        
        if option_type.upper() == 'CALL':
            price = (spot_price * stats.norm.cdf(d1) - 
                    strike_price * np.exp(-risk_free_rate * time_to_expiry) * stats.norm.cdf(d2))
        elif option_type.upper() == 'PUT':
            price = (strike_price * np.exp(-risk_free_rate * time_to_expiry) * stats.norm.cdf(-d2) - 
                    spot_price * stats.norm.cdf(-d1))
        else:
            sb.raise_error("Option type must be 'CALL' or 'PUT'")
        
        return float(price)
        
    except Exception as e:
        sb.raise_error(f"Black-Scholes calculation failed: {str(e)}")

@sb.udr_procedure
def monte_carlo_var(
    portfolio_query: str,
    confidence_level: float,
    time_horizon: int,
    num_simulations: int
) -> sb.Result:
    """Calculate Value at Risk using Monte Carlo simulation"""
    
    try:
        # Execute query to get portfolio data
        portfolio_data = sb.execute_query(portfolio_query)
        
        # Perform Monte Carlo simulation
        returns = []
        for _ in range(num_simulations):
            # Simulate portfolio return
            simulated_return = simulate_portfolio_return(portfolio_data, time_horizon)
            returns.append(simulated_return)
        
        # Calculate VaR
        returns_sorted = sorted(returns)
        var_index = int((1 - confidence_level) * num_simulations)
        var_value = returns_sorted[var_index]
        
        # Calculate additional metrics
        expected_return = np.mean(returns)
        std_dev = np.std(returns)
        
        return sb.Result(
            var_value=float(var_value),
            expected_return=float(expected_return),
            volatility=float(std_dev),
            confidence_level=confidence_level,
            simulations=num_simulations
        )
        
    except Exception as e:
        return sb.Result(
            error=True,
            message=f"VaR calculation failed: {str(e)}"
        )
```

#### Java Implementation
```java
// FinancialUDRs.java
package com.company.financial;

import com.scratchbird.udr.*;
import java.math.BigDecimal;
import java.math.RoundingMode;
import java.util.Arrays;
import java.util.List;

public class FinancialUDRs {
    
    @UDRFunction
    public static double calculateBeta(String stockSymbol, String marketIndex) 
        throws UDRException {
        try {
            // Get historical price data
            List<Double> stockReturns = getStockReturns(stockSymbol);
            List<Double> marketReturns = getMarketReturns(marketIndex);
            
            if (stockReturns.size() != marketReturns.size()) {
                throw new UDRException("Mismatched data series lengths");
            }
            
            // Calculate beta using regression
            double covariance = calculateCovariance(stockReturns, marketReturns);
            double marketVariance = calculateVariance(marketReturns);
            
            if (marketVariance == 0) {
                throw new UDRException("Market variance is zero");
            }
            
            return covariance / marketVariance;
            
        } catch (Exception e) {
            throw new UDRException("Beta calculation failed: " + e.getMessage());
        }
    }
    
    @UDRFunction
    public static BigDecimal calculateNPV(String cashFlows, double discountRate) 
        throws UDRException {
        try {
            // Parse cash flows (comma-separated values)
            String[] flowStrings = cashFlows.split(",");
            double[] flows = Arrays.stream(flowStrings)
                .mapToDouble(Double::parseDouble)
                .toArray();
            
            BigDecimal npv = BigDecimal.ZERO;
            
            for (int i = 0; i < flows.length; i++) {
                double presentValue = flows[i] / Math.pow(1 + discountRate, i);
                npv = npv.add(BigDecimal.valueOf(presentValue));
            }
            
            return npv.setScale(2, RoundingMode.HALF_UP);
            
        } catch (Exception e) {
            throw new UDRException("NPV calculation failed: " + e.getMessage());
        }
    }
    
    @UDRProcedure
    public static PortfolioOptimizationResult optimizePortfolio(
        String[] assets, 
        double[] expectedReturns, 
        double[][] covarianceMatrix,
        double targetReturn
    ) throws UDRException {
        
        try {
            // Validate inputs
            if (assets.length != expectedReturns.length) {
                throw new UDRException("Assets and returns length mismatch");
            }
            
            // Perform portfolio optimization using Modern Portfolio Theory
            PortfolioOptimizer optimizer = new PortfolioOptimizer();
            OptimizationResult result = optimizer.optimize(
                expectedReturns, covarianceMatrix, targetReturn);
            
            return new PortfolioOptimizationResult(
                result.getWeights(),
                result.getExpectedReturn(),
                result.getVolatility(),
                result.getSharpeRatio()
            );
            
        } catch (Exception e) {
            throw new UDRException("Portfolio optimization failed: " + e.getMessage());
        }
    }
    
    // Helper methods
    private static List<Double> getStockReturns(String symbol) {
        // Implementation to fetch stock returns
        // This would typically query a financial data service
        return Arrays.asList(/* stock returns */);
    }
    
    private static List<Double> getMarketReturns(String index) {
        // Implementation to fetch market returns
        return Arrays.asList(/* market returns */);
    }
    
    private static double calculateCovariance(List<Double> x, List<Double> y) {
        // Covariance calculation implementation
        double meanX = x.stream().mapToDouble(Double::doubleValue).average().orElse(0);
        double meanY = y.stream().mapToDouble(Double::doubleValue).average().orElse(0);
        
        double sum = 0;
        for (int i = 0; i < x.size(); i++) {
            sum += (x.get(i) - meanX) * (y.get(i) - meanY);
        }
        
        return sum / (x.size() - 1);
    }
    
    private static double calculateVariance(List<Double> values) {
        // Variance calculation implementation
        double mean = values.stream().mapToDouble(Double::doubleValue).average().orElse(0);
        double sumSquaredDeviations = values.stream()
            .mapToDouble(v -> Math.pow(v - mean, 2))
            .sum();
        
        return sumSquaredDeviations / (values.size() - 1);
    }
}
```

#### Deployment Scripts
```bash
#!/bin/bash
# install.sh

echo "Installing Financial UDRs..."

# Build C++ library
cd src
make clean
make -j$(nproc)

# Copy library
sudo cp libfinancial_udrs.so /usr/local/lib/scratchbird/udr/
sudo chmod 755 /usr/local/lib/scratchbird/udr/libfinancial_udrs.so

# Install Python dependencies
pip install -r python/requirements.txt

# Copy Python modules
sudo cp python/financial_python.py /usr/local/lib/scratchbird/udr/python/

# Compile Java
javac -cp /usr/local/lib/scratchbird/udr-java.jar java/FinancialUDRs.java
jar cf financial_udrs.jar java/*.class
sudo cp financial_udrs.jar /usr/local/lib/scratchbird/udr/java/

# Register functions
sb_isql -u SYSDBA -p masterkey your_database.fdb -i deployment/register_functions.sql

echo "Installation complete!"
```

```sql
-- register_functions.sql

-- C++ Functions
CREATE OR ALTER FUNCTION simple_interest (
    principal DOUBLE PRECISION NOT NULL,
    rate DOUBLE PRECISION NOT NULL,
    time DOUBLE PRECISION NOT NULL
) RETURNS DOUBLE PRECISION NOT NULL
EXTERNAL NAME 'libfinancial_udrs.so!simple_interest'
ENGINE UDR_CPP;

CREATE OR ALTER FUNCTION compound_interest (
    principal DOUBLE PRECISION NOT NULL,
    rate DOUBLE PRECISION NOT NULL,
    time DOUBLE PRECISION NOT NULL,
    frequency INTEGER NOT NULL
) RETURNS DOUBLE PRECISION NOT NULL
EXTERNAL NAME 'libfinancial_udrs.so!compound_interest'
ENGINE UDR_CPP;

-- Python Functions
CREATE OR ALTER FUNCTION black_scholes_option_price (
    spot_price DOUBLE PRECISION NOT NULL,
    strike_price DOUBLE PRECISION NOT NULL,
    time_to_expiry DOUBLE PRECISION NOT NULL,
    risk_free_rate DOUBLE PRECISION NOT NULL,
    volatility DOUBLE PRECISION NOT NULL,
    option_type VARCHAR(10) NOT NULL
) RETURNS DOUBLE PRECISION NOT NULL
EXTERNAL NAME 'financial_python.py!black_scholes_option_price'
ENGINE UDR_PYTHON;

-- Java Functions
CREATE OR ALTER FUNCTION calculate_beta (
    stock_symbol VARCHAR(10) NOT NULL,
    market_index VARCHAR(10) NOT NULL
) RETURNS DOUBLE PRECISION NOT NULL
EXTERNAL NAME 'com.company.financial.FinancialUDRs!calculateBeta'
ENGINE UDR_JAVA;

-- Procedures
CREATE OR ALTER PROCEDURE portfolio_analysis (
    portfolio_name VARCHAR(100) NOT NULL
) RETURNS (
    total_value DOUBLE PRECISION NOT NULL,
    expected_return DOUBLE PRECISION NOT NULL,
    volatility DOUBLE PRECISION NOT NULL,
    risk_level VARCHAR(20) NOT NULL
)
EXTERNAL NAME 'libfinancial_udrs.so!portfolio_analysis'
ENGINE UDR_CPP;

-- Grant privileges
GRANT EXECUTE ON FUNCTION simple_interest TO PUBLIC;
GRANT EXECUTE ON FUNCTION compound_interest TO PUBLIC;
GRANT EXECUTE ON FUNCTION black_scholes_option_price TO ROLE FINANCIAL_ANALYSTS;
GRANT EXECUTE ON FUNCTION calculate_beta TO ROLE FINANCIAL_ANALYSTS;
GRANT EXECUTE ON PROCEDURE portfolio_analysis TO ROLE PORTFOLIO_MANAGERS;

COMMIT;
```

#### Usage Examples
```sql
-- Simple interest calculation
SELECT simple_interest(10000.00, 0.05, 3) as interest;
-- Returns: 1500.00

-- Compound interest calculation
SELECT compound_interest(10000.00, 0.05, 3, 12) as interest;
-- Returns: 1614.74

-- Black-Scholes option pricing
SELECT black_scholes_option_price(100, 105, 0.25, 0.05, 0.20, 'CALL') as option_price;
-- Returns: 2.13

-- Beta calculation
SELECT calculate_beta('AAPL', 'SPY') as stock_beta;
-- Returns: 1.23

-- Portfolio analysis
SELECT * FROM portfolio_analysis('AGGRESSIVE_GROWTH');
-- Returns: Portfolio metrics
```

---

## Troubleshooting

### Common Issues and Solutions

#### Compilation Issues

**Issue**: UDR library fails to compile
```bash
# Error: undefined reference to ScratchBird UDR symbols
g++: error: undefined reference to 'FB_UDR_IMPLEMENT_FUNCTION'
```

**Solution**: Check compiler flags and library paths
```bash
# Correct compilation command
g++ -shared -fPIC -o libudr.so udr.cpp \
    -I/usr/local/include/scratchbird \
    -L/usr/local/lib \
    -lscratchbird_udr \
    -std=c++17 \
    -O2

# Verify library installation
ldconfig -p | grep scratchbird
```

**Issue**: Python UDR import errors
```python
# Error: No module named 'scratchbird_udr'
ModuleNotFoundError: No module named 'scratchbird_udr'
```

**Solution**: Install Python UDR package
```bash
# Install from package manager
pip install scratchbird-udr-python

# Or install from source
cd /usr/local/src/scratchbird/python-udr
python setup.py install

# Verify installation
python -c "import scratchbird_udr; print('UDR module installed')"
```

#### Runtime Issues

**Issue**: UDR function not found
```sql
-- Error when calling UDR
SELECT my_function(123);
-- Statement failed, SQLSTATE = 39000
-- UDR: function MY_FUNCTION is not defined
```

**Solution**: Check function registration and library path
```sql
-- Verify function exists
SELECT RDB$FUNCTION_NAME, RDB$MODULE_NAME, RDB$ENTRYPOINT
FROM RDB$FUNCTIONS
WHERE RDB$FUNCTION_NAME = 'MY_FUNCTION';

-- Check library file exists
-- Library path should match RDB$MODULE_NAME

-- Re-register if necessary
DROP FUNCTION my_function;
CREATE OR ALTER FUNCTION my_function(x INTEGER)
RETURNS INTEGER
EXTERNAL NAME 'libmyudr.so!my_function'
ENGINE UDR_CPP;
```

**Issue**: UDR execution timeout
```sql
-- Error: UDR execution timeout
-- Execution timeout after 30000ms
```

**Solution**: Configure timeout settings
```ini
# scratchbird.conf
[UDR]
ExecutionTimeoutMS = 60000  # Increase to 60 seconds
DefaultMemoryLimitMB = 512  # Increase memory limit if needed
```

#### Memory Issues

**Issue**: UDR causes memory leaks
```bash
# Monitor memory usage
ps aux | grep scratchbird
# Shows increasing memory usage over time
```

**Solution**: Implement proper memory management
```cpp
// Bad: Manual memory management
char* buffer = new char[size];
// ... use buffer ...
// delete[] buffer; // Often forgotten

// Good: RAII with smart pointers
auto buffer = std::make_unique<char[]>(size);
// Automatic cleanup when scope ends

// Good: Use UDR memory manager
FBAutoPtr<char> buffer = status->allocateMemory<char>(size);
// Automatically freed by UDR engine
```

#### Security Issues

**Issue**: UDR security violations
```log
# Error in log file
UDR Security: File system access denied for function 'read_file'
UDR Security: Network access blocked for function 'http_request'
```

**Solution**: Configure security settings
```ini
# scratchbird.conf - Allow specific access
[UDR_Security]
EnableFileSystemAccess = true
AllowedPaths = /data/exports,/tmp/udr_temp
EnableNetworkAccess = true
AllowedHosts = api.company.com,data.partner.com
```

**Issue**: UDR privilege errors
```sql
-- Error: Insufficient privileges to execute UDR
-- SQLSTATE = 28000
-- no permission for EXECUTE access to FUNCTION MY_FUNCTION
```

**Solution**: Grant appropriate privileges
```sql
-- Grant execute privilege to user
GRANT EXECUTE ON FUNCTION my_function TO user_name;

-- Grant execute privilege to role
GRANT EXECUTE ON FUNCTION my_function TO role_name;

-- Check current privileges
SELECT * FROM RDB$USER_PRIVILEGES
WHERE RDB$RELATION_NAME = 'MY_FUNCTION'
  AND RDB$PRIVILEGE = 'X';  -- X = EXECUTE
```

### Debugging Techniques

#### C++ Debugging
```cpp
// Enable debugging in UDR
#ifdef DEBUG_UDR
#include <iostream>
#include <fstream>

class UDRDebugger {
    static std::ofstream logFile;
public:
    static void log(const std::string& message) {
        if (!logFile.is_open()) {
            logFile.open("/tmp/udr_debug.log", std::ios::app);
        }
        logFile << "[" << getCurrentTime() << "] " << message << std::endl;
        logFile.flush();
    }
};

std::ofstream UDRDebugger::logFile;

#define UDR_DEBUG(msg) UDRDebugger::log(msg)
#else
#define UDR_DEBUG(msg)
#endif

FB_UDR_IMPLEMENT_FUNCTION(debug_function, Input, Output) {
    UDR_DEBUG("Function entry");
    
    try {
        double input = in->value.get();
        UDR_DEBUG("Input value: " + std::to_string(input));
        
        double result = process(input);
        UDR_DEBUG("Result: " + std::to_string(result));
        
        out->result.set(result);
        UDR_DEBUG("Function exit - success");
        
    } catch (const std::exception& e) {
        UDR_DEBUG("Function exit - error: " + std::string(e.what()));
        throw;
    }
}
```

#### Python Debugging
```python
import scratchbird_udr as sb
import logging
import traceback

# Configure logging
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('/tmp/udr_python_debug.log'),
        logging.StreamHandler()
    ]
)

logger = logging.getLogger(__name__)

@sb.udr_function
def debug_function(x: float) -> float:
    """Function with comprehensive debug logging"""
    
    logger.debug(f"Function entry with input: {x}")
    
    try:
        # Validation with logging
        if x < 0:
            logger.warning(f"Negative input detected: {x}")
            sb.raise_error("Input must be non-negative")
        
        # Processing with logging
        logger.debug(f"Starting calculation for: {x}")
        result = math.sqrt(x)
        logger.debug(f"Calculation result: {result}")
        
        logger.debug(f"Function exit - success")
        return result
        
    except sb.UDRError:
        logger.error(f"UDR error occurred")
        raise
    except Exception as e:
        logger.error(f"Unexpected error: {str(e)}")
        logger.error(traceback.format_exc())
        sb.raise_error(f"Calculation failed: {str(e)}")
```

#### Java Debugging
```java
import java.util.logging.Logger;
import java.util.logging.Level;
import java.util.logging.FileHandler;
import java.util.logging.SimpleFormatter;

public class DebuggableUDR {
    private static final Logger logger = Logger.getLogger(DebuggableUDR.class.getName());
    
    static {
        try {
            // Configure logging
            FileHandler fileHandler = new FileHandler("/tmp/udr_java_debug.log", true);
            fileHandler.setFormatter(new SimpleFormatter());
            logger.addHandler(fileHandler);
            logger.setLevel(Level.ALL);
        } catch (Exception e) {
            System.err.println("Failed to configure logging: " + e.getMessage());
        }
    }
    
    @UDRFunction
    public static double debugFunction(double input) throws UDRException {
        logger.info("Function entry with input: " + input);
        
        try {
            // Validation with logging
            if (input < 0) {
                logger.warning("Negative input detected: " + input);
                throw new UDRException("Input must be non-negative");
            }
            
            // Processing with logging
            logger.fine("Starting calculation for: " + input);
            double result = Math.sqrt(input);
            logger.fine("Calculation result: " + result);
            
            logger.info("Function exit - success");
            return result;
            
        } catch (UDRException e) {
            logger.severe("UDR error: " + e.getMessage());
            throw e;
        } catch (Exception e) {
            logger.severe("Unexpected error: " + e.getMessage());
            throw new UDRException("Calculation failed: " + e.getMessage());
        }
    }
}
```

### Performance Debugging

#### Profiling UDR Performance
```sql
-- Monitor UDR performance
CREATE OR ALTER VIEW UDR_PERFORMANCE_MONITOR AS
SELECT 
    f.RDB$FUNCTION_NAME,
    COUNT(*) as call_count,
    AVG(DATEDIFF(MILLISECOND, cs.MON$TIMESTAMP, CURRENT_TIMESTAMP)) as avg_duration_ms,
    MAX(DATEDIFF(MILLISECOND, cs.MON$TIMESTAMP, CURRENT_TIMESTAMP)) as max_duration_ms,
    SUM(mu.MON$MEMORY_USED) as total_memory_used
FROM RDB$FUNCTIONS f
LEFT JOIN MON$CALL_STACK cs ON f.RDB$FUNCTION_NAME = cs.MON$OBJECT_NAME
LEFT JOIN MON$MEMORY_USAGE mu ON cs.MON$STAT_ID = mu.MON$STAT_ID
WHERE f.RDB$ENGINE_NAME LIKE 'UDR_%'
  AND cs.MON$TIMESTAMP >= CURRENT_TIMESTAMP - INTERVAL '1' HOUR
GROUP BY f.RDB$FUNCTION_NAME
ORDER BY avg_duration_ms DESC;

-- View performance data
SELECT * FROM UDR_PERFORMANCE_MONITOR;
```

#### Memory Profiling
```cpp
// Memory usage tracking in C++
class MemoryTracker {
private:
    static std::map<std::string, size_t> allocations;
    static std::mutex tracker_mutex;
    
public:
    static void* allocate(size_t size, const std::string& context) {
        std::lock_guard<std::mutex> lock(tracker_mutex);
        void* ptr = malloc(size);
        allocations[context] += size;
        
        // Log large allocations
        if (size > 1024 * 1024) {  // > 1MB
            std::cout << "Large allocation: " << size << " bytes in " << context << std::endl;
        }
        
        return ptr;
    }
    
    static void deallocate(void* ptr, const std::string& context) {
        std::lock_guard<std::mutex> lock(tracker_mutex);
        free(ptr);
        // Note: In real implementation, track deallocation size
    }
    
    static void printStats() {
        std::lock_guard<std::mutex> lock(tracker_mutex);
        for (const auto& pair : allocations) {
            std::cout << pair.first << ": " << pair.second << " bytes" << std::endl;
        }
    }
};

// Use in UDR functions
FB_UDR_IMPLEMENT_FUNCTION(memory_tracked_function, Input, Output) {
    size_t bufferSize = in->size.get();
    char* buffer = (char*)MemoryTracker::allocate(bufferSize, "my_function_buffer");
    
    try {
        // Use buffer
        processData(buffer, bufferSize);
        
        MemoryTracker::deallocate(buffer, "my_function_buffer");
    } catch (...) {
        MemoryTracker::deallocate(buffer, "my_function_buffer");
        throw;
    }
}
```

---

*This documentation covers ScratchBird Alpha 0.6.0 UDR capabilities. For the latest features and updates, refer to the official ScratchBird documentation and release notes.*