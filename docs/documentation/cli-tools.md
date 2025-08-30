### CLI Tools

dbcheck (src/dbcheck.cpp):
- Usage: `dbcheck <database_path> [options]`
- Options:
  - `--quick` quick corruption check only
  - `--verbose` detailed progress output
  - `--no-checksums` skip page checksum verification
  - `--no-tuples` skip tuple-level validation
  - `--max-issues N` limit output to N issues (default 1000)
- Exit codes: 0=ok, 1=warnings, 2=corruption, 3=critical, 4=tool error

dbspace (src/dbspace.cpp):
- Usage: `dbspace <database_path>`
- Prints segment utilization, fragmentation, and space pressure; exit code 2 for critical, 1 for high, 0 success.

isql:
- Not available in this source tree. CMake stubs exist but sources are disabled.

