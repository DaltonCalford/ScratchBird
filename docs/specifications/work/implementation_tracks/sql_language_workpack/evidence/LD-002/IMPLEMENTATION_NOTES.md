# Implementation Notes

- Scope executed: grammar production registration.
- Registration basis: canonical section-21 feature keys.
- Family assignment is deterministic and prefix-based where file-level family is not sufficient.
- Artifact `GRAMMAR_PRODUCTION_INDEX.csv` is sorted by `feature_key` and uses stable `GP-###` registration IDs.
