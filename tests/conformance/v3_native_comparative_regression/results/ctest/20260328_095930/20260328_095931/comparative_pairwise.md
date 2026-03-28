# Native Comparative Regression Pairwise Verdicts

| Case | Dialect | Expectation | Result | Native Exec ms | Donor Exec ms | Ratio |
|---|---|---|---|---|---|---|
| `firebird_delete_all_rows` | `firebird` | `must_match` | `pass` | `2071` | `94` | `22.031915` |
| `firebird_insert_default_values` | `firebird` | `must_match` | `pass` | `2336` | `103` | `22.679612` |
| `firebird_join_using` | `firebird` | `must_match` | `fail` | `2582` | `90` | `28.688889` |
| `firebird_avg_single_row` | `firebird` | `must_match` | `fail` | `2111` | `85` | `24.835294` |
| `firebird_count_empty` | `firebird` | `must_match` | `pass` | `2263` | `80` | `28.2875` |
| `mysql_alias_wildcard_error` | `mysql` | `must_fail_same_class` | `pass` | `1938` | `44` | `44.045455` |
| `mysql_ansi_concat_operator` | `mysql` | `must_match` | `pass` | `1910` | `36` | `53.055556` |
| `mysql_ansi_groupby_error` | `mysql` | `must_fail_same_class` | `pass` | `2282` | `40` | `57.05` |
| `mysql_insert_defaults_identity` | `mysql` | `must_match` | `fail` | `2021` | `101` | `20.009901` |
| `postgresql_select_ordered_predicate` | `postgresql` | `must_match` | `pass` | `3293` | `41` | `80.317073` |
| `postgresql_insert_defaults_values` | `postgresql` | `must_match` | `pass` | `6068` | `61` | `99.47541` |
| `postgresql_update_target_qualifier_error` | `postgresql` | `must_fail_same_class` | `pass` | `2276` | `17` | `133.882353` |
| `postgresql_delete_alias_visibility` | `postgresql` | `must_match` | `fail` | `4611` | `80` | `57.6375` |
| `postgresql_join_using` | `postgresql` | `must_match` | `pass` | `5814` | `69` | `84.26087` |
