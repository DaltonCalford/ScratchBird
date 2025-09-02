# Guide: Adding a New SQL Function

## Overview
This guide explains how to add a new SQL function to ScratchBird that works across all supported database dialects.

## Steps

### 1. Define the Function Signature

First, add the function definition to the function registry:

```cpp
// src/engine/functions/function_registry.h
class FunctionRegistry {
    void register_string_functions() {
        // Example: Adding REVERSE function
        register_function({
            .name = "REVERSE",
            .category = FunctionCategory::STRING,
            .arg_types = {DataType::VARCHAR},
            .return_type = DataType::VARCHAR,
            .is_deterministic = true,
            .is_null_on_null = true,
            .implementation = reverse_impl
        });
    }
};
```

### 2. Implement the Core Logic

```cpp
// src/engine/functions/string_functions.cpp
Value reverse_impl(const vector<Value>& args) {
    if (args[0].is_null()) {
        return Value::null();
    }
    
    string input = args[0].as_string();
    string output;
    output.reserve(input.length());
    
    // Handle UTF-8 properly
    auto utf8_chars = split_utf8(input);
    reverse(utf8_chars.begin(), utf8_chars.end());
    
    for (const auto& ch : utf8_chars) {
        output += ch;
    }
    
    return Value::from_string(output);
}
```

### 3. Add Dialect Mappings

Map the function to each database's native equivalent:

```cpp
// src/yvalve/dialect_mapper.cpp
class DialectMapper {
    string map_function(const string& func, ClientType client) {
        if (func == "REVERSE") {
            switch (client) {
                case POSTGRESQL:
                    return "REVERSE";  // Native support
                case MYSQL:
                    return "REVERSE";  // Native support
                case MSSQL:
                    return "REVERSE";  // Native support
                case FIREBIRD:
                    return "REVERSE";  // Added in 2.1
                default:
                    return func;
            }
        }
    }
};
```

### 4. Handle Dialect Differences

Some functions have different names or syntax:

```cpp
// Example: String concatenation
class ConcatFunction {
    Value execute(ClientType client, const vector<Value>& args) {
        switch (client) {
            case POSTGRESQL:
            case FIREBIRD:
                // Uses || operator
                return concat_with_operator(args, "||");
                
            case MYSQL:
                // Uses CONCAT() function
                return concat_function(args);
                
            case MSSQL:
                // Uses + operator
                return concat_with_operator(args, "+");
        }
    }
};
```

### 5. Add Parser Support

Update the SQL parser to recognize the function:

```yacc
// src/parser/sql_grammar.y
function_call:
    REVERSE '(' expression ')'
    {
        $$ = make_function_call("REVERSE", {$3});
    }
    ;
```

### 6. Write Tests

Create comprehensive tests for all dialects:

```cpp
// tests/functions/test_reverse.cpp
TEST_F(FunctionTest, Reverse_Basic) {
    auto result = execute("SELECT REVERSE('hello')");
    EXPECT_EQ(result.as_string(), "olleh");
}

TEST_F(FunctionTest, Reverse_UTF8) {
    auto result = execute("SELECT REVERSE('你好')");
    EXPECT_EQ(result.as_string(), "好你");
}

TEST_F(FunctionTest, Reverse_Null) {
    auto result = execute("SELECT REVERSE(NULL)");
    EXPECT_TRUE(result.is_null());
}

TEST_F(FunctionTest, Reverse_AllDialects) {
    for (auto client : {POSTGRESQL, MYSQL, MSSQL, FIREBIRD}) {
        set_client_type(client);
        auto result = execute("SELECT REVERSE('test')");
        EXPECT_EQ(result.as_string(), "tset");
    }
}
```

### 7. Add Documentation

```cpp
// src/engine/functions/function_docs.cpp
FunctionDoc reverse_doc = {
    .name = "REVERSE",
    .description = "Reverses a string",
    .syntax = "REVERSE(string)",
    .parameters = {
        {"string", "The string to reverse"}
    },
    .returns = "The reversed string",
    .examples = {
        {"SELECT REVERSE('hello')", "olleh"},
        {"SELECT REVERSE('12345')", "54321"}
    },
    .compatibility = {
        {POSTGRESQL, "9.1+", "Native"},
        {MYSQL, "5.0+", "Native"},
        {MSSQL, "2008+", "Native"},
        {FIREBIRD, "2.1+", "Native"}
    }
};
```

## Function Categories

### String Functions
Location: `src/engine/functions/string_functions.cpp`

