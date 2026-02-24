# Issues

- task_id: `ENG-OPENSEARCH-002`
- gate: `ENG-OPENSEARCH-GATE-02`
- blocker_class: `build reproducibility`
- observed_behavior: `Gradle check timed out while progressing through broad multi-module precommit/test graph (many tasks complete/UP-TO-DATE, run still active near :client:rest:test)`
- expected_behavior: `row should complete and close gate with required evidence`
- evidence_path: `/home/dcalford/CliWork/ScratchBird/artifacts/baseline/opensearch/p6s1w2/eng-opensearch-002-tests`
- proposed_options:
  1. Increase timeout budget for OpenSearch `./gradlew check`.
  2. Define a deterministic smoke slice for this gate if full check is out of cycle scope.
  3. Keep downstream OpenSearch rows blocked until test gate closes.
