# Section 31 Dependencies

Section 31 depends on the current canonical behavior defined in:

- section `05` through `10` for storage, transaction, lock, and sweep behavior under test
- section `22`, `23`, and `28` for SBLR, compiler, executor, and parser certification surfaces
- section `26`, `29`, and `30` for protocol, listener, and client-tooling certification surfaces
- section `35` for MGA durability and restart semantics
- section `37` for metadata and DDL publication semantics
- section `41` and `42` for platform and fault-model boundaries used by certification scope

Section 31 consumes the current canonical behavior of those sections and turns it into gate and evidence obligations. It does not redefine engine semantics.
