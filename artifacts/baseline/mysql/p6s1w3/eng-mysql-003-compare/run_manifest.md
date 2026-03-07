# Run Manifest

- timestamp_utc: `2026-03-05T14:53:35Z`
- row_id: `64`
- task_id: `ENG-MYSQL-003`
- workstream: `mysql`
- slice: `E3`
- owner: `agent-mysql`
- sprint: `P6-S1/W3`
- gate: `ENG-MYSQL-GATE-03`
- status: `blocked`
- depends_on: `ENG-MYSQL-002;SB-006`
- parser_emitter_executor_touched_in_cycle: `parser:no|emitter:no|executor:no`

## Host Snapshot
- cwd: `/home/dcalford/CliWork/ScratchBird`
- host_platform: `Linux-6.17.0-14-generic-x86_64-with-glibc2.39`
- python: `3.12.3 (main, Jan 22 2026, 20:57:42) [GCC 13.3.0]`
- timestamp_utc: `2026-03-05T14:53:27Z`
- tool_ant: `Apache Ant(TM) version 1.10.14 compiled on September 25 2023`
- tool_bazel: `bazel 9.0.0`
- tool_cargo: `cargo 1.93.1 (083ac5135 2025-12-15)`
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
- tool_mongod: `db version v8.0.19`
- tool_mvn: `[1mApache Maven 3.8.7[m`
- tool_mysql: `bash: line 1: mysql: command not found`
- tool_ninja: `1.11.1`
- tool_perl: `This is perl 5, version 38, subversion 2 (v5.38.2) built for x86_64-linux-gnu-thread-multi`
- tool_psql: `psql (PostgreSQL) 18.3 (Ubuntu 18.3-1.pgdg24.04+1)`
- tool_pytest: `pytest 7.4.4`
- tool_python3: `Python 3.12.3`
- tool_redis-server: `missing`
- tool_rustc: `rustc 1.93.1 (01f6ddf75 2026-02-11)`

## Commands
- 1. cwd=`/home/dcalford/CliWork` timeout_s=`160` cmd=`test -d /home/dcalford/CliWork/mysql-server && git -C /home/dcalford/CliWork/mysql-server rev-parse --short HEAD`
- 2. cwd=`/home/dcalford/CliWork/ScratchBird` timeout_s=`960` cmd=`bash tests/compatibility/mysql/scripts/run_mysql_ctest.sh`
