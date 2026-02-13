# Component Model Diagrams

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Source:** `/docs/specifications/parser/v3/COMPONENT_MODEL_AND_RESPONSIBILITIES.md`  
**Scope:** Engine, Parser, Network Listener topology

## Mermaid

```mermaid
flowchart LR
  Clients[Clients] --> Listener[Network Listener]
  Listener -->|socket handoff| Parser[Parser (per connection)]
  Parser -->|IPC or embedded| Engine[Engine Core]
  Engine --> Catalog[(Catalog/Metadata)]
  Engine --> Storage[(Storage/Transactions)]
  Engine --> Cache[(Shared SBLR Cache)]
  Parser --> SessionCache[(Per-session Cache)]
```

## ASCII

```
Clients
  |
  v
Network Listener --(socket handoff)--> Parser (per connection)
                                          |
                                          v
                                    Engine Core
                                      |   |   |
                                      |   |   +--> Shared SBLR Cache
                                      |   +------> Catalog/Metadata
                                      +----------> Storage/Transactions
```
