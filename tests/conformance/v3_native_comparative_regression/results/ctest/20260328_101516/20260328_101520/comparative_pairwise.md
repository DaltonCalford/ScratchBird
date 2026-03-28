# Native Comparative Regression Pairwise Verdicts

| Case | Dialect | Expectation | Result | Native Exec ms | Donor Exec ms | Ratio |
|---|---|---|---|---|---|---|
| `firebird_delete_all_rows` | `firebird` | `must_match` | `pass` | `4613` | `82` | `56.256098` |
| `firebird_insert_default_values` | `firebird` | `must_match` | `pass` | `5071` | `104` | `48.759615` |
| `firebird_join_using` | `firebird` | `must_match` | `pass` | `5399` | `87` | `62.057471` |
| `firebird_avg_single_row` | `firebird` | `must_match` | `pass` | `7178` | `90` | `79.755556` |
| `firebird_count_empty` | `firebird` | `must_match` | `pass` | `4879` | `85` | `57.4` |
| `mysql_alias_wildcard_error` | `mysql` | `must_fail_same_class` | `pass` | `4658` | `28` | `166.357143` |
| `mysql_ansi_concat_operator` | `mysql` | `must_match` | `pass` | `4630` | `38` | `121.842105` |
| `mysql_ansi_groupby_error` | `mysql` | `must_fail_same_class` | `pass` | `4841` | `33` | `146.69697` |
| `mysql_insert_defaults_identity` | `mysql` | `must_match` | `fail` | `4642` | `121` | `38.363636` |
| `postgresql_select_ordered_predicate` | `postgresql` | `must_match` | `pass` | `6371` | `42` | `151.690476` |
| `postgresql_insert_defaults_values` | `postgresql` | `must_match` | `pass` | `7820` | `56` | `139.642857` |
| `postgresql_update_target_qualifier_error` | `postgresql` | `must_fail_same_class` | `pass` | `5092` | `17` | `299.529412` |
| `postgresql_delete_alias_visibility` | `postgresql` | `must_match` | `fail` | `8306` | `63` | `131.84127` |
| `postgresql_join_using` | `postgresql` | `must_match` | `pass` | `10192` | `67` | `152.119403` |
