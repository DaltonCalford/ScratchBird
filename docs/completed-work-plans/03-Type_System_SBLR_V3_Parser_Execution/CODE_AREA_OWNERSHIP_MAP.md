# Code Area Ownership Map

## Primary Write Scopes

| Ticket | Primary write scope | Conflict surfaces | Parallelization rule |
| --- | --- | --- | --- |
| B1-03-001 | assigned section specs plus this package | all package control files | serial only |
| B1-03-002 | primary canonical targets for sections `12,13,14,15,16,17,21,22,23` and the bounded section-28 subset, plus package control files | all package control files and primary section README targets | serial only |
| B1-03-003 | include/scratchbird/core/typed_value.h, src/core/typed_value.cpp, include/scratchbird/core/type_serialization.h, src/core/type_serialization.cpp, src/core/type_system.cpp, src/core/domain_manager.cpp, include/scratchbird/core/connection_context.h, src/core/connection_context.cpp, include/scratchbird/core/catalog_manager.h, src/core/catalog_manager.cpp, src/sblr/executor.cpp, src/sblr/expression_evaluator.cpp, src/sblr/extract_element_ops.cpp, include/scratchbird/parser/ast_v3.h, src/parser/parser_v3.cpp, src/parser/v3_emitter.cpp | shared parser or emitter seams and executor or runtime coercion seams | after ownership freeze |
| B1-03-004 | include/scratchbird/parser/parser_v3.h, include/scratchbird/parser/v3_emitter.h, include/scratchbird/sblr/ast_sblr_lowerer.h, src/sblr/ast_sblr_lowerer.cpp, include/scratchbird/sblr/query_compiler_v3.h, src/sblr/query_compiler_v3_optimizer_support.cpp, src/sblr/native_sql_render_contract.cpp, src/sblr/native_sql_renderer.cpp, src/sblr/language_udr_sql_render_endpoint.cpp, src/sblr/v3_container.cpp, src/sblr/v3_payloads.cpp, src/sblr/v3_validator.cpp, include/scratchbird/optimizer/query_planner.h, src/optimizer/query_planner.cpp, include/scratchbird/optimizer/vnext_plan_cache.h, src/optimizer/vnext_plan_cache.cpp | parser or emitter overlap with lane A and compiler or planner handoff seams | after lane A foundation |
| B1-03-005 | language and execution gates | shared gate runners | after implementation tickets |

## Unsafe Parallel Boundaries

- any ticket that updates SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv
- any ticket that changes the same canonical spec file as another ticket
- any ticket that changes the same gate or benchmark artifact family
- any ticket that changes `src/parser/parser_v3.cpp`, `src/parser/v3_emitter.cpp`,
  `src/sblr/ast_sblr_lowerer.cpp`, `include/scratchbird/sblr/query_compiler_v3.h`,
  `src/sblr/query_compiler_v3_optimizer_support.cpp`, or `src/sblr/executor.cpp`
