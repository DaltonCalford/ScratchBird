# Test Contract

## Direct audited proof artifacts

- `tests/unit/test_query_compiler_v3.cpp`
  - `QueryCompilerV3Test.ExecuteContextFunctionsRuntimeClosed`
  - `QueryCompilerV3Test.ExecuteBareContextKeywordsRuntimeClosed`
  - `QueryCompilerV3Test.ExecuteSetSchemaShorthandUpdatesSchemaContext`

## Required certification lanes

1. Connection-context state
- prove current user and session user state are retrievable through the owned runtime surfaces
- prove current schema and search path are stored and exposed consistently
- prove generic session-variable set, get, clear, clear-all, and list behavior

2. SHOW inventory
- prove each directly claimed SHOW name resolves correctly
- prove generic session-variable fallback works only when the name exists in session state
- prove unknown SHOW names fail closed

3. Current schema and search-path normalization
- prove emulated-dialect schema and search-path normalization remains bounded to the implemented runtime paths
- prove schema-introspection compatibility surfaces continue to consume current schema correctly

4. Transaction and statement exposure boundary
- prove SHOW transaction_isolation, SHOW statement_timeout, and SHOW autocommit remain bounded exposure surfaces
- prove section 16 does not silently widen into a typed transaction-variable registry

5. Row-context negative paths
- prove row-context-dependent execution fails when row context is unavailable
- prove no certification lane assumes public ROW.NEW or ROW.OLD syntax without direct implementation proof
