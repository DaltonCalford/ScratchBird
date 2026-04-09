# Configuration Defaults

## Current default sources

Default values currently come from code constants, struct initializers, and
local subsystem defaults. They are not generated from one machine-readable
registry.

For promoted settings, those defaults are no longer only transient process
constants:
- on first mount they seed the scalar configuration catalog or the dedicated
  listener-topology tables
- after mount they remain registry-default fallbacks when no explicit override
  row exists

## Core Config defaults currently documented in code

Representative proved defaults include:
- buffer pool size: 128 pages
- heap scan start page: 7
- base schemas: 8
- max backends: 100
- max locks: 10000
- transaction cache size: 10000
- deadlock timeout: 1000 ms
- lock timeout: 60 s
- dormant transaction lease: 3600 s
- initial xid: 100
- header update frequency: 100
- sweep interval: 20000
- max version chain length: 100
- hash bucket fill threshold: 90
- b-tree merge threshold: 80
- toast compression threshold: 256 bytes
- page compression threshold: 0.5

## Service defaults currently documented in code

Representative proved defaults include:
- service mode: multi-database
- front-door mode: direct
- manager proxy bind: 0.0.0.0:3090
- manager internal native bind: 127.0.0.1:3392
- manager owner database: main
- listener id: 1
- data directory: /var/lib/scratchbird
- pid file: /var/run/scratchbird/sb_server.pid
- log file: /var/log/scratchbird/sb_server.log
- bind address: 0.0.0.0
- control socket dir: /var/run/scratchbird
- spawn strategy: hybrid
- max connections: 100
- idle timeout: 3600 s
- shared buffers: 128 MB
- work_mem: 4 MB
- shutdown timeout: 30 s
- log level: INFO
- statistics: enabled

## Listener defaults currently documented in code

Representative proved defaults include:
- listener mode: direct
- database owner: main
- bind address: 0.0.0.0
- log level: info
- parser pool: 4 through 64
- spawn strategy: hybrid
- max requests: 0
- max age: 0
- health check interval: 5000 ms
- DBBT clock skew: 2000 ms
- DBBT replay cache size: 4096

## Boundary

These defaults are the current implementation baseline only. This section does not claim:
- one generated default registry
- one exhaustive cross-subsystem default inventory
- that bootstrap constants remain the only durable truth after promoted
  configuration has been seeded into catalog rows
