# FAQ

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-09

## What is ScratchBird?
ScratchBird is a database engine built on a Firebird-style Multi-Generational
Architecture (MGA) with multiple wire protocol listeners.

## Which ports are used?
- Native: 3092
- PostgreSQL: 5432
- MySQL: 3306
- Firebird: 3050

## Do emulated databases create physical files?
No. Emulated databases are metadata-only schemas; only ScratchBird databases
use on-disk files.

## Is a write-ahead log required?
No. MGA provides recovery without a write-ahead log. A write-after log may be
used later for replication/PITR.

## Is cluster support available?
Cluster features are deferred to Beta; specs exist but runtime support is not
in Alpha.

## Where is the authoritative spec?
See `docs/specifications/README.md` and the Developers Guide:
- [Developers Guide](developer-guide/README.md)
