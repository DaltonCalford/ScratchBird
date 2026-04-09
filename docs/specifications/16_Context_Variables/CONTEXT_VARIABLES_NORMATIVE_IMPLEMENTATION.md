# Context Variables Normative Implementation

## Connection-context authority

Current connection-context authority includes:
- current user identity
- session user identity
- current schema id and current schema name
- search path
- dialect tag
- generic session-variable mutation and lookup

The generic session-variable contract is currently:
- set by normalized name
- get by normalized name
- clear one name
- clear all names
- list current stored pairs

This is a generic key-value session store. It is not a typed registry with per-variable metadata.

## Current schema and search-path authority

Current schema and search path are part of the connection context.

Current proved behavior includes:
- current schema is stored in connection context
- search path is stored in connection context
- executor SHOW surfaces render current schema and search path from connection context
- schema-introspection compatibility views rely on current schema state
- emulated dialect flows may normalize current schema and search-path state before storing or exposing it

## Bounded SHOW inventory

The current canonical SHOW inventory includes the directly proved names below:
- search_path
- schema
- current_schema
- schema_path
- dialect_tag
- sql_dialect
- charset
- parser
- names
- statement_timeout
- autocommit
- time_zone and timezone
- transaction_isolation
- server_version
- operator.strict_mode

Current bounded fallback rule:
- if a SHOW name is not one of the direct built-ins but exists in generic session-variable storage, the executor may expose the stored session value

Current fail-closed rule:
- unknown SHOW names must be rejected explicitly

## Transaction and statement exposure boundary

Section 16 only claims the current operator-visible exposure of transaction and statement state. Examples include:
- SHOW transaction_isolation
- SHOW statement_timeout
- SHOW autocommit

Section 16 does not claim:
- a typed transaction-variable lookup inventory
- a typed statement-variable lookup inventory
- one universal transaction or statement variable namespace

## Row and trigger context boundary

Current row and trigger context are real executor runtime surfaces.

Current proved boundary:
- executor paths can run with row context and trigger context
- row-context-dependent behavior fails when that context is unavailable
- this internal runtime truth does not by itself create a public ROW.NEW or ROW.OLD language contract

## Explicit non-guarantees

This section does not currently guarantee:
- one typed context-variable catalog
- one namespace-id registry
- one public row-variable syntax surface
- one comprehensive alias matrix beyond the directly proved SHOW names
