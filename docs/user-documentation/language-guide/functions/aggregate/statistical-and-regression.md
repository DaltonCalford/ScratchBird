# Aggregate Functions: Statistical And Regression
Last modified: 2026-02-19

Back links:
- [Aggregate README](README.md)
- [Functions README](../README.md)

Series navigation:
- Previous: [Core Aggregates](core-aggregates.md)

Emitter-mapped statistical/regr families:
- `STDDEV`, `STDDEV_SAMP`, `STDDEV_POP`
- `VARIANCE`, `VAR_SAMP`, `VAR_POP`
- `CORR`, `COVAR_POP`
- `REGR_SLOPE`, `REGR_INTERCEPT`, `REGR_COUNT`, `REGR_R2`, `REGR_AVGX`, `REGR_AVGY`, `REGR_SXX`, `REGR_SYY`, `REGR_SXY`

Coverage note:
- function name parsing and opcode mapping are present
- execution closure depends on evaluator/runtime implementation depth per function
