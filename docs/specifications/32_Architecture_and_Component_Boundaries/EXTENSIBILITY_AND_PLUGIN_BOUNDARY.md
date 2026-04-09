# Extensibility and Plugin Boundary

This file owns the architecture-level capability register for extensibility surfaces.

## Extensibility capability register

| Surface | Current state | Primary owning section | Architecture meaning | Fail-closed boundary |
| --- | --- | --- | --- | --- |
| auth/plugin attachment surfaces | current_bounded | 19 | real bounded extension attachment exists where security sections own it | not a universal plugin system |
| UDR/function/procedure runtime attachment | current_bounded | 17 | real bounded procedural/runtime extensibility exists where section 17 owns it | not a universal hot-load contract |
| parser family or dialect extension | current_bounded | 21, 28 | parser-layer extensibility exists only through the parser/dialect ownership surfaces | not an engine-core plugin guarantee |
| client/tooling integration points | current_bounded | 30 | external tooling can attach through bounded client/tool surfaces | not an engine-internal ABI guarantee |
| broad general extension marketplace or stable module ABI | fail_closed | none | no current canonical section proves this as a broad platform capability | remains explicitly unsupported at architecture level |

## Architecture rules

1. Extensibility must be named by attachment point and owning section.
2. Section 32 may summarize where extensibility attaches, but may not broaden ABI/API guarantees.
3. If no owning section proves a stable extension contract, section 32 must keep the surface fail-closed.

## Explicit non-guarantees

- no universal plugin marketplace
- no general hot-load module framework
- no architecture-level guarantee that every bounded extension surface shares one stable ABI contract
