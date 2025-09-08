# ScratchBird Coding Standards

## 1. Naming Conventions

- **Classes and Structs:** `PascalCase`
- **Functions and Methods:** `camelCase`
- **Variables:** `snake_case`
- **Constants and Enums:** `UPPER_CASE_SNAKE_CASE`
- **Private Members:** `snake_case_` (with a trailing underscore)

## 2. Formatting and Style

- **Indentation:** 4 spaces.
- **Line Breaks:** Use Unix-style line endings (LF).
- **Comments:** Use `//` for single-line comments and `/* */` for multi-line comments. Doxygen-style comments are encouraged for public APIs.

## 3. Error Handling

The project uses a `Status` and `ErrorContext` based error handling mechanism. Exceptions should not be used for control flow.

- All functions that can fail should return a `Status` enum.
- For functions that need to return a value, the value should be returned via an output parameter.
- The `ErrorContext` struct should be used to provide detailed error information, including the file, line number, and a descriptive error message.
- The `SET_ERROR_CONTEXT` macro should be used to set the error context.

## 4. Resource Management

- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`) to manage dynamic memory whenever possible.
- If manual memory management is necessary, use `new(std::nothrow)` and check for `nullptr` to handle allocation failures.
- All resources (memory, file handles, etc.) must be properly released in all code paths, including error paths.

## 5. C++ Best Practices

- Use modern C++ features (C++17) where appropriate.
- Prefer `enum class` over `enum`.
- Use `const` and `constexpr` where possible.
- Avoid raw pointers when ownership is involved. Use smart pointers instead.
- Write small, focused functions and classes.
- Keep the code clean, readable, and maintainable.
