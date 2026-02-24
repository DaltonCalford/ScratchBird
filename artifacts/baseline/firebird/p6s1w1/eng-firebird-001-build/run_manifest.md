# Run Manifest

- timestamp_utc: `2026-02-24T05:01:55Z`
- row_id: `58`
- task_id: `ENG-FIREBIRD-001`
- workstream: `firebird`
- slice: `E1`
- owner: `agent-firebird`
- sprint: `P6-S1/W1`
- gate: `ENG-FIREBIRD-GATE-01`
- status: `blocked`
- depends_on: `BASE-001`
- parser_emitter_executor_touched_in_cycle: `parser:no|emitter:no|executor:no`

## Host Snapshot
- cwd: `/home/dcalford/CliWork`
- host_platform: `Linux-6.17.0-14-generic-x86_64-with-glibc2.39`
- python: `3.12.3 (main, Jan 22 2026, 20:57:42) [GCC 13.3.0]`
- timestamp_utc: `2026-02-24T04:52:13Z`
- tool_ant: `Apache Ant(TM) version 1.10.14 compiled on September 25 2023`
- tool_bazel: `bazel 9.0.0`
- tool_cargo: `cargo 1.93.0 (083ac5135 2025-12-15)`
- tool_cmake: `cmake version 3.30.5`
- tool_cqlsh: `missing`
- tool_g++: `g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`
- tool_gcc: `gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`
- tool_go: `go version go1.22.2 linux/amd64`
- tool_gradle: `openjdk version "21.0.10" 2026-01-20
OpenJDK Runtime Environment (build 21.0.10+7-Ubuntu-124.04)
OpenJDK 64-Bit Server VM (build 21.0.10+7-Ubuntu-124.04, mixed mode, sharing)`
- tool_java: `openjdk version "21.0.10" 2026-01-20`
- tool_make: `GNU Make 4.3`
- tool_mongod: `bash: line 1: mongod: command not found`
- tool_mvn: `bash: line 1: mvn: command not found`
- tool_mysql: `bash: line 1: mysql: command not found`
- tool_ninja: `1.11.1`
- tool_perl: `This is perl 5, version 38, subversion 2 (v5.38.2) built for x86_64-linux-gnu-thread-multi`
- tool_psql: `psql (PostgreSQL) 18.2 (Ubuntu 18.2-1.pgdg24.04+1)`
- tool_pytest: `pytest 7.4.4`
- tool_python3: `Python 3.12.3`
- tool_redis-server: `missing`
- tool_rustc: `rustc 1.93.0 (254b59607 2026-01-19)`

## Commands
- 1. cwd=`/home/dcalford/CliWork` timeout_s=`20` cmd=`test -d /home/dcalford/CliWork/firebird && git -C /home/dcalford/CliWork/firebird rev-parse --short HEAD`
- 2. cwd=`/home/dcalford/CliWork/firebird` timeout_s=`180` cmd=`./configure`
- 3. cwd=`/home/dcalford/CliWork/firebird` timeout_s=`300` cmd=`make -j2`
