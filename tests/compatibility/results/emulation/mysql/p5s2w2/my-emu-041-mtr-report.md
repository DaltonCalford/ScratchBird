Last updated: 2026-03-22

# MY-EMU-041 MTR Smoke Report

- Mode: `execute`
- Overall result: `pass`
- Command timeout: `7200s`

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
[ 13%] main.select_count                         [ pass ]    689
[ 20%] main.select_for_update                    [ pass ]   3850
[ 26%] main.select_all                           [ pass ]  32437
[ 33%] main.select_all_bka                       [ pass ]  41270
[ 40%] main.select_all_bka_nobnl                 [ pass ]  40062
[ 46%] main.select_found                         [ pass ]   3629
[ 53%] main.select_icp_mrr                       [ pass ]  42860
[ 60%] main.select_icp_mrr_bka                   [ pass ]  43704
[ 66%] main.select_icp_mrr_bka_nobnl             [ pass ]  43926
[ 73%] main.select_none                          [ pass ]  42427
[ 80%] main.select_none_bka                      [ pass ]  35163
[ 86%] main.select_none_bka_nobnl                [ pass ]  40147
[ 93%] main.select_safe                          [ pass ]   1028
[100%] shutdown_report                           [ pass ]       
------------------------------------------------------------------------------
The servers were restarted 2 times
The servers were reinitialized 0 times
Spent 371.192 of 395 seconds executing testcases

Completed: All 14 tests were successful.

1 tests were skipped, 0 by the test itself.
```

## Notes
- This is a smoke run for gate bootstrap; full required suites remain in MY-EMU-041 closure work.
