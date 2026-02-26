Last updated: 2026-02-26

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
ake -j1  checkprep >>'<outside-tree-path>'/tmp_install/log/install.log 2>&1
PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  initdb --auth trust --no-sync --no-instructions --lc-messages=C --no-clean '<outside-tree-path>'/tmp_install/initdb-template >>'<outside-tree-path>'/tmp_install/log/initdb-template.log 2>&1
echo "# +++ regress check in src/test/regress +++" && PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  ../../../src/test/regress/pg_regress --temp-instance=./tmp_check --inputdir=<outside-tree-path> --bindir=     --dlpath=. --max-concurrent-tests=20  --schedule=<outside-tree-path>  
# +++ regress check in src/test/regress +++
# initializing database system by copying initdb template
# using temp instance on port 58928 with PID 935591
ok 1         - test_setup                                105 ms
# parallel group (20 tests):  int2 text varchar int4 char float4 txid oid name pg_lsn money boolean int8 regproc float8 enum bit uuid numeric rangetypes
ok 2         + boolean                                    19 ms
ok 3         + char                                       16 ms
ok 4         + name                                       16 ms
ok 5         + varchar                                    13 ms
ok 6         + text                                       12 ms
ok 7         + int2                                       10 ms
ok 8         + int4                                       12 ms
ok 9         + int8                                       19 ms
ok 10        + oid                                        15 ms
ok 11        + float4                                     15 ms
ok 12        + float8                                     24 ms
ok 13        + bit                                        34 ms
ok 14        + numeric                                   125 ms
ok 15        + txid                                       14 ms
ok 16        + uuid                                       37 ms
ok 17        + enum                                       29 ms
ok 18        + money                                      17 ms
ok 19        + rangetypes                                130 ms
ok 20        + pg_lsn                                     15 ms
ok 21        + regproc                                    21 ms
# parallel group (20 tests):  lseg md5 circle line path point numerology time macaddr macaddr8 timetz date inet interval strings box polygon multirangetypes timestamp timestamptz
ok 22        + strings                                    45 ms
ok 23        + md5                                         9 ms
ok 24        + numerology                                 16 ms
ok 25        + point                                      14 ms
ok 26        + lseg                                        7 ms
ok 27        + line                                       12 ms
ok 28        + box                                        81 ms
ok 29        + path                                       12 ms
ok 30        + polygon                                    86 ms
ok 31        + circle                                     10 ms
ok 32        + date                                       27 ms
ok 33        + time                                       16 ms
ok 34        + timetz                                     22 ms
ok 35        + timestamp                                 326 ms
ok 36        + timestamptz                               340 ms
ok 37        + interval                                   32 ms
ok 38        + inet                                       28 ms
ok 39        + macaddr                                    17 ms
ok 40        + macaddr8                                   17 ms
ok 41        + multirangetypes                           106 ms
# parallel group (19 tests):  comments euc_kr misc_sanity unicode oid8 tstypes pg_ndistinct pg_dependencies encoding expressions xid horology mvcc geometry type_sanity stats_import database opr_sanity regex
ok 42        + geometry                                   43 ms
ok 43        + horology                                   35 ms
ok 44        + tstypes                                    15 ms
ok 45        + regex                                     171 ms
ok 46        + type_sanity                                47 ms
ok 47        + opr_sanity                                145 ms
ok 48        + misc_sanity                                12 ms
ok 49        + comments                                    9 ms
ok 50        + expressions                                27 ms
ok 51        + unicode                                    13 ms
ok 52        + xid                                        31 ms
ok 53        + mvcc                                       37 ms
ok 54        + database                                  120 ms
ok 55        + stats_import                               84 ms
ok 56        + pg_ndistinct                               19 ms
ok 57        + pg_dependencies                            19 ms
ok 58        + oid8                                       13 ms
ok 59        + encoding                                   25 ms
ok 60        + euc_kr                                     10 ms
# parallel group (6 tests):  copyencoding copyselect copydml copy insert_conflict insert
ok 61        + copy                                       55 ms
ok 62        + copyselect                                 14 ms
ok 63        + copydml                                    15 ms
ok 64        + copyencoding                                9 ms
ok 65        + insert                                    100 ms
ok 66        + insert_conflict                            62 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_procedure create_misc create_schema create_table
ok 67        + create_function_c                           5 ms
ok 68        + create_misc                                19 ms
ok 69        + create_operator                            10 ms
ok 70        + create_procedure                           18 ms
ok 71        + create_table                              122 ms
ok 72        + create_type                                10 ms
ok 73        + create_schema                              19 ms
# parallel group (5 tests):  index_including index_including_gist create_view create_index_spgist create_index
ok 74        + create_index                              255 ms
ok 75        + create_index_spgist                       183 ms
ok 76        + create_view                               109 ms
ok 77        + index_including                            80 ms
ok 78        + index_including_gist                       89 ms
# parallel group (16 tests):  create_cast roleattributes errors hash_func create_aggregate drop_if_exists select typed_table create_function_sql create_am infinite_recurse vacuum updatable_views constraints triggers inherit
ok 79        + create_aggregate                           19 ms
ok 80        + create_function_sql                        57 ms
ok 81        + create_cast                                11 ms
ok 82        + constraints                               255 ms
ok 83        + triggers                                  353 ms
ok 84        + select                                     26 ms
ok 85        + inherit                                   353 ms
ok 86        + typed_table                                46 ms
ok 87        + vacuum                                    152 ms
ok 88        + drop_if_exists                             24 ms
ok 89        + updatable_views                           231 ms
ok 90        + roleattributes                             14 ms
ok 91        + create_am                                  60 ms
ok 92        + hash_func                                  17 ms
ok 93        + errors                                     13 ms
ok 94        + infinite_recurse                          103 ms
ok 95        - sanity_check                               41 ms
# parallel group (20 tests):  select_distinct_on select_implicit select_having delete case namespace random prepared_xacts select_into portals union transactions select_distinct arrays subselect update hash_index aggregates join btree_index
ok 96        + select_into                                45 ms
ok 97        + select_distinct                            67 ms
ok 98        + select_distinct_on                         21 ms
ok 99        + select_implicit                            21 ms
ok 100       + select_having                              21 ms
ok 101       + subselect                                 127 ms
ok 102       + union                                      62 ms
ok 103       + case                                       31 ms
ok 104       + join                                      295 ms
ok 105       + aggregates                                231 ms
ok 106       + transactions                               64 ms
ok 107       + random                                     36 ms
ok 108       + portals                                    50 ms
ok 109       + arrays                                    114 ms
ok 110       + btree_index                               480 ms
ok 111       + hash_index                                223 ms
ok 112       + update                                    157 ms
ok 113       + delete                                     18 ms
ok 114       + namespace                                  33 ms
ok 115       + prepared_xacts                             38 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample object_address lock password collate replica_identity groupingsets matview identity rowsecurity spgist generated_stored gist gin brin join_hash privileges
ok 116       + brin                                      561 ms
ok 117       + gin                                       353 ms
ok 118       + gist                                      337 ms
ok 119       + spgist                                    281 ms
ok 120       + privileges                                711 ms
ok 121       + init_privs                                 13 ms
ok 122       + security_label                             27 ms
ok 123       + collate                                    99 ms
ok 124       + matview                                   212 ms
ok 125       + lock                                       64 ms
ok 126       + replica_identity                          119 ms
ok 127       + rowsecurity                               277 ms
ok 128       + object_address                             62 ms
ok 129       + tablesample                                33 ms
ok 130       + groupingsets                              135 ms
ok 131       + drop_operator                              19 ms
ok 132       + password                                   82 ms
ok 133       + identity                                  218 ms
ok 134       + generated_stored                          298 ms
ok 135       + join_hash                                 557 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 35 ms
ok 137       + brin_multi                                 85 ms
# parallel group (20 tests):  nls async dbsize collate.utf8 tid tsrf alter_operator tidscan sysviews create_role misc_functions misc alter_generic tidrangescan incremental_sort merge create_table_like collate.icu.utf8 generated_virtual without_overlaps
ok 138       + create_table_like                         158 ms
ok 139       + alter_generic                              60 ms
ok 140       + alter_operator                             26 ms
ok 141       + misc                                       59 ms
ok 142       + async                                       7 ms
ok 143       + dbsize                                     10 ms
ok 144       + merge                                     135 ms
ok 145       + misc_functions                             51 ms
ok 146       + nls                                         6 ms
ok 147       + sysviews                                   35 ms
ok 148       + tsrf                                       25 ms
ok 149       + tid                                        23 ms
ok 150       + tidscan                                    26 ms
ok 151       + tidrangescan                               64 ms
ok 152       + collate.utf8                               18 ms
ok 153       + collate.icu.utf8                          170 ms
ok 154       + incremental_sort                           67 ms
ok 155       + create_role                                40 ms
ok 156       + without_overlaps                          262 ms
ok 157       + generated_virtual                         199 ms
# parallel group (8 tests):  collate.windows.win1252 collate.linux.utf8 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     153 ms
ok 159       + psql                                      162 ms
ok 160       + psql_crosstab                              11 ms
ok 161       + psql_pipeline                              14 ms
ok 162       + amutils                                     8 ms
ok 163       + stats_ext                                 623 ms
ok 164       + collate.linux.utf8                          4 ms
ok 165       + collate.windows.win1252                     4 ms
ok 166       - select_parallel                           324 ms
ok 167       - write_parallel                             41 ms
ok 168       - vacuum_parallel                            46 ms
ok 169       - maintain_every                             11 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               175 ms
ok 171       + subscription                               40 ms
# parallel group (18 tests):  portals_p2 xmlmap advisory_lock combocid tsdicts guc functional_deps equivclass dependency select_views stats_rewrite window cluster bitmapops tsearch indirect_toast foreign_data foreign_key
ok 172       + select_views                               62 ms
ok 173       + portals_p2                                 17 ms
ok 174       + foreign_key                               464 ms
ok 175       + cluster                                   141 ms
ok 176       + dependency                                 49 ms
ok 177       + guc                                        38 ms
ok 178       + bitmapops                                 142 ms
ok 179       + combocid                                   29 ms
ok 180       + tsearch                                   144 ms
ok 181       + tsdicts                                    29 ms
ok 182       + foreign_data                              251 ms
ok 183       + window                                    119 ms
ok 184       + xmlmap                                     16 ms
ok 185       + functional_deps                            39 ms
ok 186       + advisory_lock                              18 ms
ok 187       + indirect_toast                            161 ms
ok 188       + equivclass                                 39 ms
ok 189       + stats_rewrite                              61 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson sqljson_jsontable jsonb_jsonpath sqljson_queryfuncs json jsonb
ok 190       + json                                       44 ms
ok 191       + jsonb                                     107 ms
ok 192       + json_encoding                               7 ms
ok 193       + jsonpath                                   16 ms
ok 194       + jsonpath_encoding                           6 ms
ok 195       + jsonb_jsonpath                             36 ms
ok 196       + sqljson                                    28 ms
ok 197       + sqljson_queryfuncs                         40 ms
ok 198       + sqljson_jsontable                          31 ms
# parallel group (18 tests):  prepare limit plancache xml conversion returning polymorphism rowtypes copy2 sequence largeobject truncate temp with rangefuncs domain plpgsql alter_table
ok 199       + plancache                                  54 ms
ok 200       + limit                                      49 ms
ok 201       + plpgsql                                   221 ms
ok 202       + copy2                                     102 ms
ok 203       + temp                                      138 ms
ok 204       + domain                                    156 ms
ok 205       + rangefuncs                                146 ms
ok 206       + prepare                                    21 ms
ok 207       + conversion                                 68 ms
ok 208       + truncate                                  131 ms
ok 209       + alter_table                               587 ms
ok 210       + sequence                                  105 ms
ok 211       + polymorphism                               94 ms
ok 212       + rowtypes                                   95 ms
ok 213       + returning                                  79 ms
ok 214       + largeobject                               124 ms
ok 215       + with                                      139 ms
ok 216       + xml                                        55 ms
# parallel group (18 tests):  numa compression_lz4 hash_part reloptions predicate partition_info explain compression memoize eager_aggregate partition_merge partition_split partition_aggregate partition_join partition_prune tuplesort indexing stats
ok 217       + partition_merge                           235 ms
ok 218       + partition_split                           271 ms
ok 219       + partition_join                            372 ms
ok 220       + partition_prune                           383 ms
ok 221       + reloptions                                 36 ms
ok 222       + hash_part                                  26 ms
ok 223       + indexing                                  436 ms
ok 224       + partition_aggregate                       320 ms
ok 225       + partition_info                             55 ms
ok 226       + tuplesort                                 383 ms
ok 227       + explain                                    57 ms
ok 228       + compression                                77 ms
ok 229       + compression_lz4                            10 ms
ok 230       + memoize                                    92 ms
ok 231       + stats                                     484 ms
ok 232       + predicate                                  46 ms
ok 233       + numa                                        6 ms
ok 234       + eager_aggregate                           135 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                   75 ms
ok 236       + event_trigger                              88 ms
ok 237       - event_trigger_login                        16 ms
ok 238       - fast_default                               58 ms
ok 239       - tablespace                                167 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
