# Error and Diagnostics Reference

## Coverage and Evidence Status

Status: Partial (source and test anchors are present for error mapping and runtime diagnostics contracts).

- Source anchor: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/vnext_error_codes.h:175
- Source anchor: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/error_context.h:17
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/ipc/ipc_error_mapper.cpp:97
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/ipc/ipc_error_mapper.cpp:166
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_engine_error_code_harmonization.cpp:44
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_error_paths.cpp:21
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_state_v3.cpp:16
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_query_compiler_v3.cpp:124
- Run anchor: /home/dcalford/CliWork/local_work/artifacts/docs_refresh/20260227T172440Z/LINK_CHECK.txt
- Why partial: the index page is complete as a navigation surface; detailed parser/runtime diagnostic semantics are documented in subsystem pages and test contracts.


[Documentation Workspace README](../README.md)

Reference for error contracts, diagnostics surfaces, and incident triage flows.

## Scope and Navigation

- [Scope and Navigation](01_scope_and_navigation.md)

## Topic Areas

- [Error Model and Contracts](error_model_and_contracts/README.md)
- [Parser Errors](parser_errors/README.md)
- [Runtime and Execution Errors](runtime_and_execution_errors/README.md)
- [Transaction and Concurrency Errors](transaction_and_concurrency_errors/README.md)
- [Security and Authentication Errors](security_and_auth_errors/README.md)
- [Cluster and Replication Errors](cluster_and_replication_errors/README.md)
- [Tooling and CLI Diagnostics](tooling_and_cli_diagnostics/README.md)
- [Observability and Forensics](observability_and_forensics/README.md)
