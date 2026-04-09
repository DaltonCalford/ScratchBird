# Dependencies

## Primary code authorities

- include/scratchbird/core/connection_context.h
- src/core/connection_context.cpp
- include/scratchbird/sblr/executor.h
- src/sblr/executor.cpp
- src/catalog/schema_introspection.cpp

## Upstream specification dependencies

- section 08 for transaction semantics and always-in-transaction rules
- section 13 for operator.strict_mode ownership
- section 17 for trigger execution and row-context behavior
- section 21 for broader compatibility naming surfaces

## Dependency boundary rules

- section 16 documents exposure and lookup boundaries, not transaction-core semantics
- section 16 may reference operator.strict_mode through SHOW, but does not own coercion policy
- section 16 may reference row context as an internal runtime surface, but does not own public trigger or row-language syntax
- schema-introspection compatibility views may consume current schema state, but they do not expand section 16 into a universal catalog contract
