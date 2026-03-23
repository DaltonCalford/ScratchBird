Last updated: 2026-03-22

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
ake -j1  checkprep >>'<outside-tree-path>'/tmp_install/log/install.log 2>&1
PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  initdb --auth trust --no-sync --no-instructions --lc-messages=C --no-clean '<outside-tree-path>'/tmp_install/initdb-template >>'<outside-tree-path>'/tmp_install/log/initdb-template.log 2>&1
echo "# +++ regress check in src/test/regress +++" && PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  ../../../src/test/regress/pg_regress --temp-instance=./tmp_check --inputdir=<outside-tree-path> --bindir=     --dlpath=. --max-concurrent-tests=20  --schedule=<outside-tree-path>  
# +++ regress check in src/test/regress +++
# initializing database system by copying initdb template
# using temp instance on port 58928 with PID 689296
ok 1         - test_setup                                238 ms
# parallel group (20 tests):  text varchar int4 oid char int2 txid name float4 pg_lsn money boolean regproc int8 float8 bit enum uuid numeric rangetypes
ok 2         + boolean                                    49 ms
ok 3         + char                                       34 ms
ok 4         + name                                       39 ms
ok 5         + varchar                                    30 ms
ok 6         + text                                       27 ms
ok 7         + int2                                       33 ms
ok 8         + int4                                       30 ms
ok 9         + int8                                       58 ms
ok 10        + oid                                        29 ms
ok 11        + float4                                     39 ms
ok 12        + float8                                     63 ms
ok 13        + bit                                        69 ms
ok 14        + numeric                                   293 ms
ok 15        + txid                                       37 ms
ok 16        + uuid                                      139 ms
ok 17        + enum                                       86 ms
ok 18        + money                                      41 ms
ok 19        + rangetypes                                362 ms
ok 20        + pg_lsn                                     36 ms
ok 21        + regproc                                    47 ms
# parallel group (20 tests):  md5 lseg path line point circle timetz macaddr time numerology macaddr8 date inet interval strings polygon box multirangetypes timestamp timestamptz
ok 22        + strings                                   156 ms
ok 23        + md5                                        16 ms
ok 24        + numerology                                 49 ms
ok 25        + point                                      32 ms
ok 26        + lseg                                       24 ms
ok 27        + line                                       31 ms
ok 28        + box                                       219 ms
ok 29        + path                                       24 ms
ok 30        + polygon                                   192 ms
ok 31        + circle                                     31 ms
ok 32        + date                                       68 ms
ok 33        + time                                       40 ms
ok 34        + timetz                                     34 ms
ok 35        + timestamp                                 342 ms
ok 36        + timestamptz                               392 ms
ok 37        + interval                                   92 ms
ok 38        + inet                                       70 ms
ok 39        + macaddr                                    36 ms
ok 40        + macaddr8                                   42 ms
ok 41        + multirangetypes                           275 ms
# parallel group (19 tests):  comments unicode misc_sanity pg_ndistinct euc_kr tstypes pg_dependencies oid8 xid expressions encoding mvcc horology type_sanity geometry stats_import database opr_sanity regex
ok 42        + geometry                                  117 ms
ok 43        + horology                                  103 ms
ok 44        + tstypes                                    49 ms
ok 45        + regex                                     441 ms
ok 46        + type_sanity                               112 ms
ok 47        + opr_sanity                                347 ms
ok 48        + misc_sanity                                26 ms
ok 49        + comments                                   16 ms
ok 50        + expressions                                79 ms
ok 51        + unicode                                    20 ms
ok 52        + xid                                        74 ms
ok 53        + mvcc                                       98 ms
ok 54        + database                                  305 ms
ok 55        + stats_import                              242 ms
ok 56        + pg_ndistinct                               24 ms
ok 57        + pg_dependencies                            46 ms
ok 58        + oid8                                       51 ms
ok 59        + encoding                                   81 ms
ok 60        + euc_kr                                     23 ms
# parallel group (6 tests):  copyencoding copyselect copydml copy insert_conflict insert
ok 61        + copy                                      128 ms
ok 62        + copyselect                                 29 ms
ok 63        + copydml                                    34 ms
ok 64        + copyencoding                               16 ms
ok 65        + insert                                    328 ms
ok 66        + insert_conflict                           162 ms
# parallel group (7 tests):  create_function_c create_operator create_schema create_type create_misc create_procedure create_table
ok 67        + create_function_c                          14 ms
ok 68        + create_misc                                51 ms
ok 69        + create_operator                            27 ms
ok 70        + create_procedure                           60 ms
ok 71        + create_table                              336 ms
ok 72        + create_type                                39 ms
ok 73        + create_schema                              34 ms
# parallel group (5 tests):  index_including index_including_gist create_view create_index_spgist create_index
ok 74        + create_index                              697 ms
ok 75        + create_index_spgist                       357 ms
ok 76        + create_view                               281 ms
ok 77        + index_including                           169 ms
ok 78        + index_including_gist                      186 ms
# parallel group (16 tests):  create_cast errors hash_func create_aggregate roleattributes select drop_if_exists typed_table create_function_sql create_am infinite_recurse vacuum constraints updatable_views triggers inherit
ok 79        + create_aggregate                           36 ms
ok 80        + create_function_sql                       163 ms
ok 81        + create_cast                                19 ms
ok 82        + constraints                               729 ms
ok 83        + triggers                                  960 ms
ok 84        + select                                     73 ms
ok 85        + inherit                                  1022 ms
ok 86        + typed_table                               108 ms
ok 87        + vacuum                                    444 ms
ok 88        + drop_if_exists                             81 ms
ok 89        + updatable_views                           750 ms
ok 90        + roleattributes                             52 ms
ok 91        + create_am                                 174 ms
ok 92        + hash_func                                  33 ms
ok 93        + errors                                     28 ms
ok 94        + infinite_recurse                          212 ms
ok 95        - sanity_check                              106 ms
# parallel group (20 tests):  select_having select_implicit select_distinct_on case delete prepared_xacts namespace random select_into transactions portals select_distinct union arrays subselect update hash_index aggregates join btree_index
ok 96        + select_into                               137 ms
ok 97        + select_distinct                           191 ms
ok 98        + select_distinct_on                         44 ms
ok 99        + select_implicit                            44 ms
ok 100       + select_having                              29 ms
ok 101       + subselect                                 379 ms
ok 102       + union                                     202 ms
ok 103       + case                                       57 ms
ok 104       + join                                      810 ms
ok 105       + aggregates                                670 ms
ok 106       + transactions                              166 ms
ok 107       + random                                    125 ms
ok 108       + portals                                   173 ms
ok 109       + arrays                                    370 ms
ok 110       + btree_index                              1296 ms
ok 111       + hash_index                                597 ms
ok 112       + update                                    486 ms
ok 113       + delete                                     67 ms
ok 114       + namespace                                 122 ms
ok 115       + prepared_xacts                            102 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock password object_address collate replica_identity groupingsets matview identity spgist generated_stored rowsecurity gin gist join_hash brin privileges
ok 116       + brin                                     1532 ms
ok 117       + gin                                       935 ms
ok 118       + gist                                      948 ms
ok 119       + spgist                                    866 ms
ok 120       + privileges                               1852 ms
ok 121       + init_privs                                 18 ms
ok 122       + security_label                             70 ms
ok 123       + collate                                   289 ms
ok 124       + matview                                   628 ms
ok 125       + lock                                      129 ms
ok 126       + replica_identity                          348 ms
ok 127       + rowsecurity                               924 ms
ok 128       + object_address                            214 ms
ok 129       + tablesample                               100 ms
ok 130       + groupingsets                              446 ms
ok 131       + drop_operator                              33 ms
ok 132       + password                                  211 ms
ok 133       + identity                                  664 ms
ok 134       + generated_stored                          916 ms
ok 135       + join_hash                                1513 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 87 ms
ok 137       + brin_multi                                230 ms
# parallel group (20 tests):  async dbsize nls tidscan alter_operator tsrf collate.utf8 sysviews tid create_role misc_functions tidrangescan alter_generic incremental_sort misc merge collate.icu.utf8 create_table_like generated_virtual without_overlaps
ok 138       + create_table_like                         490 ms
ok 139       + alter_generic                             163 ms
ok 140       + alter_operator                             70 ms
ok 141       + misc                                      218 ms
ok 142       + async                                      16 ms
ok 143       + dbsize                                     19 ms
ok 144       + merge                                     399 ms
ok 145       + misc_functions                            125 ms
ok 146       + nls                                        29 ms
ok 147       + sysviews                                   98 ms
ok 148       + tsrf                                       72 ms
ok 149       + tid                                       100 ms
ok 150       + tidscan                                    66 ms
ok 151       + tidrangescan                              144 ms
ok 152       + collate.utf8                               70 ms
ok 153       + collate.icu.utf8                          458 ms
ok 154       + incremental_sort                          203 ms
ok 155       + create_role                               119 ms
ok 156       + without_overlaps                          681 ms
ok 157       + generated_virtual                         546 ms
# parallel group (8 tests):  collate.windows.win1252 collate.linux.utf8 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     362 ms
ok 159       + psql                                      650 ms
ok 160       + psql_crosstab                              32 ms
ok 161       + psql_pipeline                              43 ms
ok 162       + amutils                                    19 ms
ok 163       + stats_ext                                1636 ms
ok 164       + collate.linux.utf8                         12 ms
ok 165       + collate.windows.win1252                    10 ms
ok 166       - select_parallel                           790 ms
ok 167       - write_parallel                             90 ms
ok 168       - vacuum_parallel                           114 ms
ok 169       - maintain_every                             29 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               491 ms
ok 171       + subscription                               68 ms
# parallel group (18 tests):  portals_p2 advisory_lock combocid xmlmap tsdicts equivclass guc dependency functional_deps select_views stats_rewrite window tsearch bitmapops cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                              174 ms
ok 173       + portals_p2                                 41 ms
ok 174       + foreign_key                              1229 ms
ok 175       + cluster                                   403 ms
ok 176       + dependency                                144 ms
ok 177       + guc                                       135 ms
ok 178       + bitmapops                                 385 ms
ok 179       + combocid                                   63 ms
ok 180       + tsearch                                   377 ms
ok 181       + tsdicts                                   107 ms
ok 182       + foreign_data                              741 ms
ok 183       + window                                    325 ms
ok 184       + xmlmap                                     86 ms
ok 185       + functional_deps                           144 ms
ok 186       + advisory_lock                              46 ms
ok 187       + indirect_toast                            412 ms
ok 188       + equivclass                                130 ms
ok 189       + stats_rewrite                             197 ms
# parallel group (9 tests):  json_encoding jsonpath_encoding jsonpath sqljson_jsontable sqljson json sqljson_queryfuncs jsonb_jsonpath jsonb
ok 190       + json                                      109 ms
ok 191       + jsonb                                     294 ms
ok 192       + json_encoding                              16 ms
ok 193       + jsonpath                                   36 ms
ok 194       + jsonpath_encoding                          17 ms
ok 195       + jsonb_jsonpath                            128 ms
ok 196       + sqljson                                    89 ms
ok 197       + sqljson_queryfuncs                        120 ms
ok 198       + sqljson_jsontable                          82 ms
# parallel group (18 tests):  prepare limit xml plancache conversion returning polymorphism rowtypes copy2 sequence largeobject rangefuncs temp truncate with domain plpgsql alter_table
ok 199       + plancache                                 156 ms
ok 200       + limit                                      99 ms
ok 201       + plpgsql                                   713 ms
ok 202       + copy2                                     280 ms
ok 203       + temp                                      356 ms
ok 204       + domain                                    412 ms
ok 205       + rangefuncs                                347 ms
ok 206       + prepare                                    49 ms
ok 207       + conversion                                169 ms
ok 208       + truncate                                  359 ms
ok 209       + alter_table                              1702 ms
ok 210       + sequence                                  286 ms
ok 211       + polymorphism                              231 ms
ok 212       + rowtypes                                  258 ms
ok 213       + returning                                 205 ms
ok 214       + largeobject                               313 ms
ok 215       + with                                      364 ms
ok 216       + xml                                       142 ms
# parallel group (18 tests):  compression_lz4 numa hash_part reloptions explain predicate partition_info compression memoize eager_aggregate partition_merge partition_split partition_aggregate stats partition_prune partition_join tuplesort indexing
ok 217       + partition_merge                           602 ms
ok 218       + partition_split                           705 ms
ok 219       + partition_join                            991 ms
ok 220       + partition_prune                           957 ms
ok 221       + reloptions                                 98 ms
ok 222       + hash_part                                  59 ms
ok 223       + indexing                                 1181 ms
ok 224       + partition_aggregate                       826 ms
ok 225       + partition_info                            155 ms
ok 226       + tuplesort                                 998 ms
ok 227       + explain                                   111 ms
ok 228       + compression                               210 ms
ok 229       + compression_lz4                            18 ms
ok 230       + memoize                                   227 ms
ok 231       + stats                                     861 ms
ok 232       + predicate                                 125 ms
ok 233       + numa                                       21 ms
ok 234       + eager_aggregate                           349 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                  264 ms
ok 236       + event_trigger                             301 ms
ok 237       - event_trigger_login                        38 ms
ok 238       - fast_default                              185 ms
ok 239       - tablespace                                515 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
