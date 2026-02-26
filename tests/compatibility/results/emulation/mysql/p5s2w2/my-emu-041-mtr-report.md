Last updated: 2026-02-26

# MY-EMU-041 MTR Smoke Report

- Mode: `execute`
- Overall result: `pass`
- Command timeout: `1200s`

## Command Results

### Command 1
- `cwd`: `<outside-tree-path>`
- `cmd`: `perl -c mysql-test-run.pl`
- `exit_code`: `0`
- `timed_out`: `false`

```text
mysql-test-run.pl syntax OK
```

### Command 2
- `cwd`: `<outside-tree-path>`
- `cmd`: `perl mysql-test-run.pl --suite=main --do-test=select --retry=0 --parallel=1 --force --client-bindir=<outside-tree-path>`
- `exit_code`: `0`
- `timed_out`: `false`

```text
Logging: mysql-test-run.pl  --suite=main --do-test=select --retry=0 --parallel=1 --force --client-bindir=<outside-tree-path>
MySQL Version 9.6.0
Checking supported features
Using suite(s): main
Collecting tests
Checking leftover processes
Removing old var directory
Creating var directory '<outside-tree-path>'
Installing system database
Using parallel: 1
ports_per_thread:30

==============================================================================
                  TEST NAME                       RESULT  TIME (ms) COMMENT
------------------------------------------------------------------------------
[  6%] main.select_distinct_debug                [ skipped ]  Test needs debug binaries.
[ 13%] main.select_count                         [ pass ]   1433
[ 20%] main.select_for_update                    [ pass ]  10853
[ 26%] main.select_all                           [ pass ]  66920
[ 33%] main.select_all_bka                       [ pass ]  69028
[ 40%] main.select_all_bka_nobnl                 [ pass ]  54291
[ 46%] main.select_found                         [ pass ]   3542
[ 53%] main.select_icp_mrr                       [ pass ]  46892
[ 60%] main.select_icp_mrr_bka                   [ pass ]  43280
[ 66%] main.select_icp_mrr_bka_nobnl             [ pass ]  43314
[ 73%] main.select_none                          [ pass ]  43490
[ 80%] main.select_none_bka                      [ pass ]  43208
[ 86%] main.select_none_bka_nobnl                [ pass ]  43721
[ 93%] main.select_safe                          [ pass ]   1066
[100%] shutdown_report                           [ pass ]       
------------------------------------------------------------------------------
The servers were restarted 2 times
The servers were reinitialized 0 times
Spent 471.038 of 503 seconds executing testcases

Completed: All 14 tests were successful.

1 tests were skipped, 0 by the test itself.
```

## Notes
- This is a smoke run for gate bootstrap; full required suites remain in MY-EMU-041 closure work.
