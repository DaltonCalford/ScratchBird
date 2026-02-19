# Admin REDIS BRIDGE COMMANDS: Syntax
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Topic README](README.md)
- Next: [Semantics](semantics.md)

## Coverage
- Status: Partial

## Form
~~~sql
REDIS STRING ...; REDIS HASH ...; REDIS LIST ...; REDIS SET ...; REDIS ZSET ...; REDIS STREAM ...; REDIS PUBSUB ...;
EVAL LUA SCRIPT <script_text> KEYS <key_list> ARGS <arg_list>;
REDIS LUA EVAL SCRIPT <script_text> KEYS <key_list> ARGS <arg_list>;
XGROUP CREATE STREAM <stream_name> GROUP <group_name>;
XREADGROUP STREAM <stream_name> GROUP <group_name> CONSUMER <consumer_name> [COUNT <n>] [BLOCK <ms>];
XCLAIM STREAM <stream_name> GROUP <group_name> CONSUMER <consumer_name> MINIDLE <ms> IDS <id_list>;
~~~

## Notes
- Details: Redis family bridge forms are explicit parser/emitter surfaces.
- Runtime note: Runtime is bridge-partial in 0.1.0.
- Error/contract note: Unsupported semantic variants return deterministic bridge errors.
- Usage rationale: Low-latency key-value and stream operations from SQL entrypoint.
