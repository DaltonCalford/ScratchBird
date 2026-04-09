# Test Contract

## Direct audited test authorities
- `tests/unit/test_parser_v3_udr_compile_emitter_contract.cpp`
- `tests/unit/test_sblr_vnext_executor_dispatch_contract.cpp`
- `tests/unit/test_udr_connector_factory.cpp`
- `tests/unit/test_catalog_remote_connector_extension_contract.cpp`
- `tests/unit/test_catalog_cluster_fabric_extension_contract.cpp`
- `tests/unit/test_catalog_engine_specific_extension_contract.cpp`
- `tests/unit/test_executor.cpp`
- `tests/unit/test_query_compiler_v3.cpp`
- `tests/unit/test_code_dependencies.cpp`

## Lane-A proof anchors
- `tests/unit/test_executor.cpp`
  - `ExecutorTest.ProcedureCallRejectsSignatureMismatchDeterministically`
  - `ExecutorTest.ProcedureCallAllowsDeterministicNumericWidening`
  - `ExecutorTest.ProcedureCallRejectsArgumentCountMismatchDeterministically`
- `tests/unit/test_query_compiler_v3.cpp`
  - `QueryCompilerV3Test.ExecuteUnqualifiedRoutinesWithTriggerAndProcedureFromCurrentSchema`
  - `QueryCompilerV3Test.CommitAfterCreateProcedureKeepsTransactionAndPersistsRoutine`
- `tests/unit/test_catalog_remote_connector_extension_contract.cpp`
  - `CatalogRemoteConnectorExtensionContractTest.RemoteConnectorExtensionCatalogContracts`
- `tests/unit/test_catalog_cluster_fabric_extension_contract.cpp`
  - `CatalogClusterFabricExtensionContractTest.ClusterFabricCatalogContracts`
- `tests/unit/test_catalog_engine_specific_extension_contract.cpp`
  - `CatalogEngineSpecificExtensionContractTest.EngineSpecificCatalogContracts`

## What the current test surface proves
- UDR compile parser or emitter or dispatcher surfaces exist
- remote connector factory and procedure-introspection surfaces exist
- remote connector catalog extension rows and state transitions exist
- cluster-fabric catalog extension rows exist
- blob-filter catalog extension rows exist
- stored routine execution and dependency surfaces exist

## What the current test surface does not prove
- generic runtime blob-filter invocation
- a closed operator-facing remote-engine execution contract
- live cluster-fabric runtime dispatch or transport semantics
- exhaustive overloaded routine-dispatch or signature-resolution behavior across all edge cases

## Section test rule
Section `17` must stay fail-closed around the currently audited tests. Checklist material without direct runtime or extension-test proof remains bounded or unproven.

## Beta 2 scientific and analytical UDR proof obligations

Before the Beta 2 package family in this section can be promoted from
canonical target-state to implemented truth, the test corpus must add:

- numeric array constructor, reshape, broadcast, reduction, and matrix
  operation tests
- compiled expression normalization, caching, and determinism tests
- Arrow or Parquet import/export roundtrip tests including schema and null
  fidelity
- scientific and statistical routine tests with stable fixture answers
- labeled N-D alignment, coordinate selection, and dimension-reduction tests
- symbolic parse, simplify, differentiate, and codegen verification tests
- bounded solver admission, timeout, convergence, and metrics tests
- machine-learning preprocessing, model artifact, and inference tests
- Bayesian model admission, bounded inference, posterior-summary, and
  reproducibility tests
- quota, memory-pressure, and spill-behavior tests for vectorized analytical
  UDRs
- finance pricing, risk, curve, and portfolio optimization fixture tests
- unit-conversion, dimension-mismatch, uncertainty-propagation, and exact-math
  precision tests
- differential-equation solve, event, and stochastic-seed reproducibility tests
- graph construction, path, centrality, and flow fixture tests
- probability distribution, fitting, and seeded sampling tests
- autodiff gradient, jacobian, hessian, and fail-closed admissibility tests
- astronomy, chemistry, and education overlay tests for each admitted vertical
- geospatial CRS, predicate, transform, and metric fixture tests
- text normalization, tokenization, entity extraction, and model-version tests
- document type-detection, metadata extraction, and bounded-parser admission
  tests
- business/fiscal/educational calendar determinism and holiday-rule tests
- contract validation, compatibility-diff, and contract-version tests
- quality expectation-suite, batch-validation, and drift-check tests
- rules/policy evaluation, trace, and version-determinism tests
- FHIR resource validation, profile-check, and rowset-projection tests
- DICOM detection, tag extraction, and de-identification preview tests
- entity matching, clustering, survivorship, and explanation tests
- delimited text-file read, append, overwrite, schema-inference, and
  atomic-replace tests
- ODBC connector capability-discovery, metadata, prepared-exec, CRUD, batch,
  generated-key, timeout/cancel, and SQLSTATE/native-error mapping tests
- managed `WASM/WASI` module verification, capability refusal, bounded-memory,
  and binding-marshalling tests

No package in the Beta 2 analytical program may be documented as implemented
until the relevant proof lane exists in the test tree.
