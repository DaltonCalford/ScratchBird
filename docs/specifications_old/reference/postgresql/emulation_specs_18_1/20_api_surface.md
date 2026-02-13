# PostgreSQL 18.1 API Surface — Draft 1

## 1. FE/BE Protocol Messages
- Message formats and sequence rules are defined in `doc/src/sgml/protocol.sgml` and implemented in `src/backend/libpq/*`.

## 2. Authentication
- Auth methods (MD5, SCRAM-SHA-256, etc.) are defined by protocol docs and implemented in `src/backend/libpq/auth.c` and related files.

## 3. Query Modes
- Simple Query protocol and Extended Query protocol are defined by protocol docs.

## 4. Copy
- COPY protocol is defined in protocol docs and implemented in `src/backend/commands/copy.c` and libpq layer.

## 5. Compliance Rule
All behavior must match the source copies.
