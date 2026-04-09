# Normative UDR Remote Engine Connector Checklist

## Capability matrix
- generic connector procedure metadata lookup: `supported`
- generic connector procedure listing: `supported`
- engine-specific procedure metadata lookup: `supported`
- engine-specific procedure listing: `supported`
- remote connector catalog rows: `supported`
- remote connector capability rows: `supported`
- remote connector state machine: `supported`
- unified remote execution contract: `unproven`
- remote incident vocabulary: `unproven`
- remote transaction orchestration: `unproven`

## Current code-backed truth
Remote-engine connector support is partially real.

Audited connector anchors:
- `udr_connector.cpp:624`
- `udr_connector.cpp:652`
- `firebird_udr.cpp:1596`
- `firebird_udr.cpp:1670`
- `postgresql_udr.cpp:1733`
- `postgresql_udr.cpp:1833`
- `mysql_udr.cpp:1564`
- `mysql_udr.cpp:1628`
- `scratchbird_udr.cpp:1656`
- `scratchbird_udr.cpp:1726`

Remote connector catalog anchors:
- `catalog_manager.h:1249`
- `catalog_manager.h:1347`
- `catalog_manager.h:9384`
- `catalog_manager.cpp:85356`
- `catalog_manager.cpp:85676`

## Main fail-closed rule
Treat remote-engine connector support as a partial connector and catalog surface. Do not describe it as a closed remote procedure or remote function runtime platform without a later code-backed audit.
