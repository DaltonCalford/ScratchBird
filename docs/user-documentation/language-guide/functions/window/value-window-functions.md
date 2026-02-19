# Window Functions: Value Functions And Partials
Last modified: 2026-02-19

Back links:
- [Window README](README.md)
- [Functions README](../README.md)

Series navigation:
- Previous: [Ranking Window Functions](ranking.md)

Parsed window names include:
- `LAG`
- `LEAD`
- `FIRST_VALUE`
- `LAST_VALUE`
- `NTH_VALUE`

Current 0.1.0 limitation:
- these parsed names are emitted through fallback window opcode behavior rather than dedicated per-function opcodes
- this keeps value-window coverage partial until dedicated opcode/emitter/runtime closure is completed
