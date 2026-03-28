# Native Comparative Regression Pairwise Verdicts

| Case | Dialect | Expectation | Result | Native Exec ms | Donor Exec ms | Ratio |
|---|---|---|---|---|---|---|
| `firebird_delete_all_rows` | `firebird` | `must_match` | `pass` | `3085` | `84` | `36.72619` |
| `firebird_insert_default_values` | `firebird` | `must_match` | `pass` | `3628` | `105` | `34.552381` |
| `firebird_join_using` | `firebird` | `must_match` | `fail` | `3825` | `94` | `40.691489` |
| `firebird_avg_single_row` | `firebird` | `must_match` | `fail` | `4263` | `100` | `42.63` |
| `firebird_count_empty` | `firebird` | `must_match` | `pass` | `3341` | `83` | `40.253012` |
| `mysql_alias_wildcard_error` | `mysql` | `must_fail_same_class` | `pass` | `3246` | `36` | `90.166667` |
| `mysql_ansi_concat_operator` | `mysql` | `must_match` | `pass` | `3392` | `37` | `91.675676` |
| `mysql_ansi_groupby_error` | `mysql` | `must_fail_same_class` | `pass` | `3518` | `38` | `92.578947` |
| `mysql_insert_defaults_identity` | `mysql` | `must_match` | `fail` | `3255` | `113` | `28.80531` |
| `postgresql_select_ordered_predicate` | `postgresql` | `must_match` | `pass` | `4872` | `40` | `121.8` |
| `postgresql_insert_defaults_values` | `postgresql` | `must_match` | `pass` | `6136` | `64` | `95.875` |
| `postgresql_update_target_qualifier_error` | `postgresql` | `must_fail_same_class` | `pass` | `3450` | `16` | `215.625` |
| `postgresql_delete_alias_visibility` | `postgresql` | `must_match` | `fail` | `6212` | `70` | `88.742857` |
| `postgresql_join_using` | `postgresql` | `must_match` | `pass` | `7822` | `71` | `110.169014` |
