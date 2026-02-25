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
# using temp instance on port 58928 with PID 3928180
ok 1         - test_setup                                102 ms
# parallel group (20 tests):  oid char txid int2 pg_lsn name varchar text int4 float4 regproc money int8 boolean float8 enum bit uuid numeric rangetypes
ok 2         + boolean                                    25 ms
ok 3         + char                                       10 ms
ok 4         + name                                       14 ms
ok 5         + varchar                                    14 ms
ok 6         + text                                       16 ms
ok 7         + int2                                       12 ms
ok 8         + int4                                       16 ms
ok 9         + int8                                       23 ms
ok 10        + oid                                         9 ms
ok 11        + float4                                     19 ms
ok 12        + float8                                     28 ms
ok 13        + bit                                        37 ms
ok 14        + numeric                                   121 ms
ok 15        + txid                                        9 ms
ok 16        + uuid                                       49 ms
ok 17        + enum                                       33 ms
ok 18        + money                                      20 ms
ok 19        + rangetypes                                135 ms
ok 20        + pg_lsn                                     12 ms
ok 21        + regproc                                    18 ms
# parallel group (20 tests):  md5 line lseg path circle time timetz point macaddr macaddr8 numerology inet date interval strings box polygon multirangetypes timestamp timestamptz
ok 22        + strings                                    57 ms
ok 23        + md5                                         8 ms
ok 24        + numerology                                 21 ms
ok 25        + point                                      16 ms
ok 26        + lseg                                       11 ms
ok 27        + line                                        8 ms
ok 28        + box                                        76 ms
ok 29        + path                                       13 ms
ok 30        + polygon                                    79 ms
ok 31        + circle                                     14 ms
ok 32        + date                                       28 ms
ok 33        + time                                       15 ms
ok 34        + timetz                                     15 ms
ok 35        + timestamp                                 325 ms
ok 36        + timestamptz                               340 ms
ok 37        + interval                                   35 ms
ok 38        + inet                                       27 ms
ok 39        + macaddr                                    18 ms
ok 40        + macaddr8                                   20 ms
ok 41        + multirangetypes                            99 ms
# parallel group (19 tests):  unicode euc_kr pg_ndistinct comments misc_sanity pg_dependencies tstypes oid8 xid encoding expressions mvcc horology geometry type_sanity stats_import database opr_sanity regex
ok 42        + geometry                                   40 ms
ok 43        + horology                                   37 ms
ok 44        + tstypes                                    24 ms
ok 45        + regex                                     179 ms
ok 46        + type_sanity                                45 ms
ok 47        + opr_sanity                                135 ms
ok 48        + misc_sanity                                10 ms
ok 49        + comments                                   10 ms
ok 50        + expressions                                30 ms
ok 51        + unicode                                     6 ms
ok 52        + xid                                        25 ms
ok 53        + mvcc                                       33 ms
ok 54        + database                                  118 ms
ok 55        + stats_import                               79 ms
ok 56        + pg_ndistinct                                9 ms
ok 57        + pg_dependencies                            12 ms
ok 58        + oid8                                       22 ms
ok 59        + encoding                                   26 ms
ok 60        + euc_kr                                      4 ms
# parallel group (6 tests):  copyencoding copyselect copydml insert_conflict copy insert
ok 61        + copy                                       52 ms
ok 62        + copyselect                                 12 ms
ok 63        + copydml                                    14 ms
ok 64        + copyencoding                                8 ms
ok 65        + insert                                    115 ms
ok 66        + insert_conflict                            40 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_schema create_misc create_procedure create_table
ok 67        + create_function_c                           5 ms
ok 68        + create_misc                                19 ms
ok 69        + create_operator                            11 ms
ok 70        + create_procedure                           26 ms
ok 71        + create_table                              111 ms
ok 72        + create_type                                12 ms
ok 73        + create_schema                              18 ms
# parallel group (5 tests):  index_including index_including_gist create_view create_index_spgist create_index
ok 74        + create_index                              302 ms
ok 75        + create_index_spgist                       170 ms
ok 76        + create_view                               115 ms
ok 77        + index_including                            81 ms
ok 78        + index_including_gist                       88 ms
# parallel group (16 tests):  create_cast errors create_aggregate roleattributes hash_func select drop_if_exists create_function_sql typed_table create_am infinite_recurse vacuum updatable_views constraints triggers inherit
ok 79        + create_aggregate                           16 ms
ok 80        + create_function_sql                        45 ms
ok 81        + create_cast                                11 ms
ok 82        + constraints                               260 ms
ok 83        + triggers                                  339 ms
ok 84        + select                                     20 ms
ok 85        + inherit                                   395 ms
ok 86        + typed_table                                44 ms
ok 87        + vacuum                                    135 ms
ok 88        + drop_if_exists                             28 ms
ok 89        + updatable_views                           218 ms
ok 90        + roleattributes                             16 ms
ok 91        + create_am                                  55 ms
ok 92        + hash_func                                  16 ms
ok 93        + errors                                     13 ms
ok 94        + infinite_recurse                          109 ms
ok 95        - sanity_check                               39 ms
# parallel group (20 tests):  select_having case delete select_distinct_on random select_implicit namespace prepared_xacts select_into portals union transactions select_distinct arrays subselect update hash_index aggregates join btree_index
ok 96        + select_into                                48 ms
ok 97        + select_distinct                            73 ms
ok 98        + select_distinct_on                         28 ms
ok 99        + select_implicit                            34 ms
ok 100       + select_having                              19 ms
ok 101       + subselect                                 136 ms
ok 102       + union                                      63 ms
ok 103       + case                                       19 ms
ok 104       + join                                      278 ms
ok 105       + aggregates                                246 ms
ok 106       + transactions                               67 ms
ok 107       + random                                     31 ms
ok 108       + portals                                    58 ms
ok 109       + arrays                                    120 ms
ok 110       + btree_index                               465 ms
ok 111       + hash_index                                209 ms
ok 112       + update                                    172 ms
ok 113       + delete                                     22 ms
ok 114       + namespace                                  35 ms
ok 115       + prepared_xacts                             39 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock object_address password collate replica_identity groupingsets identity matview generated_stored rowsecurity spgist gin gist join_hash brin privileges
ok 116       + brin                                      577 ms
ok 117       + gin                                       382 ms
ok 118       + gist                                      397 ms
ok 119       + spgist                                    319 ms
ok 120       + privileges                                709 ms
ok 121       + init_privs                                  8 ms
ok 122       + security_label                             29 ms
ok 123       + collate                                   112 ms
ok 124       + matview                                   230 ms
ok 125       + lock                                       57 ms
ok 126       + replica_identity                          135 ms
ok 127       + rowsecurity                               306 ms
ok 128       + object_address                             72 ms
ok 129       + tablesample                                45 ms
ok 130       + groupingsets                              157 ms
ok 131       + drop_operator                              13 ms
ok 132       + password                                   83 ms
ok 133       + identity                                  227 ms
ok 134       + generated_stored                          297 ms
ok 135       + join_hash                                 565 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 43 ms
ok 137       + brin_multi                                 92 ms
# parallel group (20 tests):  async nls dbsize collate.utf8 alter_operator tidscan tsrf tid sysviews create_role alter_generic misc_functions misc tidrangescan incremental_sort merge collate.icu.utf8 create_table_like generated_virtual without_overlaps
ok 138       + create_table_like                         173 ms
ok 139       + alter_generic                              57 ms
ok 140       + alter_operator                             26 ms
ok 141       + misc                                       62 ms
ok 142       + async                                       6 ms
ok 143       + dbsize                                      8 ms
ok 144       + merge                                     132 ms
ok 145       + misc_functions                             58 ms
ok 146       + nls                                         7 ms
ok 147       + sysviews                                   36 ms
ok 148       + tsrf                                       28 ms
ok 149       + tid                                        29 ms
ok 150       + tidscan                                    26 ms
ok 151       + tidrangescan                               68 ms
ok 152       + collate.utf8                               22 ms
ok 153       + collate.icu.utf8                          159 ms
ok 154       + incremental_sort                           74 ms
ok 155       + create_role                                35 ms
ok 156       + without_overlaps                          270 ms
ok 157       + generated_virtual                         224 ms
# parallel group (8 tests):  collate.windows.win1252 collate.linux.utf8 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     145 ms
ok 159       + psql                                      264 ms
ok 160       + psql_crosstab                              14 ms
ok 161       + psql_pipeline                              15 ms
ok 162       + amutils                                     9 ms
ok 163       + stats_ext                                 595 ms
ok 164       + collate.linux.utf8                          5 ms
ok 165       + collate.windows.win1252                     5 ms
ok 166       - select_parallel                           332 ms
ok 167       - write_parallel                             43 ms
ok 168       - vacuum_parallel                            39 ms
ok 169       - maintain_every                             12 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               156 ms
ok 171       + subscription                               42 ms
# parallel group (18 tests):  portals_p2 advisory_lock combocid tsdicts xmlmap equivclass guc functional_deps dependency select_views stats_rewrite window tsearch bitmapops cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                               68 ms
ok 173       + portals_p2                                 13 ms
ok 174       + foreign_key                               434 ms
ok 175       + cluster                                   155 ms
ok 176       + dependency                                 53 ms
ok 177       + guc                                        42 ms
ok 178       + bitmapops                                 135 ms
ok 179       + combocid                                   22 ms
ok 180       + tsearch                                   132 ms
ok 181       + tsdicts                                    21 ms
ok 182       + foreign_data                              266 ms
ok 183       + window                                    101 ms
ok 184       + xmlmap                                     22 ms
ok 185       + functional_deps                            44 ms
ok 186       + advisory_lock                              15 ms
ok 187       + indirect_toast                            153 ms
ok 188       + equivclass                                 40 ms
ok 189       + stats_rewrite                              68 ms
# parallel group (9 tests):  json_encoding jsonpath_encoding jsonpath sqljson_jsontable sqljson json sqljson_queryfuncs jsonb_jsonpath jsonb
ok 190       + json                                       36 ms
ok 191       + jsonb                                      95 ms
ok 192       + json_encoding                               6 ms
ok 193       + jsonpath                                   15 ms
ok 194       + jsonpath_encoding                           6 ms
ok 195       + jsonb_jsonpath                             46 ms
ok 196       + sqljson                                    28 ms
ok 197       + sqljson_queryfuncs                         44 ms
ok 198       + sqljson_jsontable                          26 ms
# parallel group (18 tests):  prepare limit xml conversion plancache returning polymorphism rowtypes sequence copy2 temp largeobject rangefuncs truncate with domain plpgsql alter_table
ok 199       + plancache                                  58 ms
ok 200       + limit                                      42 ms
ok 201       + plpgsql                                   250 ms
ok 202       + copy2                                     101 ms
ok 203       + temp                                      115 ms
ok 204       + domain                                    139 ms
ok 205       + rangefuncs                                121 ms
ok 206       + prepare                                    16 ms
ok 207       + conversion                                 57 ms
ok 208       + truncate                                  129 ms
ok 209       + alter_table                               507 ms
ok 210       + sequence                                   97 ms
ok 211       + polymorphism                               90 ms
ok 212       + rowtypes                                   94 ms
ok 213       + returning                                  71 ms
ok 214       + largeobject                               117 ms
ok 215       + with                                      130 ms
ok 216       + xml                                        48 ms
# parallel group (18 tests):  numa compression_lz4 hash_part reloptions partition_info predicate explain compression memoize eager_aggregate partition_merge partition_split tuplesort partition_aggregate partition_join partition_prune indexing stats
ok 217       + partition_merge                           255 ms
ok 218       + partition_split                           287 ms
ok 219       + partition_join                            356 ms
ok 220       + partition_prune                           394 ms
ok 221       + reloptions                                 48 ms
ok 222       + hash_part                                  28 ms
ok 223       + indexing                                  408 ms
ok 224       + partition_aggregate                       343 ms
ok 225       + partition_info                             47 ms
ok 226       + tuplesort                                 328 ms
ok 227       + explain                                    50 ms
ok 228       + compression                                83 ms
ok 229       + compression_lz4                            13 ms
ok 230       + memoize                                    99 ms
ok 231       + stats                                     512 ms
ok 232       + predicate                                  49 ms
ok 233       + numa                                        8 ms
ok 234       + eager_aggregate                           150 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                   74 ms
ok 236       + event_trigger                              86 ms
ok 237       - event_trigger_login                        17 ms
ok 238       - fast_default                               57 ms
ok 239       - tablespace                                133 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
