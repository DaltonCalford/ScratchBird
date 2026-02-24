# Issues

- task_id: `ENG-MILVUS-001`
- gate: `ENG-MILVUS-GATE-01`
- blocker_class: `toolchain/environment`
- observed_behavior: `Milvus build failed in Go/CGo stage: pkg-config could not resolve milvus_core, rdkafka, rocksdb, milvus-storage, and jemalloc (plus missing libjemalloc.so warning in internal/core output)`
- expected_behavior: `row should complete and close gate with required evidence`
- evidence_path: `/home/dcalford/CliWork/ScratchBird/artifacts/baseline/milvus/p6s1w1/eng-milvus-001-build`
- proposed_options:
  1. Provide required `*.pc` providers and export `PKG_CONFIG_PATH` for Milvus core + third-party dependencies.
  2. Ensure Milvus core artifacts (including jemalloc shared object) are generated before `make -j2`.
  3. Keep downstream Milvus rows blocked until this build gate closes.
