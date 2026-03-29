# Native Comparative Regression Pairwise Verdicts

| Case | Dialect | Expectation | Result | Native Exec ms | Donor Exec ms | Ratio |
|---|---|---|---|---|---|---|
| `firebird_delete_all_rows` | `firebird` | `must_match` | `pass` | `825` | `88` | `9.375` |
| `firebird_insert_default_values` | `firebird` | `must_match` | `pass` | `1098` | `109` | `10.073394` |
| `firebird_join_using` | `firebird` | `must_match` | `pass` | `1270` | `87` | `14.597701` |
| `firebird_avg_single_row` | `firebird` | `must_match` | `pass` | `821` | `86` | `9.546512` |
| `firebird_count_empty` | `firebird` | `must_match` | `pass` | `826` | `89` | `9.280899` |
| `mysql_alias_wildcard_error` | `mysql` | `must_fail_same_class` | `pass` | `711` | `30` | `23.7` |
| `mysql_ansi_concat_operator` | `mysql` | `must_match` | `pass` | `749` | `36` | `20.805556` |
| `mysql_ansi_groupby_error` | `mysql` | `must_fail_same_class` | `pass` | `1000` | `38` | `26.315789` |
| `mysql_insert_defaults_identity` | `mysql` | `must_match` | `pass` | `3759` | `108` | `34.805556` |
| `postgresql_select_ordered_predicate` | `postgresql` | `must_match` | `pass` | `2305` | `31` | `74.354839` |
| `postgresql_insert_defaults_values` | `postgresql` | `must_match` | `pass` | `3595` | `53` | `67.830189` |
| `postgresql_update_target_qualifier_error` | `postgresql` | `must_fail_same_class` | `pass` | `1001` | `19` | `52.684211` |
| `postgresql_delete_alias_visibility` | `postgresql` | `must_match` | `pass` | `3900` | `64` | `60.9375` |
| `postgresql_join_using` | `postgresql` | `must_match` | `pass` | `5616` | `79` | `71.088608` |
