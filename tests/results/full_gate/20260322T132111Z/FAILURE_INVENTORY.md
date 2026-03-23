# Full Gate Failure Inventory

Run bundle:
- `configure.log`
- `build.log`
- `ctest.log`

Status:
- clean configure: passed
- clean build: passed
- full `ctest`: interrupted after the final tail gate blocked on a hung PostgreSQL compatibility precheck under `ConformancePublicBetaRequiredGate`

Known failing tests captured before the blocked tail:
- `JobSchedulerGovernance.ProcedureRunRespectsWorkloadAdmissionPolicy`
- `CatalogRenameMoveTest.RenameFunctionUpdatesResolverAndPersists`
- `CatalogRenameMoveTest.MoveFunctionUpdatesResolver`
- `CatalogRenameMoveTest.RenameProcedureUpdatesResolver`
- `CatalogRenameMoveTest.MoveProcedureUpdatesResolver`
- `CatalogVirtualOverlayConformanceContractTest.VirtualOverlayConformance`
- `StoredCodeDependencyTest.CreateFunctionRegistersDependencies`
- `StoredCodeDependencyTest.CreateProcedureRegistersDependencies`
- `StoredCodeDependencyTest.DropFunctionFailsIfCalledByAnotherFunction`
- `StoredCodeDependencyTest.DropFunctionSucceedsAfterDroppingDependent`
- `StoredCodeDependencyTest.DropProcedureFailsIfCalled`
- `StoredCodeDependencyTest.DropTableBlockedByFunctionDependency`
- `StoredCodeDependencyTest.DropFunctionClearsDependencies`
- `StoredCodeDependencyTest.ComplexFunctionChain`
- `StoredCodeDependencyTest.MultipleFunctionsSameTable`
- `StoredCodeDependencyTest.MixedFunctionProcedureDependencies`
- `StoredCodeDependencyTest.DropPackageFailsIfDependentExists`
- `StoredCodeDependencyTest.DropUDRFailsIfDependentExists`
- `DependencyErrorTest.ErrorMessageFormatIncludesDetailAndHint`
- `ExecutorTest.TriggerBeforeInsertMissingProcedureFailsClosed`
- `ExecutorTest.TriggerAfterInsertMissingProcedureFailsClosed`
- `ObjectResolverViewTest.ViewMatchesResolverCache`
- `ProtocolAdapterDialectsFirebird.FirebirdSessionSchemaContextUsesEmulatedDatabaseRoot`
- `TypeDependencyTest.DropExceptionFailsIfProcedureReferences`
- `VirtualCatalogOverlayGroupAContractTest.GroupAHandlersRegisteredAndQueryable`
- `BTreeProofCorpusBenchmark.RestartAnchorAndCompactionMaintenanceCorpus`
- `QueryPlannerIntegrationTest.AdaptiveFeedbackCachedPlanReflectsLatestFeedbackStateAfterRepeatExecution`
- `ConformanceV3NativeParserInet`
- `CompatibilityMySQL`
- `CompatibilityPostgreSQL`

Blocked tail gate:
- `ConformancePublicBetaRequiredGate`

Blocked-tail details:
- the gate stalled inside `tests/compatibility/postgresql/scripts/run_postgresql_ctest.sh`
- child process observed: donor `psql` precheck against `127.0.0.1:16432`
- no new output was written after `2026-03-22 10:07:07 -0400`
- latest fixture refresh recorded PostgreSQL listener startup failure followed by `post-bootstrap SQL failed`

High-level clustering:
- stored-code dependency / rename / resolver regressions
- runtime fail-closed trigger/dependency semantics
- Firebird emulation schema-context regression
- optimizer feedback recache regression
- v3 native inet security / deprecated-alias conformance regressions
- MySQL/PostgreSQL compatibility endpoint/auth failures
- public beta gate blocked by the PostgreSQL compatibility sub-lane hang
