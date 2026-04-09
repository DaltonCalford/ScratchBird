# Beta 2 Managed WASM WASI Extensibility Runtime Model

## Purpose

Define the managed extension runtime used to host safe compiled modules for
functions, procedures, analytical kernels, and controlled background helpers.

## Governing rules

1. Managed modules execute inside an admitted `WASM/WASI` host, not as
   unrestricted native code.
2. Capability declarations are explicit and cataloged.
3. No module receives raw filesystem, network, or process-spawn access unless a
   capability policy explicitly grants it.
4. Module publication is versioned and reversible.
5. Managed runtime state is never parser-private.

## Canonical metadata

- `sb_managed_module`
  - `module_uuid`
  - `module_name`
  - `module_version`
  - `binary_locator`
  - `abi_family`
  - `status`
- `sb_managed_capability_policy`
  - `policy_uuid`
  - `module_uuid`
  - `allowed_imports`
  - `memory_limit_bytes`
  - `cpu_budget`
  - `filesystem_scope`
  - `connector_scope`
  - `status`
- `sb_managed_binding`
  - `binding_uuid`
  - `module_uuid`
  - `binding_kind`
  - `sql_name`
  - `entry_symbol`
  - `arg_signature`
  - `result_signature`

## Runtime flow

1. Module binary is verified and published.
2. Capability policy is resolved.
3. SQL or internal binding is created.
4. Runtime instantiates the module with bounded memory and imports.
5. Invocation occurs through canonical marshalling.
6. Result, refusal, and metric data are published.

## Admitted binding kinds

- scalar function
- table function
- procedure
- aggregate helper
- background helper

## Required refusal rules

- `MANAGED_MODULE_ABI_UNSUPPORTED`
- `MANAGED_CAPABILITY_REFUSED`
- `MANAGED_MEMORY_LIMIT_EXCEEDED`
- `MANAGED_IMPORT_NOT_ALLOWED`
- `MANAGED_INSTANCE_VERIFICATION_FAILED`

## Metrics

- module load count
- instantiation time
- execution time
- trapped execution count
- memory high-water mark
- refused capability count

## Example

```sql
create managed module risk_math version '1.0.0';
grant managed capability memory_limit_bytes = 134217728 to module risk_math;
create function risk_black_scholes(...)
external managed module risk_math entry 'black_scholes';
```

## Cross-section requirements

- section `17` owns binding and package contract
- section `23` owns executor marshalling and runtime host integration
- section `19` owns capability policy and signing rules
