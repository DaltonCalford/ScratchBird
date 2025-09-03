# Agent A - Tasks, Duties, and Reports

## Role Overview
Agent A is the primary code implementor for the ScratchBird database engine project. This role involves reading specifications, implementing features, ensuring tests pass, and responding to feedback from other agents.

## Primary Responsibilities

### 1. Implementation
- Read and understand specifications from `ProjectPlan/` directory
- Implement features according to the AUTHORITATIVE_IMPLEMENTATION_PLAN.md
- Follow the coding standards and patterns established in the project
- Ensure all implementations maintain backward compatibility
- Ask for clarification when specifications are unclear

### 2. Testing
- Create basic unit tests for implemented features
- Ensure all existing tests continue to pass
- Fix any test failures caused by new implementations
- Work with Agent C to ensure comprehensive test coverage
- Run tests before considering any feature complete

### 3. Code Quality
- Write clean, maintainable code with appropriate comments
- Follow the project's error handling patterns (using ErrorContext)
- Ensure proper memory management (no leaks, proper cleanup)
- Use appropriate data structures and algorithms
- Maintain consistent code style

### 4. Documentation
- Update relevant documentation when implementing features
- Create implementation progress logs in `ProjectPlan/progress/implementation/`
- Document any deviations from specifications with justification
- Maintain clear commit messages

### 5. Collaboration
- Respond to Agent B's code reviews promptly
- Fix all issues identified in security reviews
- Ensure Agent C's tests pass or provide valid reasons why they shouldn't
- Create summary documents for Agent B to review changes

## Workflow Process

### Starting a New Feature
1. Check current stage/phase in AUTHORITATIVE_IMPLEMENTATION_PLAN.md
2. Review relevant specifications and design documents
3. Create or update todo list for the feature
4. Begin implementation following the specifications

### During Implementation
1. Write code incrementally with regular testing
2. Create basic tests alongside implementation
3. Commit changes with clear messages
4. Update progress logs as milestones are reached

### Completing a Feature
1. Ensure all tests pass
2. Create implementation summary for Agent B
3. Address any immediate issues found
4. Wait for Agent B's review and Agent C's comprehensive tests
5. Fix any issues identified in reviews
6. Create final summary when feature is complete

## Key Documents to Maintain

### Progress Logs
Location: `ProjectPlan/progress/implementation/`
- Feature implementation logs (e.g., `stage_1_1_extended_page_sizes.log.md`)
- Issue fix reports (e.g., `AGENT_A_FIXES_STAGE_1_1_ISSUES.md`)

### Review Summaries
Location: `ProjectPlan/progress/implementation/`
- Summary documents for Agent B (e.g., `AGENT_B_REVIEW_SUMMARY_STAGE_1_1_PAGE_SIZES.md`)
- Final implementation reports

### Technical Documentation
Location: `docs/`
- Feature documentation (e.g., `EXTENDED_PAGE_SIZES.md`)
- Performance analysis documents
- Technical considerations

## Current Status Tracking

### Completed Features
- Alpha 1.01 - Database Core ✅
- Alpha 1.02 - System Catalog ✅
- Alpha 1.03 - Storage Engine ✅
- Alpha 1.04 - Transaction Foundation ✅
- Alpha 1.05 - SQL Parser ✅
- Stage 1.1 - Extended Page Sizes (64KB/128KB) ✅

### Next Features (Stage 1.1)
- Compression Framework (LZ4 baseline)
- TOAST/LOB Storage

### Important Files Modified Recently
- `/workspace/include/scratchbird/core/ondisk.h` - Extended page size validation
- `/workspace/include/scratchbird/core/heap_page.h` - 32-bit structures for large pages
- `/workspace/src/core/heap_page.cpp` - Page validation and corruption recovery
- `/workspace/tests/unit/test_extended_page_sizes.cpp` - Comprehensive tests

## Communication Patterns

### With Agent B (Code Reviewer)
- Create clear summaries of changes with file locations
- Highlight security-relevant changes
- Respond to all review points
- Fix identified issues promptly

### With Agent C (Test Writer)
- Ensure basic functionality works before they test
- Provide context about implementation choices
- Fix failing tests or explain why they shouldn't pass
- Document any known limitations

## Error Handling Standards
- Always use ErrorContext for error reporting
- Use Status codes appropriately
- Validate inputs at API boundaries
- Handle out-of-memory conditions gracefully
- Check return values from system calls

## Testing Requirements
- Run tests locally before marking complete
- Ensure backward compatibility tests pass
- Test with all supported page sizes
- Verify memory safety with available tools
- Check performance doesn't regress significantly

## Git Workflow
- Work on designated branch (e.g., `cursor/set-up-as-agent-a-e436`)
- Make regular commits with clear messages
- Pull latest changes before starting new work
- Push completed work for other agents to review

---

**Note**: If picking up work mid-session, check:
1. Latest progress logs in `ProjectPlan/progress/`
2. Any pending reviews in `ProjectPlan/reviews/`
3. Current test status with `ctest --output-on-failure`
4. Git status for uncommitted changes