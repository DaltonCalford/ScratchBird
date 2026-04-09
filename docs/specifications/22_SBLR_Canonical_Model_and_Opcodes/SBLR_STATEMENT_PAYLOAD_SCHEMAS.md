# SBLR Statement Payload Schemas

Status: current_authority

## General schema rules

Each statement payload must encode enough normalized information for the shared compiler front door to bind, validate, and plan the statement without parser-family guessing.

Every statement payload must carry:

- canonical statement kind
- statement-local flags
- resolved durable object identity where required
- expression tree references or inline expression payloads using canonical opcode rules
- domain references for typed values and coercions
- source-position or diagnostics anchors when needed for stable error reporting

## Name and UUID rules

- Database objects that participate in durable execution semantics must be UUID-bound before final payload emission.
- Session-local names, aliases, and correlation names may remain symbolic when they are local to the statement and not durable catalog identity.
- Client echo text is not execution authority.

## Transaction and DDL rule

Because ScratchBird is always in a transaction, statement payloads for both DDL and DML must be emitted with the same transactional assumptions:

- they execute inside an existing transaction context
- commit publication and rollback retirement occur outside the payload through section 08 rules
- autocommit does not change payload semantics; it changes only post-success commit behavior
