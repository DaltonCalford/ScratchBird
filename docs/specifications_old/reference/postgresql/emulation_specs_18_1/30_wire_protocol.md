# PostgreSQL 18.1 Wire Protocol — Draft 1

## Authoritative Sources
- `doc/src/sgml/protocol.sgml`
- `src/backend/libpq/*`

## 1. Startup and Authentication
- Startup packet, SSLRequest, CancelRequest, and auth flows are defined in `protocol.sgml`.

## 2. Simple Query and Extended Query
- Full message sequences and formats defined by protocol docs and libpq implementation.

## 3. Copy
- COPY IN/OUT/BOTH message sequences and data formats defined by protocol docs.

## 4. Compliance Rule
All fields and edge cases must match the source copies.
