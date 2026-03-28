# Native Comparative Regression Pairwise Verdicts

| Case | Dialect | Expectation | Result | Native Exec ms | Donor Exec ms | Ratio |
|---|---|---|---|---|---|---|
| `firebird_delete_all_rows` | `firebird` | `must_match` | `pass` | `1168` | `83` | `14.072289` |
| `firebird_insert_default_values` | `firebird` | `must_match` | `pass` | `1418` | `112` | `12.660714` |
| `firebird_join_using` | `firebird` | `must_match` | `fail` | `1289` | `87` | `14.816092` |
| `firebird_avg_single_row` | `firebird` | `must_match` | `fail` | `1243` | `86` | `14.453488` |
| `firebird_count_empty` | `firebird` | `must_match` | `pass` | `1254` | `82` | `15.292683` |
| `mysql_alias_wildcard_error` | `mysql` | `must_fail_same_class` | `pass` | `1214` | `32` | `37.9375` |
| `mysql_ansi_concat_operator` | `mysql` | `must_match` | `pass` | `1113` | `32` | `34.78125` |
| `mysql_ansi_groupby_error` | `mysql` | `must_fail_same_class` | `pass` | `1336` | `32` | `41.75` |
| `mysql_insert_defaults_identity` | `mysql` | `must_match` | `fail` | `1113` | `120` | `9.275` |
| `postgresql_select_ordered_predicate` | `postgresql` | `must_match` | `pass` | `2203` | `41` | `53.731707` |
| `postgresql_insert_defaults_values` | `postgresql` | `must_match` | `pass` | `3174` | `46` | `69.0` |
| `postgresql_update_target_qualifier_error` | `postgresql` | `must_fail_same_class` | `pass` | `1372` | `19` | `72.210526` |
| `postgresql_delete_alias_visibility` | `postgresql` | `must_match` | `fail` | `3425` | `65` | `52.692308` |
| `postgresql_join_using` | `postgresql` | `must_match` | `pass` | `4623` | `67` | `69.0` |
