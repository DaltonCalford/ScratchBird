# CLI Guide

## Coverage and Evidence Status

Status: Deferred to next pass (no current implementation proof in this revision).

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/network/sb_listener_main.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_listener_ipc_adapter.cpp:1
- Run anchor: /home/dcalford/CliWork/local_work/artifacts/docs_refresh/20260227T172322Z/LINK_CHECK.txt
- Why deferred: this section is a navigation or index page; operational content is documented in child statement/system pages and is staged for a later pass.


[Documentation Workspace README](../README.md)

This guide is a technical reference manual set.
Each CLI has its own directory with task-sized reference documents.

## CLI Manual Directories

- [sb_server](sb_server/README.md)
- [sb_isql](sb_isql/README.md)
- [sb_pg_isql](sb_pg_isql/README.md)
- [sb_my_isql](sb_my_isql/README.md)
- [sb_fb_isql](sb_fb_isql/README.md)
- [sb_listener_native](sb_listener_native/README.md)
- [sb_listener_pg](sb_listener_pg/README.md)
- [sb_listener_mysql](sb_listener_mysql/README.md)
- [sb_listener_fb](sb_listener_fb/README.md)
- [sb_parser_native](sb_parser_native/README.md)
- [sb_parser_pg](sb_parser_pg/README.md)
- [sb_parser_mysql](sb_parser_mysql/README.md)
- [sb_parser_fb](sb_parser_fb/README.md)

## Authoring Rules

Every CLI directory should document:

- canonical command identity and role
- complete invocation syntax and option reference
- environment variables and config precedence
- runtime flow/modes and ownership boundaries
- exit code and error contracts
- operator examples and troubleshooting flow
