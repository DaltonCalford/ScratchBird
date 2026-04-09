# Agent Spawn Pack

Date: 2026-03-03
Use this pack to launch parallel implementation agents from the baseline tracker.

## Global Rules for All Lane Agents
1. Treat `JDBCBL-*` as mandatory baseline floor.
2. Do not alter parser/engine invariants.
3. Implement metadata-only recursive schema tree behavior.
4. Update tracker row status and evidence path after each task.
5. Escalate any baseline conflict immediately.

## Lane Prompts
- `jdbc-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/jdbc`
  Prompt: `Execute DLB-JDBC-* tasks in order and keep JDBC baseline authoritative and regression-stable.`
- `odbc-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/odbc`
  Prompt: `Implement DLB-ODBC-* tasks in order and satisfy ODBCBL-* requirements.`
- `cpp-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/beta/drivers/cpp`
  Prompt: `Implement DLB-CPP-* tasks in order and satisfy CPPBL-* requirements.`
- `dotnet-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/dotnet`
  Prompt: `Implement DLB-DOTNET-* tasks in order and satisfy DOTNETBL-* requirements.`
- `go-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/go`
  Prompt: `Implement DLB-GO-* tasks in order and satisfy GOBL-* requirements.`
- `rust-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/rust`
  Prompt: `Implement DLB-RUST-* tasks in order and satisfy RUSTBL-* requirements.`
- `node-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/node`
  Prompt: `Implement DLB-NODE-* tasks in order and satisfy NODEBL-* requirements.`
- `python-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/python`
  Prompt: `Implement DLB-PYTHON-* tasks in order and satisfy PYTHONBL-* requirements.`
- `php-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/php`
  Prompt: `Implement DLB-PHP-* tasks in order and satisfy PHPBL-* requirements.`
- `ruby-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/ruby`
  Prompt: `Implement DLB-RUBY-* tasks in order and satisfy RUBYBL-* requirements.`
- `pascal-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/pascal`
  Prompt: `Implement DLB-PASCAL-* tasks in order and satisfy PASCALBL-* requirements.`
- `mojo-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/mojo`
  Prompt: `Implement DLB-MOJO-* tasks in order and satisfy MOJOBL-* requirements.`
- `cli-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/cli`
  Prompt: `Implement DLB-CLI-* tasks in order and satisfy CLIBL-* requirements.`
- `dart-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/beta/drivers/dart`
  Prompt: `Implement DLB-DART-* tasks in order and satisfy DARTBL-* requirements.`
- `swift-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/beta/drivers/swift`
  Prompt: `Implement DLB-SWIFT-* tasks in order and satisfy SWIFTBL-* requirements.`
- `r-agent` owner path: `/home/dcalford/CliWork/ScratchBird-driver/tracks/beta/drivers/r`
  Prompt: `Implement DLB-R-* tasks in order and satisfy RBL-* requirements.`

## Integration Agent Prompt
Prompt:
`Run cross-lane conformance (T30-I, T30-J), publish per-lane pass/fail and unresolved gaps, and update gate states DLB-GATE-00..07.`
