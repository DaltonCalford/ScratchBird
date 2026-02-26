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
[ 13%] main.select_count                         [ pass ]    783
[ 20%] main.select_for_update                    [ pass ]   9815
[ 26%] main.select_all                           [ pass ]  61424
[ 33%] main.select_all_bka                       [ pass ]  62656
[ 40%] main.select_all_bka_nobnl                 [ pass ]  57966
[ 46%] main.select_found                         [ pass ]   3553
[ 53%] main.select_icp_mrr                       [ pass ]  46054
[ 60%] main.select_icp_mrr_bka                   [ pass ]  43733
[ 66%] main.select_icp_mrr_bka_nobnl             [ pass ]  44248
[ 73%] main.select_none                          [ pass ]  43690
[ 80%] main.select_none_bka                      [ pass ]  43729
[ 86%] main.select_none_bka_nobnl                [ pass ]  43495
[ 93%] main.select_safe                          [ pass ]   1088
[100%] shutdown_report                           [ pass ]       
------------------------------------------------------------------------------
The servers were restarted 2 times
The servers were reinitialized 0 times
Spent 462.234 of 490 seconds executing testcases

Completed: All 14 tests were successful.

1 tests were skipped, 0 by the test itself.
```

## Notes
- This is a smoke run for gate bootstrap; full required suites remain in MY-EMU-041 closure work.
