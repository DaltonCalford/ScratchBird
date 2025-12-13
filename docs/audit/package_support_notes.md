# Package Support Notes (Snapshot)

## Current Implementation
- Catalog: `packages` table with header/body stored via TOAST; CRUD exposed (`createPackage`, `getPackage`, `getPackageByName`, `updatePackage`, `dropPackage`, `listPackages`).
- Semantic/dependency: best-effort package dependency added when encountering `package.member` names in SemanticAnalyzerV2/QueryCompilerV2.
- Lookup: unified object lookup can return package by name (schema-scoped) with ObjectType::PACKAGE.
- Namespace resolution: no clear disambiguation logic between `<schema>.<procedure>` vs `<package>.<procedure>`; current heuristic in semantic layer just adds a package dependency when a dotted name is seen, without explicit resolution rules or stored-code dispatch.

## Missing/Unclear
- Name resolution priority/rules for `schema.proc` vs `package.proc` and nested scopes.
- Execution/dispatch: no validated path that routes calls to package-contained routines vs schema-level routines.
- Dependency enforcement: package members not separately cataloged; dropping a package may not cascade/block routines; dependency graph may be incomplete.
- Schema/package scoping: unclear how packages interact with search_path/current_schema.
- Visibility rules: package members should be visible to each other (procedures/functions/triggers/temp tables) with full internal rights; grant EXECUTE on package should allow internal calls without extra grants; default listings should hide package members unless explicitly querying the package.

## Actions
- Define resolution rules for dotted names and implement in semantic analyzer and executor.  
- Add package member cataloging or routing logic so calls resolve to the correct routine.  
- Enforce dependencies: link package -> routines; block drops appropriately.  
- Tests: name resolution collisions (`schema.proc` vs `package.proc`), dependency behavior, drop/package update effects.  
- Implement package-as-container flag: default SHOW lists exclude package members; add SHOW ... IN PACKAGE <pkg>.  
- Enforce package-internal visibility and rights: all members can call/select/update/delete each other; GRANT EXECUTE ON PACKAGE covers internal calls; ensure search/path resolution within package scope.  
