# Test Contract

## Direct audited proof artifacts

- `tests/unit/test_executor.cpp`
  - `ExecutorTest.TemporalArithmeticWithImplicitTextCoercion`
  - `ExecutorTest.ProcedureCallRejectsSignatureMismatchDeterministically`
  - `ExecutorTest.ProcedureCallAllowsDeterministicNumericWidening`
  - `ExecutorTest.ProcedureCallRejectsArgumentCountMismatchDeterministically`
- `tests/unit/test_query_compiler_v3.cpp`
  - `QueryCompilerV3Test.ExecuteCanonicalMathFunctionsInvalidDomainReturnsNull`

## Required certification lanes

1. Explicit cast success cases
- representative scalar casts must succeed through `TypedValue::convertTo(...)`
- target precision, scale, and timezone metadata must be honored where applicable

2. Explicit cast fail-closed strict cases
- `TIMESTAMP_NS -> TIMESTAMP` with sub-microsecond remainder must fail in strict mode
- `DECIMAL256 -> integer` with fractional remainder must fail in strict mode
- `INT256 -> UINT256` must fail regardless of permissive lossy-rounding branches

3. Process coercion-context coverage
- `types.coercion_context=STRICT` and `types.coercion_context=PERMISSIVE` must both be exercised
- conversions that change behavior across those modes must be asserted explicitly

4. Session `operator.strict_mode` coverage
- `SET operator.strict_mode ON|OFF|TRUE|FALSE|1|0` must be accepted
- `SET LOCAL operator.strict_mode ...` must be rejected
- reset or null-setting path must clear the session override
- session settings introspection must expose `operator.strict_mode`

5. Write-path coercion coverage
- `INSERT` and `UPDATE` style paths must prove that write coercion uses catalog column metadata
- mismatch failures must include the column name and preserve the conversion error detail

6. Array write coercion coverage
- array input and JSON array text input must be tested
- invalid JSON array text must fail closed
- non-array JSON input must fail closed for array targets

7. Unsupported DDL refusal coverage
- no certification lane may assume catalog-backed `CREATE CAST`, `DROP CAST`, `CREATE OPERATOR`, or `DROP OPERATOR`
- if such syntax is encountered in any compatibility surface, the implementation must refuse it or keep it outside canonical shipped authority

## Beta 2 required proof additions

1. String round-trip coverage
- every Beta 2 datatype from sections `14` and `15` must prove `CAST(value AS TEXT)` and `CAST(text AS type)` round-trip where the type defines a reversible text form

2. Selector coverage
- every selector family listed in `BETA2_CAST_STRING_AND_SUBFIELD_ACCESS_MODEL.md` must prove success and fail-closed behavior

3. Strict-mode refusal coverage
- lossy temporal downcasts, malformed jsonpath input, malformed hex input, and invalid vector or multirange text must fail under strict coercion
