Last updated: 2026-03-06

# PG-EMU-041 Regression Smoke Report

- Mode: `execute`
- Overall result: `pass`
- Command timeout: `7200s`

## Command Results

### Command 1
- `cwd`: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/postgresql/repos/postgres/src/test/regress`
- `cmd`: `bash -lc 'test -f GNUmakefile && test -d sql && echo pg_regress_snapshot_present'`
- `exit_code`: `0`
- `timed_out`: `false`

```text
pg_regress_snapshot_present
```

### Command 2
- `cwd`: `<outside-tree-path>`
- `cmd`: `make -C src/test/regress check`
- `exit_code`: `0`
- `timed_out`: `false`

```text
ke -j1  checkprep >>'<outside-tree-path>'/tmp_install/log/install.log 2>&1
PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  initdb --auth trust --no-sync --no-instructions --lc-messages=C --no-clean '<outside-tree-path>'/tmp_install/initdb-template >>'<outside-tree-path>'/tmp_install/log/initdb-template.log 2>&1
echo "# +++ regress check in src/test/regress +++" && PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  ../../../src/test/regress/pg_regress --temp-instance=./tmp_check --inputdir=<outside-tree-path> --bindir=     --dlpath=. --max-concurrent-tests=20  --schedule=<outside-tree-path>  
# +++ regress check in src/test/regress +++
# initializing database system by copying initdb template
# using temp instance on port 58928 with PID 1437357
ok 1         - test_setup                                223 ms
# parallel group (20 tests):  text char varchar name oid int2 int4 pg_lsn boolean txid money float4 regproc int8 float8 bit enum uuid numeric rangetypes
ok 2         + boolean                                    30 ms
ok 3         + char                                       20 ms
ok 4         + name                                       24 ms
ok 5         + varchar                                    22 ms
ok 6         + text                                       17 ms
ok 7         + int2                                       24 ms
ok 8         + int4                                       26 ms
ok 9         + int8                                       47 ms
ok 10        + oid                                        23 ms
ok 11        + float4                                     36 ms
ok 12        + float8                                     50 ms
ok 13        + bit                                        57 ms
ok 14        + numeric                                   242 ms
ok 15        + txid                                       29 ms
ok 16        + uuid                                       99 ms
ok 17        + enum                                       71 ms
ok 18        + money                                      32 ms
ok 19        + rangetypes                                363 ms
ok 20        + pg_lsn                                     25 ms
ok 21        + regproc                                    40 ms
# parallel group (20 tests):  md5 lseg circle line path timetz macaddr8 point time macaddr numerology date inet interval strings polygon box multirangetypes timestamp timestamptz
ok 22        + strings                                   148 ms
ok 23        + md5                                        17 ms
ok 24        + numerology                                 55 ms
ok 25        + point                                      47 ms
ok 26        + lseg                                       25 ms
ok 27        + line                                       36 ms
ok 28        + box                                       235 ms
ok 29        + path                                       37 ms
ok 30        + polygon                                   187 ms
ok 31        + circle                                     26 ms
ok 32        + date                                       62 ms
ok 33        + time                                       46 ms
ok 34        + timetz                                     40 ms
ok 35        + timestamp                                 373 ms
ok 36        + timestamptz                               386 ms
ok 37        + interval                                   93 ms
ok 38        + inet                                       77 ms
ok 39        + macaddr                                    49 ms
ok 40        + macaddr8                                   37 ms
ok 41        + multirangetypes                           278 ms
# parallel group (19 tests):  comments unicode misc_sanity euc_kr pg_ndistinct pg_dependencies tstypes encoding xid expressions oid8 horology mvcc geometry type_sanity stats_import opr_sanity database regex
ok 42        + geometry                                  109 ms
ok 43        + horology                                   77 ms
ok 44        + tstypes                                    50 ms
ok 45        + regex                                     489 ms
ok 46        + type_sanity                               123 ms
ok 47        + opr_sanity                                279 ms
ok 48        + misc_sanity                                21 ms
ok 49        + comments                                   17 ms
ok 50        + expressions                                59 ms
ok 51        + unicode                                    20 ms
ok 52        + xid                                        58 ms
ok 53        + mvcc                                      102 ms
ok 54        + database                                  303 ms
ok 55        + stats_import                              181 ms
ok 56        + pg_ndistinct                               29 ms
ok 57        + pg_dependencies                            39 ms
ok 58        + oid8                                       59 ms
ok 59        + encoding                                   55 ms
ok 60        + euc_kr                                     21 ms
# parallel group (6 tests):  copyencoding copyselect copydml copy insert_conflict insert
ok 61        + copy                                      124 ms
ok 62        + copyselect                                 27 ms
ok 63        + copydml                                    37 ms
ok 64        + copyencoding                               18 ms
ok 65        + insert                                    413 ms
ok 66        + insert_conflict                           159 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_misc create_schema create_procedure create_table
ok 67        + create_function_c                          13 ms
ok 68        + create_misc                                43 ms
ok 69        + create_operator                            26 ms
ok 70        + create_procedure                           67 ms
ok 71        + create_table                              416 ms
ok 72        + create_type                                32 ms
ok 73        + create_schema                              47 ms
# parallel group (5 tests):  index_including index_including_gist create_view create_index_spgist create_index
ok 74        + create_index                              788 ms
ok 75        + create_index_spgist                       399 ms
ok 76        + create_view                               316 ms
ok 77        + index_including                           199 ms
ok 78        + index_including_gist                      220 ms
# parallel group (16 tests):  create_cast errors roleattributes hash_func create_aggregate select drop_if_exists typed_table create_function_sql create_am infinite_recurse vacuum updatable_views constraints triggers inherit
ok 79        + create_aggregate                           39 ms
ok 80        + create_function_sql                       119 ms
ok 81        + create_cast                                28 ms
ok 82        + constraints                               654 ms
ok 83        + triggers                                  896 ms
ok 84        + select                                     55 ms
ok 85        + inherit                                   959 ms
ok 86        + typed_table                               106 ms
ok 87        + vacuum                                    372 ms
ok 88        + drop_if_exists                             63 ms
ok 89        + updatable_views                           622 ms
ok 90        + roleattributes                             29 ms
ok 91        + create_am                                 138 ms
ok 92        + hash_func                                  34 ms
ok 93        + errors                                     28 ms
ok 94        + infinite_recurse                          244 ms
ok 95        - sanity_check                               90 ms
# parallel group (20 tests):  select_having select_distinct_on case delete select_implicit random prepared_xacts select_into transactions namespace union portals select_distinct arrays subselect update hash_index aggregates join btree_index
ok 96        + select_into                               164 ms
ok 97        + select_distinct                           226 ms
ok 98        + select_distinct_on                         53 ms
ok 99        + select_implicit                            80 ms
ok 100       + select_having                              34 ms
ok 101       + subselect                                 418 ms
ok 102       + union                                     216 ms
ok 103       + case                                       57 ms
ok 104       + join                                      789 ms
ok 105       + aggregates                                695 ms
ok 106       + transactions                              189 ms
ok 107       + random                                     84 ms
ok 108       + portals                                   213 ms
ok 109       + arrays                                    391 ms
ok 110       + btree_index                              1282 ms
ok 111       + hash_index                                673 ms
ok 112       + update                                    498 ms
ok 113       + delete                                     64 ms
ok 114       + namespace                                 208 ms
ok 115       + prepared_xacts                            129 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock object_address password collate replica_identity groupingsets matview identity rowsecurity spgist generated_stored gin gist join_hash brin privileges
ok 116       + brin                                     1390 ms
ok 117       + gin                                       886 ms
ok 118       + gist                                      887 ms
ok 119       + spgist                                    727 ms
ok 120       + privileges                               1717 ms
ok 121       + init_privs                                 24 ms
ok 122       + security_label                             51 ms
ok 123       + collate                                   214 ms
ok 124       + matview                                   501 ms
ok 125       + lock                                      122 ms
ok 126       + replica_identity                          234 ms
ok 127       + rowsecurity                               673 ms
ok 128       + object_address                            138 ms
ok 129       + tablesample                               117 ms
ok 130       + groupingsets                              344 ms
ok 131       + drop_operator                              36 ms
ok 132       + password                                  210 ms
ok 133       + identity                                  505 ms
ok 134       + generated_stored                          760 ms
ok 135       + join_hash                                1374 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 78 ms
ok 137       + brin_multi                                225 ms
# parallel group (20 tests):  async dbsize nls tidscan alter_operator tsrf collate.utf8 tid sysviews create_role misc_functions tidrangescan alter_generic misc incremental_sort merge create_table_like collate.icu.utf8 generated_virtual without_overlaps
ok 138       + create_table_like                         418 ms
ok 139       + alter_generic                             146 ms
ok 140       + alter_operator                             67 ms
ok 141       + misc                                      156 ms
ok 142       + async                                      17 ms
ok 143       + dbsize                                     21 ms
ok 144       + merge                                     348 ms
ok 145       + misc_functions                            120 ms
ok 146       + nls                                        25 ms
ok 147       + sysviews                                   78 ms
ok 148       + tsrf                                       69 ms
ok 149       + tid                                        74 ms
ok 150       + tidscan                                    61 ms
ok 151       + tidrangescan                              129 ms
ok 152       + collate.utf8                               70 ms
ok 153       + collate.icu.utf8                          427 ms
ok 154       + incremental_sort                          185 ms
ok 155       + create_role                                98 ms
ok 156       + without_overlaps                          725 ms
ok 157       + generated_virtual                         444 ms
# parallel group (8 tests):  collate.windows.win1252 collate.linux.utf8 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     360 ms
ok 159       + psql                                      516 ms
ok 160       + psql_crosstab                              31 ms
ok 161       + psql_pipeline                              35 ms
ok 162       + amutils                                    19 ms
ok 163       + stats_ext                                1405 ms
ok 164       + collate.linux.utf8                         11 ms
ok 165       + collate.windows.win1252                    10 ms
ok 166       - select_parallel                           558 ms
ok 167       - write_parallel                             62 ms
ok 168       - vacuum_parallel                            80 ms
ok 169       - maintain_every                             19 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               349 ms
ok 171       + subscription                               45 ms
# parallel group (18 tests):  portals_p2 advisory_lock xmlmap combocid tsdicts functional_deps equivclass guc dependency select_views stats_rewrite window bitmapops tsearch cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                              114 ms
ok 173       + portals_p2                                 23 ms
ok 174       + foreign_key                              1054 ms
ok 175       + cluster                                   297 ms
ok 176       + dependency                                 89 ms
ok 177       + guc                                        80 ms
ok 178       + bitmapops                                 244 ms
ok 179       + combocid                                   39 ms
ok 180       + tsearch                                   274 ms
ok 181       + tsdicts                                    52 ms
ok 182       + foreign_data                              584 ms
ok 183       + window                                    230 ms
ok 184       + xmlmap                                     37 ms
ok 185       + functional_deps                            61 ms
ok 186       + advisory_lock                              31 ms
ok 187       + indirect_toast                            320 ms
ok 188       + equivclass                                 69 ms
ok 189       + stats_rewrite                             121 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson json sqljson_jsontable jsonb_jsonpath sqljson_queryfuncs jsonb
ok 190       + json                                       60 ms
ok 191       + jsonb                                     207 ms
ok 192       + json_encoding                              12 ms
ok 193       + jsonpath                                   24 ms
ok 194       + jsonpath_encoding                          10 ms
ok 195       + jsonb_jsonpath                             79 ms
ok 196       + sqljson                                    49 ms
ok 197       + sqljson_queryfuncs                         80 ms
ok 198       + sqljson_jsontable                          59 ms
# parallel group (18 tests):  prepare limit xml plancache conversion returning polymorphism copy2 rowtypes sequence with rangefuncs largeobject truncate temp domain plpgsql alter_table
ok 199       + plancache                                 123 ms
ok 200       + limit                                      89 ms
ok 201       + plpgsql                                   565 ms
ok 202       + copy2                                     198 ms
ok 203       + temp                                      291 ms
ok 204       + domain                                    338 ms
ok 205       + rangefuncs                                265 ms
ok 206       + prepare                                    37 ms
ok 207       + conversion                                126 ms
ok 208       + truncate                                  287 ms
ok 209       + alter_table                              1357 ms
ok 210       + sequence                                  216 ms
ok 211       + polymorphism                              196 ms
ok 212       + rowtypes                                  215 ms
ok 213       + returning                                 157 ms
ok 214       + largeobject                               266 ms
ok 215       + with                                      237 ms
ok 216       + xml                                       106 ms
# parallel group (18 tests):  numa compression_lz4 hash_part reloptions predicate explain partition_info compression memoize eager_aggregate partition_merge partition_split partition_aggregate partition_prune partition_join stats tuplesort indexing
ok 217       + partition_merge                           467 ms
ok 218       + partition_split                           537 ms
ok 219       + partition_join                            827 ms
ok 220       + partition_prune                           806 ms
ok 221       + reloptions                                 64 ms
ok 222       + hash_part                                  43 ms
ok 223       + indexing                                  963 ms
ok 224       + partition_aggregate                       723 ms
ok 225       + partition_info                             90 ms
ok 226       + tuplesort                                 850 ms
ok 227       + explain                                    78 ms
ok 228       + compression                               135 ms
ok 229       + compression_lz4                            19 ms
ok 230       + memoize                                   175 ms
ok 231       + stats                                     844 ms
ok 232       + predicate                                  63 ms
ok 233       + numa                                       10 ms
ok 234       + eager_aggregate                           281 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                  171 ms
ok 236       + event_trigger                             205 ms
ok 237       - event_trigger_login                        35 ms
ok 238       - fast_default                              177 ms
ok 239       - tablespace                                343 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
