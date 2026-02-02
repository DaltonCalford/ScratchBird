# i18n Conformance Report
Status: Superseded (implementation verified)
Last Updated: 2026-02-02

Note: All gaps called out here are closed. Track any remaining work in
`docs/planning/TRACKER_OUTSTANDING_MASTER.md`.


## Firebird

- Charsets: 37
- Collations: 5

Missing mapping tables:
- ASCII
- NONE
- OCTETS

Collations referencing unknown charset: none

## MySQL

- Charsets: 50
- Collations: 95

Missing mapping tables:
- ARMSCII8
- ASCII
- DEC8
- EUC-CN
- GEOSTD8
- HP8
- KEYBCS2
- OCTETS
- SWE7
- binary
- eucjpms
- greek
- hebrew
- latin1
- latin2
- latin5
- latin7
- sjis
- ujis

Collations referencing unknown charset: none

## PostgreSQL

- Charsets: 37
- Collations: 28

Missing mapping tables:
- ASCII
- EUC-CN
- EUC-TW
- EUC_JIS_2004
- MULE_INTERNAL
- Windows-874

Collations referencing unknown charset: none
