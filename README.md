# ScratchBird Database Engine

## Status

For the current status of the project, please see the [STATUS.md](STATUS.md) file.

## Project Structure

- **`docs/`** - Documentation, including design documents, specifications, and reference materials.
  - `design/` - High-level design and architecture documents.
  - `specifications/` - Detailed technical specifications for features.
  - `reference/` - External reference materials and examples.
- **`project/`** - Project management files.
  - `plan/` - The authoritative implementation plan.
  - `progress/` - Progress logs and status reports.
  - `reviews/` - Code reviews.
  - `tests/` - Test reports.
- **`src/`** - Source code
  - `core/` - Database engine
  - `parser/` - SQL parser
  - `sblr/` - Bytecode system
- **`tests/`** - Test suites
  - `unit/` - Unit tests
  - `integration/` - Integration tests

## Development Process

1. Review the `AUTHORITATIVE_IMPLEMENTATION_PLAN.md` in the `project/plan` directory.
2. Begin implementation of the next Alpha phase.
3. Progress is tracked in the `project/progress` directory.
4. Tests are created alongside implementation.

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Testing

```bash
# Run all tests
ctest --output-on-failure

# Run specific tests
ctest -R "Alpha101"
```

## License

See LICENSE file for details.
