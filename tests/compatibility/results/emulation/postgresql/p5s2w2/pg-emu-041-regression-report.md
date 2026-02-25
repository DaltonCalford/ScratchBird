Last updated: 2026-02-24

# PG-EMU-041 Regression Smoke Report

- Mode: `execute`
- Overall result: `pass`
- Command timeout: `1200s`

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
# using temp instance on port 58928 with PID 3889800
ok 1         - test_setup                                108 ms
# parallel group (20 tests):  oid text name varchar int2 int4 char txid money pg_lsn boolean float4 int8 regproc float8 enum bit uuid numeric rangetypes
ok 2         + boolean                                    21 ms
ok 3         + char                                       16 ms
ok 4         + name                                       11 ms
ok 5         + varchar                                    11 ms
ok 6         + text                                       10 ms
ok 7         + int2                                       13 ms
ok 8         + int4                                       14 ms
ok 9         + int8                                       21 ms
ok 10        + oid                                         9 ms
ok 11        + float4                                     20 ms
ok 12        + float8                                     25 ms
ok 13        + bit                                        32 ms
ok 14        + numeric                                    80 ms
ok 15        + txid                                       15 ms
ok 16        + uuid                                       40 ms
ok 17        + enum                                       30 ms
ok 18        + money                                      16 ms
ok 19        + rangetypes                                138 ms
ok 20        + pg_lsn                                     15 ms
ok 21        + regproc                                    21 ms
# parallel group (20 tests):  md5 line lseg path circle time point macaddr numerology timetz macaddr8 date inet interval strings multirangetypes box polygon timestamp timestamptz
ok 22        + strings                                    60 ms
ok 23        + md5                                         8 ms
ok 24        + numerology                                 17 ms
ok 25        + point                                      14 ms
ok 26        + lseg                                        9 ms
ok 27        + line                                        8 ms
ok 28        + box                                        77 ms
ok 29        + path                                        9 ms
ok 30        + polygon                                    87 ms
ok 31        + circle                                      9 ms
ok 32        + date                                       24 ms
ok 33        + time                                        9 ms
ok 34        + timetz                                     16 ms
ok 35        + timestamp                                 329 ms
ok 36        + timestamptz                               341 ms
ok 37        + interval                                   35 ms
ok 38        + inet                                       24 ms
ok 39        + macaddr                                    14 ms
ok 40        + macaddr8                                   15 ms
ok 41        + multirangetypes                            73 ms
# parallel group (19 tests):  comments unicode euc_kr misc_sanity oid8 pg_dependencies pg_ndistinct tstypes xid encoding expressions horology mvcc geometry type_sanity stats_import database regex opr_sanity
ok 42        + geometry                                   42 ms
ok 43        + horology                                   33 ms
ok 44        + tstypes                                    24 ms
ok 45        + regex                                     152 ms
ok 46        + type_sanity                                48 ms
ok 47        + opr_sanity                                153 ms
ok 48        + misc_sanity                                11 ms
ok 49        + comments                                    6 ms
ok 50        + expressions                                30 ms
ok 51        + unicode                                     7 ms
ok 52        + xid                                        27 ms
ok 53        + mvcc                                       33 ms
ok 54        + database                                  117 ms
ok 55        + stats_import                               88 ms
ok 56        + pg_ndistinct                               21 ms
ok 57        + pg_dependencies                            13 ms
ok 58        + oid8                                       10 ms
ok 59        + encoding                                   26 ms
ok 60        + euc_kr                                      7 ms
# parallel group (6 tests):  copyencoding copyselect copydml insert_conflict copy insert
ok 61        + copy                                       48 ms
ok 62        + copyselect                                 13 ms
ok 63        + copydml                                    14 ms
ok 64        + copyencoding                                9 ms
ok 65        + insert                                    111 ms
ok 66        + insert_conflict                            46 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_schema create_misc create_procedure create_table
ok 67        + create_function_c                           5 ms
ok 68        + create_misc                                20 ms
ok 69        + create_operator                             8 ms
ok 70        + create_procedure                           27 ms
ok 71        + create_table                              107 ms
ok 72        + create_type                                15 ms
ok 73        + create_schema                              19 ms
# parallel group (5 tests):  index_including index_including_gist create_view create_index_spgist create_index
ok 74        + create_index                              303 ms
ok 75        + create_index_spgist                       168 ms
ok 76        + create_view                               124 ms
ok 77        + index_including                            59 ms
ok 78        + index_including_gist                      109 ms
# parallel group (16 tests):  errors create_cast create_aggregate roleattributes hash_func select drop_if_exists typed_table create_function_sql create_am infinite_recurse vacuum constraints updatable_views triggers inherit
ok 79        + create_aggregate                           19 ms
ok 80        + create_function_sql                        59 ms
ok 81        + create_cast                                14 ms
ok 82        + constraints                               231 ms
ok 83        + triggers                                  301 ms
ok 84        + select                                     27 ms
ok 85        + inherit                                   371 ms
ok 86        + typed_table                                44 ms
ok 87        + vacuum                                    162 ms
ok 88        + drop_if_exists                             27 ms
ok 89        + updatable_views                           254 ms
ok 90        + roleattributes                             18 ms
ok 91        + create_am                                  64 ms
ok 92        + hash_func                                  19 ms
ok 93        + errors                                     10 ms
ok 94        + infinite_recurse                          109 ms
ok 95        - sanity_check                               40 ms
# parallel group (20 tests):  select_having select_implicit delete case select_distinct_on prepared_xacts random select_into namespace portals transactions select_distinct union arrays subselect update hash_index aggregates join btree_index
ok 96        + select_into                                38 ms
ok 97        + select_distinct                            66 ms
ok 98        + select_distinct_on                         26 ms
ok 99        + select_implicit                            17 ms
ok 100       + select_having                              13 ms
ok 101       + subselect                                 137 ms
ok 102       + union                                      69 ms
ok 103       + case                                       24 ms
ok 104       + join                                      296 ms
ok 105       + aggregates                                249 ms
ok 106       + transactions                               63 ms
ok 107       + random                                     31 ms
ok 108       + portals                                    55 ms
ok 109       + arrays                                    122 ms
ok 110       + btree_index                               473 ms
ok 111       + hash_index                                229 ms
ok 112       + update                                    155 ms
ok 113       + delete                                     19 ms
ok 114       + namespace                                  37 ms
ok 115       + prepared_xacts                             29 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock object_address password collate replica_identity groupingsets matview identity rowsecurity spgist gin generated_stored gist join_hash brin privileges
ok 116       + brin                                      573 ms
ok 117       + gin                                       323 ms
ok 118       + gist                                      374 ms
ok 119       + spgist                                    305 ms
ok 120       + privileges                                731 ms
ok 121       + init_privs                                 10 ms
ok 122       + security_label                             24 ms
ok 123       + collate                                    85 ms
ok 124       + matview                                   221 ms
ok 125       + lock                                       58 ms
ok 126       + replica_identity                          103 ms
ok 127       + rowsecurity                               302 ms
ok 128       + object_address                             76 ms
ok 129       + tablesample                                44 ms
ok 130       + groupingsets                              153 ms
ok 131       + drop_operator                              18 ms
ok 132       + password                                   80 ms
ok 133       + identity                                  232 ms
ok 134       + generated_stored                          330 ms
ok 135       + join_hash                                 563 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 38 ms
ok 137       + brin_multi                                 92 ms
# parallel group (20 tests):  nls async dbsize collate.utf8 alter_operator tid tsrf tidscan sysviews create_role misc_functions alter_generic misc tidrangescan incremental_sort merge collate.icu.utf8 create_table_like generated_virtual without_overlaps
ok 138       + create_table_like                         171 ms
ok 139       + alter_generic                              55 ms
ok 140       + alter_operator                             24 ms
ok 141       + misc                                       63 ms
ok 142       + async                                       8 ms
ok 143       + dbsize                                      9 ms
ok 144       + merge                                     137 ms
ok 145       + misc_functions                             52 ms
ok 146       + nls                                         6 ms
ok 147       + sysviews                                   34 ms
ok 148       + tsrf                                       26 ms
ok 149       + tid                                        26 ms
ok 150       + tidscan                                    26 ms
ok 151       + tidrangescan                               63 ms
ok 152       + collate.utf8                               23 ms
ok 153       + collate.icu.utf8                          168 ms
ok 154       + incremental_sort                           70 ms
ok 155       + create_role                                39 ms
ok 156       + without_overlaps                          239 ms
ok 157       + generated_virtual                         195 ms
# parallel group (8 tests):  collate.linux.utf8 collate.windows.win1252 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     152 ms
ok 159       + psql                                      232 ms
ok 160       + psql_crosstab                              13 ms
ok 161       + psql_pipeline                              16 ms
ok 162       + amutils                                     8 ms
ok 163       + stats_ext                                 668 ms
ok 164       + collate.linux.utf8                          5 ms
ok 165       + collate.windows.win1252                     5 ms
ok 166       - select_parallel                           352 ms
ok 167       - write_parallel                             45 ms
ok 168       - vacuum_parallel                            46 ms
ok 169       - maintain_every                             14 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               162 ms
ok 171       + subscription                               40 ms
# parallel group (18 tests):  portals_p2 xmlmap advisory_lock combocid tsdicts guc functional_deps equivclass dependency stats_rewrite select_views window bitmapops tsearch cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                               70 ms
ok 173       + portals_p2                                 14 ms
ok 174       + foreign_key                               421 ms
ok 175       + cluster                                   142 ms
ok 176       + dependency                                 50 ms
ok 177       + guc                                        35 ms
ok 178       + bitmapops                                 128 ms
ok 179       + combocid                                   25 ms
ok 180       + tsearch                                   134 ms
ok 181       + tsdicts                                    30 ms
ok 182       + foreign_data                              280 ms
ok 183       + window                                     87 ms
ok 184       + xmlmap                                     21 ms
ok 185       + functional_deps                            39 ms
ok 186       + advisory_lock                              22 ms
ok 187       + indirect_toast                            177 ms
ok 188       + equivclass                                 43 ms
ok 189       + stats_rewrite                              55 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson_jsontable sqljson json sqljson_queryfuncs jsonb_jsonpath jsonb
ok 190       + json                                       34 ms
ok 191       + jsonb                                     108 ms
ok 192       + json_encoding                               7 ms
ok 193       + jsonpath                                   18 ms
ok 194       + jsonpath_encoding                           6 ms
ok 195       + jsonb_jsonpath                             48 ms
ok 196       + sqljson                                    30 ms
ok 197       + sqljson_queryfuncs                         45 ms
ok 198       + sqljson_jsontable                          24 ms
# parallel group (18 tests):  prepare xml limit plancache returning conversion rowtypes sequence copy2 polymorphism rangefuncs with truncate largeobject temp domain plpgsql alter_table
ok 199       + plancache                                  61 ms
ok 200       + limit                                      53 ms
ok 201       + plpgsql                                   232 ms
ok 202       + copy2                                     104 ms
ok 203       + temp                                      131 ms
ok 204       + domain                                    148 ms
ok 205       + rangefuncs                                119 ms
ok 206       + prepare                                    16 ms
ok 207       + conversion                                 80 ms
ok 208       + truncate                                  127 ms
ok 209       + alter_table                               621 ms
ok 210       + sequence                                  100 ms
ok 211       + polymorphism                              104 ms
ok 212       + rowtypes                                   98 ms
ok 213       + returning                                  76 ms
ok 214       + largeobject                               128 ms
ok 215       + with                                      121 ms
ok 216       + xml                                        45 ms
# parallel group (18 tests):  numa compression_lz4 hash_part reloptions predicate explain partition_info compression memoize eager_aggregate partition_merge partition_split partition_aggregate partition_prune partition_join tuplesort indexing stats
ok 217       + partition_merge                           242 ms
ok 218       + partition_split                           278 ms
ok 219       + partition_join                            359 ms
ok 220       + partition_prune                           344 ms
ok 221       + reloptions                                 40 ms
ok 222       + hash_part                                  21 ms
ok 223       + indexing                                  407 ms
ok 224       + partition_aggregate                       330 ms
ok 225       + partition_info                             48 ms
ok 226       + tuplesort                                 396 ms
ok 227       + explain                                    46 ms
ok 228       + compression                                81 ms
ok 229       + compression_lz4                             8 ms
ok 230       + memoize                                    91 ms
ok 231       + stats                                     492 ms
ok 232       + predicate                                  41 ms
ok 233       + numa                                        6 ms
ok 234       + eager_aggregate                           131 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                   77 ms
ok 236       + event_trigger                              92 ms
ok 237       - event_trigger_login                        18 ms
ok 238       - fast_default                               45 ms
ok 239       - tablespace                                150 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