Common functions to implement:
- LENGTH, CHAR_LENGTH, BIT_LENGTH
- UPPER, LOWER, INITCAP
- TRIM, LTRIM, RTRIM
- SUBSTRING, LEFT, RIGHT
- REPLACE, TRANSLATE
- CONCAT, CONCAT_WS
- SPLIT_PART, STRING_TO_ARRAY

### Numeric Functions
Location: `src/engine/functions/numeric_functions.cpp`

- ABS, SIGN, MOD
- ROUND, CEIL, FLOOR, TRUNC
- POWER, SQRT, EXP, LN, LOG
- SIN, COS, TAN, ASIN, ACOS, ATAN
- RANDOM, SETSEED

### Date/Time Functions
Location: `src/engine/functions/datetime_functions.cpp`

- NOW, CURRENT_DATE, CURRENT_TIME
- DATE_PART, EXTRACT
- DATE_TRUNC
- AGE, DATE_ADD, DATE_SUB
- TO_CHAR, TO_DATE, TO_TIMESTAMP

### Aggregate Functions
Location: `src/engine/functions/aggregate_functions.cpp`

- COUNT, SUM, AVG, MIN, MAX
- STRING_AGG, ARRAY_AGG
- STDDEV, VARIANCE
- PERCENTILE_CONT, PERCENTILE_DISC

## Handling Database-Specific Functions

For functions that only exist in specific databases:

```cpp
class DatabaseSpecificFunction {
    bool is_supported(ClientType client) {
        // Example: ARRAY functions only in PostgreSQL
        if (function_name.starts_with("ARRAY_")) {
            return client == POSTGRESQL;
        }
        return true;
    }
    
    Value execute_or_error(ClientType client, const vector<Value>& args) {
        if (!is_supported(client)) {
            throw unsupported_function_error(
                format("{} is not supported in {} mode", 
                       function_name, 
                       client_name(client))
            );
        }
        return execute(args);
    }
};
```

## Performance Considerations

1. **Avoid Copies**: Use move semantics for large strings
2. **Cache Results**: For deterministic functions
3. **Vectorize**: Process multiple rows at once
4. **JIT Compile**: For hot path functions

```cpp
class OptimizedFunction {
    // Cache for deterministic functions
    LRUCache<size_t, Value> result_cache{1000};
    
    Value execute_cached(const vector<Value>& args) {
        if (is_deterministic) {
            size_t hash = hash_args(args);
            if (auto cached = result_cache.get(hash)) {
                return *cached;
            }
            auto result = execute_impl(args);
            result_cache.put(hash, result);
            return result;
        }
        return execute_impl(args);
    }
};
```

## Testing Checklist

- [ ] Basic functionality works
- [ ] NULL handling correct
- [ ] Empty string handling
- [ ] UTF-8 support
- [ ] Large input handling
- [ ] All dialects supported
- [ ] Performance acceptable
- [ ] Memory leaks checked
- [ ] Thread safety verified
- [ ] Documentation complete

## Common Pitfalls

1. **Character Set Issues**: Always handle UTF-8 properly
2. **NULL Propagation**: Most functions return NULL on NULL input
3. **Type Coercion**: Handle implicit conversions
4. **Dialect Differences**: Test all supported databases
5. **Performance**: Profile with large datasets

## Example: Complete Implementation

Here's a complete example of adding the INITCAP function:

```cpp
// 1. Register
register_function({
    .name = "INITCAP",
    .category = FunctionCategory::STRING,
    .arg_types = {DataType::VARCHAR},
    .return_type = DataType::VARCHAR,
    .is_deterministic = true,
    .is_null_on_null = true,
    .implementation = initcap_impl
});

// 2. Implement
Value initcap_impl(const vector<Value>& args) {
    if (args[0].is_null()) return Value::null();
    
    string input = args[0].as_string();
    string output;
    bool next_upper = true;
    
    for (char c : input) {
        if (isalpha(c)) {
            output += next_upper ? toupper(c) : tolower(c);
            next_upper = false;
        } else {
            output += c;
            next_upper = true;
        }
    }
    
    return Value::from_string(output);
}

// 3. Test
TEST_F(FunctionTest, InitCap) {
    EXPECT_EQ(execute("SELECT INITCAP('hello world')").as_string(), 
              "Hello World");
    EXPECT_EQ(execute("SELECT INITCAP('SQL-DATABASE')").as_string(), 
              "Sql-Database");
}
```