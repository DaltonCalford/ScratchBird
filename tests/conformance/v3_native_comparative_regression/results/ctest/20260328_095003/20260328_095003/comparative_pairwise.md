# Native Comparative Regression Pairwise Verdicts

| Case | Dialect | Expectation | Result | Native Exec ms | Donor Exec ms | Ratio |
|---|---|---|---|---|---|---|
| `firebird_delete_all_rows` | `firebird` | `must_match` | `fail` | `939` | `231` | `4.064935` |
| `firebird_insert_default_values` | `firebird` | `must_match` | `fail` | `1071` | `236` | `4.538136` |
| `firebird_join_using` | `firebird` | `must_match` | `fail` | `1749` | `432` | `4.048611` |
| `firebird_avg_single_row` | `firebird` | `must_match` | `fail` | `1023` | `257` | `3.980545` |
| `firebird_count_empty` | `firebird` | `must_match` | `fail` | `933` | `215` | `4.339535` |
| `mysql_alias_wildcard_error` | `mysql` | `must_fail_same_class` | `fail` | `482` | `34` | `14.176471` |
| `mysql_ansi_concat_operator` | `mysql` | `must_match` | `pass` | `471` | `37` | `12.72973` |
| `mysql_ansi_groupby_error` | `mysql` | `must_fail_same_class` | `fail` | `653` | `32` | `20.40625` |
| `mysql_insert_defaults_identity` | `mysql` | `must_match` | `fail` | `514` | `104` | `4.942308` |
| `postgresql_select_ordered_predicate` | `postgresql` | `must_match` | `pass` | `1214` | `54` | `22.481481` |
| `postgresql_insert_defaults_values` | `postgresql` | `must_match` | `pass` | `1860` | `65` | `28.615385` |
| `postgresql_update_target_qualifier_error` | `postgresql` | `must_fail_same_class` | `fail` | `643` | `16` | `40.1875` |
| `postgresql_delete_alias_visibility` | `postgresql` | `must_match` | `fail` | `1990` | `68` | `29.264706` |
| `postgresql_join_using` | `postgresql` | `must_match` | `pass` | `2794` | `70` | `39.914286` |
