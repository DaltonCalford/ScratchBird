# Test Contract

## Current directly evidenced package-02 proof

- `ShowSetCommandsTest.ConfigCommandsCompileToAlterSystemOpcode`
- `ShowSetCommandsTest.ShowConfigAndHistoryUseCatalogManagedValues`
- `ShowSetCommandsTest.DedicatedListenerTopologyKeysRejectAlterSystemSet`
- `JobSchedulerRuntimeSql.AlterSystemAppliesSchedulerConfig`
- `ExecutorTransactionPayloadTest.AlterSystemAppliesDormantPolicyAndRunsMaintenance`
- `CatalogDatabaseBootstrapTest.CreatesCanonicalFixedSchemaTree`

## Required certification lanes

1. Bootstrap precedence and seeding
- prove command-line overrides environment
- prove environment overrides file
- prove file overrides defaults
- prove first-mount seeding of missing scalar catalog rows and dedicated
  listener-topology rows from bootstrap inputs

2. Core Config and ConfigParser bootstrap substrate
- prove simple section and key lookup
- prove string, integer, unsigned, boolean, and double retrieval
- prove missing values fall back to defaults
- prove include handling
- prove environment expansion
- prove typed size and duration parsing
- prove strict parse failure paths remain deterministic

3. Service and listener bootstrap mapping
- prove implemented sections map into service and listener runtime structures correctly

4. Catalog-backed scalar management
- prove `ALTER SYSTEM SET` writes durable scalar configuration state rather
  than only transient process-local state
- prove `ALTER SYSTEM RESET` retires a durable override and republishes the
  inherited or default value
- prove `SHOW CONFIG` and `CONFIG HISTORY` expose effective value, source, and
  committed generation information
- prove generic scalar mutation refuses keys that belong to the dedicated
  listener-topology families

5. Reload and reconcile behavior
- prove reload re-reads bootstrap sources
- prove only the implemented live subset applies directly
- prove committed catalog-managed scalar rows are not silently overwritten by
  reload
- prove dedicated listener-topology rows require explicit durable mutation or
  reconcile handling rather than blind hot-apply

6. Cluster-config epoch and generation boundary
- prove cluster identity and cluster_config_epoch publication
- prove target-local configuration generation publication for committed
  catalog-managed changes
- prove mismatch handling distinguishes epoch mismatch from local-generation
  drift or refusal

## Explicit non-certification

This section does not require full distributed consensus for every management
action.
