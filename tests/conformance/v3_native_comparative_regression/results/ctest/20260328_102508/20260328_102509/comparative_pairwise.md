# Native Comparative Regression Pairwise Verdicts

| Case | Dialect | Expectation | Result | Native Exec ms | Donor Exec ms | Ratio |
|---|---|---|---|---|---|---|
| `firebird_delete_all_rows` | `firebird` | `must_match` | `pass` | `742` | `89` | `8.337079` |
| `firebird_insert_default_values` | `firebird` | `must_match` | `pass` | `944` | `111` | `8.504505` |
| `firebird_join_using` | `firebird` | `must_match` | `pass` | `1100` | `81` | `13.580247` |
| `firebird_avg_single_row` | `firebird` | `must_match` | `pass` | `702` | `95` | `7.389474` |
| `firebird_count_empty` | `firebird` | `must_match` | `pass` | `786` | `80` | `9.825` |
| `mysql_alias_wildcard_error` | `mysql` | `must_fail_same_class` | `pass` | `649` | `36` | `18.027778` |
| `mysql_ansi_concat_operator` | `mysql` | `must_match` | `pass` | `678` | `33` | `20.545455` |
| `mysql_ansi_groupby_error` | `mysql` | `must_fail_same_class` | `pass` | `2106` | `33` | `63.818182` |
| `mysql_insert_defaults_identity` | `mysql` | `must_match` | `pass` | `2610` | `103` | `25.339806` |
| `postgresql_select_ordered_predicate` | `postgresql` | `must_match` | `pass` | `1661` | `44` | `37.75` |
| `postgresql_insert_defaults_values` | `postgresql` | `must_match` | `pass` | `2444` | `59` | `41.423729` |
| `postgresql_update_target_qualifier_error` | `postgresql` | `must_fail_same_class` | `pass` | `916` | `15` | `61.066667` |
| `postgresql_delete_alias_visibility` | `postgresql` | `must_match` | `pass` | `2600` | `64` | `40.625` |
| `postgresql_join_using` | `postgresql` | `must_match` | `pass` | `3687` | `74` | `49.824324` |
