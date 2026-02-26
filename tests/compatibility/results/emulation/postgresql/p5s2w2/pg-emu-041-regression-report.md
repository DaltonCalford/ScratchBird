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
# using temp instance on port 58928 with PID 960521
ok 1         - test_setup                                104 ms
# parallel group (20 tests):  text varchar oid name char pg_lsn int8 int2 boolean txid int4 regproc money float4 float8 bit enum uuid numeric rangetypes
ok 2         + boolean                                    18 ms
ok 3         + char                                       16 ms
ok 4         + name                                       15 ms
ok 5         + varchar                                    10 ms
ok 6         + text                                        9 ms
ok 7         + int2                                       16 ms
ok 8         + int4                                       18 ms
ok 9         + int8                                       16 ms
ok 10        + oid                                        12 ms
ok 11        + float4                                     22 ms
ok 12        + float8                                     25 ms
ok 13        + bit                                        25 ms
ok 14        + numeric                                   106 ms
ok 15        + txid                                       16 ms
ok 16        + uuid                                       42 ms
ok 17        + enum                                       31 ms
ok 18        + money                                      18 ms
ok 19        + rangetypes                                139 ms
ok 20        + pg_lsn                                     14 ms
ok 21        + regproc                                    15 ms
# parallel group (20 tests):  md5 lseg circle line path timetz point time macaddr numerology macaddr8 date inet interval strings polygon box multirangetypes timestamp timestamptz
ok 22        + strings                                    55 ms
ok 23        + md5                                         7 ms
ok 24        + numerology                                 17 ms
ok 25        + point                                      15 ms
ok 26        + lseg                                        8 ms
ok 27        + line                                       12 ms
ok 28        + box                                        84 ms
ok 29        + path                                       12 ms
ok 30        + polygon                                    75 ms
ok 31        + circle                                      8 ms
ok 32        + date                                       25 ms
ok 33        + time                                       15 ms
ok 34        + timetz                                     12 ms
ok 35        + timestamp                                 321 ms
ok 36        + timestamptz                               344 ms
ok 37        + interval                                   27 ms
ok 38        + inet                                       26 ms
ok 39        + macaddr                                    15 ms
ok 40        + macaddr8                                   17 ms
ok 41        + multirangetypes                            98 ms
# parallel group (19 tests):  comments unicode euc_kr misc_sanity pg_ndistinct xid encoding pg_dependencies tstypes oid8 mvcc expressions horology geometry type_sanity stats_import database opr_sanity regex
ok 42        + geometry                                   36 ms
ok 43        + horology                                   34 ms
ok 44        + tstypes                                    22 ms
ok 45        + regex                                     167 ms
ok 46        + type_sanity                                46 ms
ok 47        + opr_sanity                                150 ms
ok 48        + misc_sanity                                11 ms
ok 49        + comments                                    7 ms
ok 50        + expressions                                33 ms
ok 51        + unicode                                     8 ms
ok 52        + xid                                        16 ms
ok 53        + mvcc                                       32 ms
ok 54        + database                                  129 ms
ok 55        + stats_import                               86 ms
ok 56        + pg_ndistinct                               12 ms
ok 57        + pg_dependencies                            16 ms
ok 58        + oid8                                       24 ms
ok 59        + encoding                                   15 ms
ok 60        + euc_kr                                      8 ms
# parallel group (6 tests):  copyencoding copyselect copydml insert_conflict copy insert
ok 61        + copy                                       49 ms
ok 62        + copyselect                                 11 ms
ok 63        + copydml                                    14 ms
ok 64        + copyencoding                                6 ms
ok 65        + insert                                    112 ms
ok 66        + insert_conflict                            44 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_schema create_misc create_procedure create_table
ok 67        + create_function_c                           5 ms
ok 68        + create_misc                                20 ms
ok 69        + create_operator                            11 ms
ok 70        + create_procedure                           23 ms
ok 71        + create_table                              128 ms
ok 72        + create_type                                15 ms
ok 73        + create_schema                              16 ms
# parallel group (5 tests):  index_including create_view index_including_gist create_index_spgist create_index
ok 74        + create_index                              286 ms
ok 75        + create_index_spgist                       164 ms
ok 76        + create_view                                91 ms
ok 77        + index_including                            67 ms
ok 78        + index_including_gist                      109 ms
# parallel group (16 tests):  create_cast roleattributes errors create_aggregate hash_func drop_if_exists select create_function_sql typed_table create_am infinite_recurse vacuum constraints updatable_views inherit triggers
ok 79        + create_aggregate                           21 ms
ok 80        + create_function_sql                        53 ms
ok 81        + create_cast                                15 ms
ok 82        + constraints                               242 ms
ok 83        + triggers                                  339 ms
ok 84        + select                                     28 ms
ok 85        + inherit                                   333 ms
ok 86        + typed_table                                57 ms
ok 87        + vacuum                                    140 ms
ok 88        + drop_if_exists                             27 ms
ok 89        + updatable_views                           252 ms
ok 90        + roleattributes                             14 ms
ok 91        + create_am                                  64 ms
ok 92        + hash_func                                  20 ms
ok 93        + errors                                     14 ms
ok 94        + infinite_recurse                           81 ms
ok 95        - sanity_check                               42 ms
# parallel group (20 tests):  select_having select_distinct_on delete select_implicit case random namespace select_into prepared_xacts transactions portals select_distinct union arrays subselect update hash_index aggregates join btree_index
ok 96        + select_into                                41 ms
ok 97        + select_distinct                            62 ms
ok 98        + select_distinct_on                         17 ms
ok 99        + select_implicit                            27 ms
ok 100       + select_having                              17 ms
ok 101       + subselect                                 120 ms
ok 102       + union                                      75 ms
ok 103       + case                                       26 ms
ok 104       + join                                      303 ms
ok 105       + aggregates                                248 ms
ok 106       + transactions                               54 ms
ok 107       + random                                     26 ms
ok 108       + portals                                    58 ms
ok 109       + arrays                                    118 ms
ok 110       + btree_index                               474 ms
ok 111       + hash_index                                218 ms
ok 112       + update                                    167 ms
ok 113       + delete                                     19 ms
ok 114       + namespace                                  33 ms
ok 115       + prepared_xacts                             39 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock password object_address collate replica_identity groupingsets matview identity generated_stored rowsecurity spgist gin gist join_hash brin privileges
ok 116       + brin                                      562 ms
ok 117       + gin                                       343 ms
ok 118       + gist                                      351 ms
ok 119       + spgist                                    309 ms
ok 120       + privileges                                683 ms
ok 121       + init_privs                                 10 ms
ok 122       + security_label                             24 ms
ok 123       + collate                                    97 ms
ok 124       + matview                                   203 ms
ok 125       + lock                                       56 ms
ok 126       + replica_identity                          117 ms
ok 127       + rowsecurity                               284 ms
ok 128       + object_address                             87 ms
ok 129       + tablesample                                38 ms
ok 130       + groupingsets                              140 ms
ok 131       + drop_operator                              12 ms
ok 132       + password                                   78 ms
ok 133       + identity                                  219 ms
ok 134       + generated_stored                          277 ms
ok 135       + join_hash                                 550 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 37 ms
ok 137       + brin_multi                                 92 ms
# parallel group (20 tests):  async nls dbsize alter_operator collate.utf8 tidscan sysviews tid tsrf create_role misc_functions alter_generic tidrangescan misc incremental_sort merge collate.icu.utf8 create_table_like generated_virtual without_overlaps
ok 138       + create_table_like                         170 ms
ok 139       + alter_generic                              58 ms
ok 140       + alter_operator                             20 ms
ok 141       + misc                                       68 ms
ok 142       + async                                       5 ms
ok 143       + dbsize                                      8 ms
ok 144       + merge                                     140 ms
ok 145       + misc_functions                             51 ms
ok 146       + nls                                         6 ms
ok 147       + sysviews                                   32 ms
ok 148       + tsrf                                       36 ms
ok 149       + tid                                        35 ms
ok 150       + tidscan                                    27 ms
ok 151       + tidrangescan                               61 ms
ok 152       + collate.utf8                               22 ms
ok 153       + collate.icu.utf8                          162 ms
ok 154       + incremental_sort                           67 ms
ok 155       + create_role                                41 ms
ok 156       + without_overlaps                          255 ms
ok 157       + generated_virtual                         180 ms
# parallel group (8 tests):  collate.windows.win1252 collate.linux.utf8 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     136 ms
ok 159       + psql                                      250 ms
ok 160       + psql_crosstab                              12 ms
ok 161       + psql_pipeline                              14 ms
ok 162       + amutils                                     9 ms
ok 163       + stats_ext                                 649 ms
ok 164       + collate.linux.utf8                          5 ms
ok 165       + collate.windows.win1252                     5 ms
ok 166       - select_parallel                           315 ms
ok 167       - write_parallel                             38 ms
ok 168       - vacuum_parallel                            38 ms
ok 169       - maintain_every                             11 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               150 ms
ok 171       + subscription                               47 ms
# parallel group (18 tests):  portals_p2 advisory_lock tsdicts xmlmap combocid equivclass functional_deps guc dependency stats_rewrite select_views tsearch window bitmapops cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                               68 ms
ok 173       + portals_p2                                 17 ms
ok 174       + foreign_key                               502 ms
ok 175       + cluster                                   139 ms
ok 176       + dependency                                 45 ms
ok 177       + guc                                        42 ms
ok 178       + bitmapops                                 131 ms
ok 179       + combocid                                   27 ms
ok 180       + tsearch                                   105 ms
ok 181       + tsdicts                                    26 ms
ok 182       + foreign_data                              257 ms
ok 183       + window                                    115 ms
ok 184       + xmlmap                                     26 ms
ok 185       + functional_deps                            39 ms
ok 186       + advisory_lock                              20 ms
ok 187       + indirect_toast                            153 ms
ok 188       + equivclass                                 38 ms
ok 189       + stats_rewrite                              65 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson sqljson_jsontable jsonb_jsonpath json sqljson_queryfuncs jsonb
ok 190       + json                                       40 ms
ok 191       + jsonb                                     110 ms
ok 192       + json_encoding                               8 ms
ok 193       + jsonpath                                   14 ms
ok 194       + jsonpath_encoding                           7 ms
ok 195       + jsonb_jsonpath                             34 ms
ok 196       + sqljson                                    27 ms
ok 197       + sqljson_queryfuncs                         45 ms
ok 198       + sqljson_jsontable                          27 ms
# parallel group (18 tests):  prepare limit xml plancache conversion returning polymorphism copy2 sequence largeobject with rowtypes truncate rangefuncs domain temp plpgsql alter_table
ok 199       + plancache                                  72 ms
ok 200       + limit                                      40 ms
ok 201       + plpgsql                                   224 ms
ok 202       + copy2                                     101 ms
ok 203       + temp                                      142 ms
ok 204       + domain                                    139 ms
ok 205       + rangefuncs                                135 ms
ok 206       + prepare                                    22 ms
ok 207       + conversion                                 72 ms
ok 208       + truncate                                  132 ms
ok 209       + alter_table                               552 ms
ok 210       + sequence                                  102 ms
ok 211       + polymorphism                               90 ms
ok 212       + rowtypes                                  115 ms
ok 213       + returning                                  83 ms
ok 214       + largeobject                               111 ms
ok 215       + with                                      111 ms
ok 216       + xml                                        43 ms
# parallel group (18 tests):  numa compression_lz4 hash_part explain predicate reloptions partition_info compression memoize eager_aggregate partition_merge partition_split partition_aggregate partition_prune partition_join tuplesort indexing stats
ok 217       + partition_merge                           208 ms
ok 218       + partition_split                           258 ms
ok 219       + partition_join                            359 ms
ok 220       + partition_prune                           337 ms
ok 221       + reloptions                                 40 ms
ok 222       + hash_part                                  25 ms
ok 223       + indexing                                  437 ms
ok 224       + partition_aggregate                       297 ms
ok 225       + partition_info                             48 ms
ok 226       + tuplesort                                 391 ms
ok 227       + explain                                    37 ms
ok 228       + compression                                72 ms
ok 229       + compression_lz4                            10 ms
ok 230       + memoize                                    89 ms
ok 231       + stats                                     476 ms
ok 232       + predicate                                  38 ms
ok 233       + numa                                        8 ms
ok 234       + eager_aggregate                           134 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                   83 ms
ok 236       + event_trigger                              92 ms
ok 237       - event_trigger_login                        18 ms
ok 238       - fast_default                               43 ms
ok 239       - tablespace                                158 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
