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
# using temp instance on port 58928 with PID 3952885
ok 1         - test_setup                                102 ms
# parallel group (20 tests):  oid varchar int4 text char int2 name pg_lsn txid boolean money float4 int8 float8 regproc bit enum uuid numeric rangetypes
ok 2         + boolean                                    18 ms
ok 3         + char                                       11 ms
ok 4         + name                                       12 ms
ok 5         + varchar                                     8 ms
ok 6         + text                                        9 ms
ok 7         + int2                                       11 ms
ok 8         + int4                                        8 ms
ok 9         + int8                                       20 ms
ok 10        + oid                                         7 ms
ok 11        + float4                                     18 ms
ok 12        + float8                                     19 ms
ok 13        + bit                                        26 ms
ok 14        + numeric                                   107 ms
ok 15        + txid                                       15 ms
ok 16        + uuid                                       43 ms
ok 17        + enum                                       33 ms
ok 18        + money                                      16 ms
ok 19        + rangetypes                                140 ms
ok 20        + pg_lsn                                     13 ms
ok 21        + regproc                                    20 ms
# parallel group (20 tests):  lseg md5 path line circle time point macaddr timetz macaddr8 numerology date inet interval strings polygon box multirangetypes timestamp timestamptz
ok 22        + strings                                    58 ms
ok 23        + md5                                         8 ms
ok 24        + numerology                                 19 ms
ok 25        + point                                      17 ms
ok 26        + lseg                                        6 ms
ok 27        + line                                       11 ms
ok 28        + box                                        77 ms
ok 29        + path                                       10 ms
ok 30        + polygon                                    74 ms
ok 31        + circle                                     12 ms
ok 32        + date                                       28 ms
ok 33        + time                                       12 ms
ok 34        + timetz                                     17 ms
ok 35        + timestamp                                 321 ms
ok 36        + timestamptz                               345 ms
ok 37        + interval                                   35 ms
ok 38        + inet                                       29 ms
ok 39        + macaddr                                    16 ms
ok 40        + macaddr8                                   17 ms
ok 41        + multirangetypes                            98 ms
# parallel group (19 tests):  unicode euc_kr comments pg_ndistinct misc_sanity pg_dependencies tstypes oid8 xid encoding expressions horology mvcc geometry type_sanity stats_import opr_sanity regex database
ok 42        + geometry                                   42 ms
ok 43        + horology                                   33 ms
ok 44        + tstypes                                    20 ms
ok 45        + regex                                     141 ms
ok 46        + type_sanity                                44 ms
ok 47        + opr_sanity                                116 ms
ok 48        + misc_sanity                                13 ms
ok 49        + comments                                   11 ms
ok 50        + expressions                                29 ms
ok 51        + unicode                                     7 ms
ok 52        + xid                                        25 ms
ok 53        + mvcc                                       35 ms
ok 54        + database                                  149 ms
ok 55        + stats_import                               84 ms
ok 56        + pg_ndistinct                               10 ms
ok 57        + pg_dependencies                            16 ms
ok 58        + oid8                                       20 ms
ok 59        + encoding                                   25 ms
ok 60        + euc_kr                                      9 ms
# parallel group (6 tests):  copyencoding copyselect copydml copy insert_conflict insert
ok 61        + copy                                       44 ms
ok 62        + copyselect                                 13 ms
ok 63        + copydml                                    15 ms
ok 64        + copyencoding                                9 ms
ok 65        + insert                                    117 ms
ok 66        + insert_conflict                            57 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_schema create_misc create_procedure create_table
ok 67        + create_function_c                           5 ms
ok 68        + create_misc                                18 ms
ok 69        + create_operator                            11 ms
ok 70        + create_procedure                           21 ms
ok 71        + create_table                              126 ms
ok 72        + create_type                                15 ms
ok 73        + create_schema                              17 ms
# parallel group (5 tests):  index_including index_including_gist create_view create_index_spgist create_index
ok 74        + create_index                              279 ms
ok 75        + create_index_spgist                       162 ms
ok 76        + create_view                               118 ms
ok 77        + index_including                            67 ms
ok 78        + index_including_gist                       90 ms
# parallel group (16 tests):  create_cast errors hash_func create_aggregate roleattributes select drop_if_exists typed_table create_function_sql create_am infinite_recurse vacuum constraints updatable_views triggers inherit
ok 79        + create_aggregate                           19 ms
ok 80        + create_function_sql                        55 ms
ok 81        + create_cast                                 8 ms
ok 82        + constraints                               217 ms
ok 83        + triggers                                  295 ms
ok 84        + select                                     26 ms
ok 85        + inherit                                   364 ms
ok 86        + typed_table                                46 ms
ok 87        + vacuum                                    142 ms
ok 88        + drop_if_exists                             29 ms
ok 89        + updatable_views                           236 ms
ok 90        + roleattributes                             21 ms
ok 91        + create_am                                  58 ms
ok 92        + hash_func                                  14 ms
ok 93        + errors                                     12 ms
ok 94        + infinite_recurse                          118 ms
ok 95        - sanity_check                               37 ms
# parallel group (20 tests):  select_having select_implicit case select_distinct_on delete random prepared_xacts namespace select_into portals select_distinct transactions union arrays subselect update aggregates hash_index join btree_index
ok 96        + select_into                                52 ms
ok 97        + select_distinct                            68 ms
ok 98        + select_distinct_on                         31 ms
ok 99        + select_implicit                            24 ms
ok 100       + select_having                              19 ms
ok 101       + subselect                                 128 ms
ok 102       + union                                      77 ms
ok 103       + case                                       27 ms
ok 104       + join                                      301 ms
ok 105       + aggregates                                253 ms
ok 106       + transactions                               75 ms
ok 107       + random                                     39 ms
ok 108       + portals                                    61 ms
ok 109       + arrays                                    113 ms
ok 110       + btree_index                               463 ms
ok 111       + hash_index                                259 ms
ok 112       + update                                    152 ms
ok 113       + delete                                     31 ms
ok 114       + namespace                                  43 ms
ok 115       + prepared_xacts                             40 ms
# parallel group (20 tests):  init_privs drop_operator security_label lock tablesample object_address password collate replica_identity groupingsets matview identity generated_stored rowsecurity spgist gin gist join_hash brin privileges
ok 116       + brin                                      572 ms
ok 117       + gin                                       358 ms
ok 118       + gist                                      369 ms
ok 119       + spgist                                    309 ms
ok 120       + privileges                                705 ms
ok 121       + init_privs                                  6 ms
ok 122       + security_label                             25 ms
ok 123       + collate                                   102 ms
ok 124       + matview                                   207 ms
ok 125       + lock                                       40 ms
ok 126       + replica_identity                          118 ms
ok 127       + rowsecurity                               288 ms
ok 128       + object_address                             74 ms
ok 129       + tablesample                                45 ms
ok 130       + groupingsets                              150 ms
ok 131       + drop_operator                              10 ms
ok 132       + password                                   90 ms
ok 133       + identity                                  219 ms
ok 134       + generated_stored                          270 ms
ok 135       + join_hash                                 562 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 38 ms
ok 137       + brin_multi                                 79 ms
# parallel group (20 tests):  async dbsize nls collate.utf8 tsrf tid alter_operator tidscan sysviews create_role misc_functions alter_generic tidrangescan misc incremental_sort merge collate.icu.utf8 create_table_like generated_virtual without_overlaps
ok 138       + create_table_like                         183 ms
ok 139       + alter_generic                              61 ms
ok 140       + alter_operator                             27 ms
ok 141       + misc                                       64 ms
ok 142       + async                                       6 ms
ok 143       + dbsize                                      7 ms
ok 144       + merge                                     145 ms
ok 145       + misc_functions                             44 ms
ok 146       + nls                                         8 ms
ok 147       + sysviews                                   37 ms
ok 148       + tsrf                                       21 ms
ok 149       + tid                                        21 ms
ok 150       + tidscan                                    33 ms
ok 151       + tidrangescan                               61 ms
ok 152       + collate.utf8                               20 ms
ok 153       + collate.icu.utf8                          175 ms
ok 154       + incremental_sort                           74 ms
ok 155       + create_role                                39 ms
ok 156       + without_overlaps                          264 ms
ok 157       + generated_virtual                         203 ms
# parallel group (8 tests):  collate.linux.utf8 collate.windows.win1252 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     148 ms
ok 159       + psql                                      250 ms
ok 160       + psql_crosstab                              10 ms
ok 161       + psql_pipeline                              15 ms
ok 162       + amutils                                     6 ms
ok 163       + stats_ext                                 653 ms
ok 164       + collate.linux.utf8                          4 ms
ok 165       + collate.windows.win1252                     4 ms
ok 166       - select_parallel                           338 ms
ok 167       - write_parallel                             39 ms
ok 168       - vacuum_parallel                            43 ms
ok 169       - maintain_every                             13 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               146 ms
ok 171       + subscription                               31 ms
# parallel group (18 tests):  advisory_lock portals_p2 combocid tsdicts xmlmap functional_deps guc dependency equivclass stats_rewrite select_views window tsearch bitmapops cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                               67 ms
ok 173       + portals_p2                                 18 ms
ok 174       + foreign_key                               451 ms
ok 175       + cluster                                   153 ms
ok 176       + dependency                                 43 ms
ok 177       + guc                                        41 ms
ok 178       + bitmapops                                 137 ms
ok 179       + combocid                                   24 ms
ok 180       + tsearch                                   123 ms
ok 181       + tsdicts                                    26 ms
ok 182       + foreign_data                              252 ms
ok 183       + window                                    118 ms
ok 184       + xmlmap                                     26 ms
ok 185       + functional_deps                            35 ms
ok 186       + advisory_lock                              15 ms
ok 187       + indirect_toast                            167 ms
ok 188       + equivclass                                 45 ms
ok 189       + stats_rewrite                              60 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson sqljson_jsontable sqljson_queryfuncs json jsonb_jsonpath jsonb
ok 190       + json                                       43 ms
ok 191       + jsonb                                     101 ms
ok 192       + json_encoding                               8 ms
ok 193       + jsonpath                                   15 ms
ok 194       + jsonpath_encoding                           6 ms
ok 195       + jsonb_jsonpath                             47 ms
ok 196       + sqljson                                    30 ms
ok 197       + sqljson_queryfuncs                         34 ms
ok 198       + sqljson_jsontable                          31 ms
# parallel group (18 tests):  prepare limit xml conversion plancache returning polymorphism rowtypes copy2 sequence largeobject with truncate rangefuncs temp domain plpgsql alter_table
ok 199       + plancache                                  71 ms
ok 200       + limit                                      53 ms
ok 201       + plpgsql                                   247 ms
ok 202       + copy2                                     107 ms
ok 203       + temp                                      147 ms
ok 204       + domain                                    151 ms
ok 205       + rangefuncs                                146 ms
ok 206       + prepare                                    22 ms
ok 207       + conversion                                 55 ms
ok 208       + truncate                                  143 ms
ok 209       + alter_table                               548 ms
ok 210       + sequence                                  114 ms
ok 211       + polymorphism                               97 ms
ok 212       + rowtypes                                   98 ms
ok 213       + returning                                  89 ms
ok 214       + largeobject                               123 ms
ok 215       + with                                      128 ms
ok 216       + xml                                        51 ms
# parallel group (18 tests):  compression_lz4 numa hash_part predicate partition_info reloptions explain memoize compression eager_aggregate partition_merge partition_split partition_aggregate partition_join partition_prune tuplesort indexing stats
ok 217       + partition_merge                           226 ms
ok 218       + partition_split                           275 ms
ok 219       + partition_join                            368 ms
ok 220       + partition_prune                           377 ms
ok 221       + reloptions                                 49 ms
ok 222       + hash_part                                  18 ms
ok 223       + indexing                                  473 ms
ok 224       + partition_aggregate                       332 ms
ok 225       + partition_info                             48 ms
ok 226       + tuplesort                                 414 ms
ok 227       + explain                                    49 ms
ok 228       + compression                                95 ms
ok 229       + compression_lz4                             6 ms
ok 230       + memoize                                    93 ms
ok 231       + stats                                     504 ms
ok 232       + predicate                                  40 ms
ok 233       + numa                                        9 ms
ok 234       + eager_aggregate                           148 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                   78 ms
ok 236       + event_trigger                              87 ms
ok 237       - event_trigger_login                        15 ms
ok 238       - fast_default                               57 ms
ok 239       - tablespace                                155 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
