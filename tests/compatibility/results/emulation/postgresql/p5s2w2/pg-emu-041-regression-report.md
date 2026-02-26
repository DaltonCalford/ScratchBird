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
ke -j1  checkprep >>'<outside-tree-path>'/tmp_install/log/install.log 2>&1
PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  initdb --auth trust --no-sync --no-instructions --lc-messages=C --no-clean '<outside-tree-path>'/tmp_install/initdb-template >>'<outside-tree-path>'/tmp_install/log/initdb-template.log 2>&1
echo "# +++ regress check in src/test/regress +++" && PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  ../../../src/test/regress/pg_regress --temp-instance=./tmp_check --inputdir=<outside-tree-path> --bindir=     --dlpath=. --max-concurrent-tests=20  --schedule=<outside-tree-path>  
# +++ regress check in src/test/regress +++
# initializing database system by copying initdb template
# using temp instance on port 58928 with PID 1023041
ok 1         - test_setup                                111 ms
# parallel group (20 tests):  int4 varchar int2 name text oid pg_lsn regproc boolean money char txid int8 float8 bit float4 enum uuid numeric rangetypes
ok 2         + boolean                                    18 ms
ok 3         + char                                       20 ms
ok 4         + name                                       11 ms
ok 5         + varchar                                    10 ms
ok 6         + text                                       11 ms
ok 7         + int2                                       10 ms
ok 8         + int4                                       10 ms
ok 9         + int8                                       20 ms
ok 10        + oid                                        15 ms
ok 11        + float4                                     34 ms
ok 12        + float8                                     24 ms
ok 13        + bit                                        29 ms
ok 14        + numeric                                   109 ms
ok 15        + txid                                       19 ms
ok 16        + uuid                                       44 ms
ok 17        + enum                                       36 ms
ok 18        + money                                      18 ms
ok 19        + rangetypes                                152 ms
ok 20        + pg_lsn                                     14 ms
ok 21        + regproc                                    15 ms
# parallel group (20 tests):  md5 path circle time point line lseg macaddr timetz numerology macaddr8 inet date interval strings multirangetypes polygon box timestamp timestamptz
ok 22        + strings                                    68 ms
ok 23        + md5                                         6 ms
ok 24        + numerology                                 22 ms
ok 25        + point                                      13 ms
ok 26        + lseg                                       16 ms
ok 27        + line                                       13 ms
ok 28        + box                                        87 ms
ok 29        + path                                       11 ms
ok 30        + polygon                                    85 ms
ok 31        + circle                                     11 ms
ok 32        + date                                       31 ms
ok 33        + time                                       11 ms
ok 34        + timetz                                     20 ms
ok 35        + timestamp                                 321 ms
ok 36        + timestamptz                               335 ms
ok 37        + interval                                   46 ms
ok 38        + inet                                       30 ms
ok 39        + macaddr                                    18 ms
ok 40        + macaddr8                                   22 ms
ok 41        + multirangetypes                            80 ms
# parallel group (19 tests):  unicode comments euc_kr misc_sanity tstypes pg_ndistinct pg_dependencies encoding oid8 xid expressions geometry mvcc horology type_sanity stats_import opr_sanity database regex
ok 42        + geometry                                   41 ms
ok 43        + horology                                   50 ms
ok 44        + tstypes                                    20 ms
ok 45        + regex                                     186 ms
ok 46        + type_sanity                                55 ms
ok 47        + opr_sanity                                110 ms
ok 48        + misc_sanity                                15 ms
ok 49        + comments                                    9 ms
ok 50        + expressions                                32 ms
ok 51        + unicode                                     8 ms
ok 52        + xid                                        31 ms
ok 53        + mvcc                                       40 ms
ok 54        + database                                  134 ms
ok 55        + stats_import                               84 ms
ok 56        + pg_ndistinct                               19 ms
ok 57        + pg_dependencies                            21 ms
ok 58        + oid8                                       26 ms
ok 59        + encoding                                   25 ms
ok 60        + euc_kr                                      8 ms
# parallel group (6 tests):  copyencoding copyselect copydml copy insert_conflict insert
ok 61        + copy                                       49 ms
ok 62        + copyselect                                 13 ms
ok 63        + copydml                                    13 ms
ok 64        + copyencoding                                8 ms
ok 65        + insert                                    129 ms
ok 66        + insert_conflict                            51 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_misc create_schema create_procedure create_table
ok 67        + create_function_c                           5 ms
ok 68        + create_misc                                18 ms
ok 69        + create_operator                             8 ms
ok 70        + create_procedure                           28 ms
ok 71        + create_table                              132 ms
ok 72        + create_type                                10 ms
ok 73        + create_schema                              19 ms
# parallel group (5 tests):  index_including index_including_gist create_view create_index_spgist create_index
ok 74        + create_index                              271 ms
ok 75        + create_index_spgist                       151 ms
ok 76        + create_view                               131 ms
ok 77        + index_including                            82 ms
ok 78        + index_including_gist                       84 ms
# parallel group (16 tests):  create_cast errors roleattributes hash_func create_aggregate drop_if_exists select typed_table create_am create_function_sql infinite_recurse vacuum updatable_views constraints triggers inherit
ok 79        + create_aggregate                           21 ms
ok 80        + create_function_sql                        66 ms
ok 81        + create_cast                                12 ms
ok 82        + constraints                               269 ms
ok 83        + triggers                                  344 ms
ok 84        + select                                     43 ms
ok 85        + inherit                                   376 ms
ok 86        + typed_table                                45 ms
ok 87        + vacuum                                    165 ms
ok 88        + drop_if_exists                             24 ms
ok 89        + updatable_views                           253 ms
ok 90        + roleattributes                             15 ms
ok 91        + create_am                                  64 ms
ok 92        + hash_func                                  17 ms
ok 93        + errors                                     11 ms
ok 94        + infinite_recurse                          108 ms
ok 95        - sanity_check                               38 ms
# parallel group (20 tests):  select_having case delete select_implicit select_distinct_on prepared_xacts namespace select_into random portals transactions union select_distinct arrays subselect update aggregates hash_index join btree_index
ok 96        + select_into                                42 ms
ok 97        + select_distinct                            73 ms
ok 98        + select_distinct_on                         29 ms
ok 99        + select_implicit                            22 ms
ok 100       + select_having                              13 ms
ok 101       + subselect                                 125 ms
ok 102       + union                                      70 ms
ok 103       + case                                       20 ms
ok 104       + join                                      284 ms
ok 105       + aggregates                                241 ms
ok 106       + transactions                               64 ms
ok 107       + random                                     42 ms
ok 108       + portals                                    62 ms
ok 109       + arrays                                    118 ms
ok 110       + btree_index                               450 ms
ok 111       + hash_index                                258 ms
ok 112       + update                                    141 ms
ok 113       + delete                                     19 ms
ok 114       + namespace                                  33 ms
ok 115       + prepared_xacts                             32 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock password object_address collate replica_identity groupingsets matview identity rowsecurity spgist generated_stored gin gist join_hash brin privileges
ok 116       + brin                                      560 ms
ok 117       + gin                                       344 ms
ok 118       + gist                                      366 ms
ok 119       + spgist                                    297 ms
ok 120       + privileges                                703 ms
ok 121       + init_privs                                  8 ms
ok 122       + security_label                             40 ms
ok 123       + collate                                    98 ms
ok 124       + matview                                   220 ms
ok 125       + lock                                       70 ms
ok 126       + replica_identity                          122 ms
ok 127       + rowsecurity                               288 ms
ok 128       + object_address                             80 ms
ok 129       + tablesample                                50 ms
ok 130       + groupingsets                              145 ms
ok 131       + drop_operator                              14 ms
ok 132       + password                                   78 ms
ok 133       + identity                                  223 ms
ok 134       + generated_stored                          299 ms
ok 135       + join_hash                                 556 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 39 ms
ok 137       + brin_multi                                 76 ms
# parallel group (20 tests):  async nls dbsize alter_operator tsrf collate.utf8 tid tidscan sysviews create_role misc_functions alter_generic tidrangescan incremental_sort misc merge create_table_like collate.icu.utf8 generated_virtual without_overlaps
ok 138       + create_table_like                         178 ms
ok 139       + alter_generic                              54 ms
ok 140       + alter_operator                             18 ms
ok 141       + misc                                       71 ms
ok 142       + async                                       6 ms
ok 143       + dbsize                                     10 ms
ok 144       + merge                                     128 ms
ok 145       + misc_functions                             52 ms
ok 146       + nls                                         9 ms
ok 147       + sysviews                                   35 ms
ok 148       + tsrf                                       20 ms
ok 149       + tid                                        30 ms
ok 150       + tidscan                                    30 ms
ok 151       + tidrangescan                               53 ms
ok 152       + collate.utf8                               27 ms
ok 153       + collate.icu.utf8                          177 ms
ok 154       + incremental_sort                           69 ms
ok 155       + create_role                                44 ms
ok 156       + without_overlaps                          250 ms
ok 157       + generated_virtual                         201 ms
# parallel group (8 tests):  collate.linux.utf8 collate.windows.win1252 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     136 ms
ok 159       + psql                                      187 ms
ok 160       + psql_crosstab                              12 ms
ok 161       + psql_pipeline                              13 ms
ok 162       + amutils                                     9 ms
ok 163       + stats_ext                                 636 ms
ok 164       + collate.linux.utf8                          5 ms
ok 165       + collate.windows.win1252                     5 ms
ok 166       - select_parallel                           325 ms
ok 167       - write_parallel                             36 ms
ok 168       - vacuum_parallel                            42 ms
ok 169       - maintain_every                             11 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               162 ms
ok 171       + subscription                               39 ms
# parallel group (18 tests):  portals_p2 advisory_lock xmlmap combocid tsdicts guc equivclass functional_deps dependency select_views stats_rewrite window bitmapops tsearch cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                               62 ms
ok 173       + portals_p2                                 17 ms
ok 174       + foreign_key                               388 ms
ok 175       + cluster                                   147 ms
ok 176       + dependency                                 48 ms
ok 177       + guc                                        36 ms
ok 178       + bitmapops                                 123 ms
ok 179       + combocid                                   20 ms
ok 180       + tsearch                                   136 ms
ok 181       + tsdicts                                    23 ms
ok 182       + foreign_data                              256 ms
ok 183       + window                                    109 ms
ok 184       + xmlmap                                     17 ms
ok 185       + functional_deps                            43 ms
ok 186       + advisory_lock                              15 ms
ok 187       + indirect_toast                            179 ms
ok 188       + equivclass                                 40 ms
ok 189       + stats_rewrite                              67 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson_jsontable sqljson json sqljson_queryfuncs jsonb_jsonpath jsonb
ok 190       + json                                       38 ms
ok 191       + jsonb                                     105 ms
ok 192       + json_encoding                               7 ms
ok 193       + jsonpath                                   15 ms
ok 194       + jsonpath_encoding                           6 ms
ok 195       + jsonb_jsonpath                             47 ms
ok 196       + sqljson                                    30 ms
ok 197       + sqljson_queryfuncs                         38 ms
ok 198       + sqljson_jsontable                          27 ms
# parallel group (18 tests):  prepare xml limit plancache conversion returning polymorphism sequence rowtypes largeobject with rangefuncs temp copy2 truncate domain plpgsql alter_table
ok 199       + plancache                                  66 ms
ok 200       + limit                                      49 ms
ok 201       + plpgsql                                   222 ms
ok 202       + copy2                                     130 ms
ok 203       + temp                                      128 ms
ok 204       + domain                                    153 ms
ok 205       + rangefuncs                                124 ms
ok 206       + prepare                                    27 ms
ok 207       + conversion                                 66 ms
ok 208       + truncate                                  141 ms
ok 209       + alter_table                               549 ms
ok 210       + sequence                                  107 ms
ok 211       + polymorphism                               87 ms
ok 212       + rowtypes                                  110 ms
ok 213       + returning                                  80 ms
ok 214       + largeobject                               111 ms
ok 215       + with                                      116 ms
ok 216       + xml                                        45 ms
# parallel group (18 tests):  compression_lz4 numa hash_part reloptions explain predicate partition_info compression memoize eager_aggregate partition_merge partition_split partition_aggregate partition_prune partition_join tuplesort indexing stats
ok 217       + partition_merge                           211 ms
ok 218       + partition_split                           251 ms
ok 219       + partition_join                            346 ms
ok 220       + partition_prune                           340 ms
ok 221       + reloptions                                 36 ms
ok 222       + hash_part                                  21 ms
ok 223       + indexing                                  399 ms
ok 224       + partition_aggregate                       337 ms
ok 225       + partition_info                             49 ms
ok 226       + tuplesort                                 390 ms
ok 227       + explain                                    45 ms
ok 228       + compression                                74 ms
ok 229       + compression_lz4                             9 ms
ok 230       + memoize                                    92 ms
ok 231       + stats                                     490 ms
ok 232       + predicate                                  46 ms
ok 233       + numa                                       10 ms
ok 234       + eager_aggregate                           133 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                   88 ms
ok 236       + event_trigger                              96 ms
ok 237       - event_trigger_login                        17 ms
ok 238       - fast_default                               64 ms
ok 239       - tablespace                                154 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
