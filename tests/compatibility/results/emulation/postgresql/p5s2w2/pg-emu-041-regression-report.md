Last updated: 2026-03-07

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
# using temp instance on port 58928 with PID 1752179
ok 1         - test_setup                                104 ms
# parallel group (20 tests):  boolean oid char int2 float4 varchar name text money int8 regproc int4 pg_lsn txid float8 bit enum uuid numeric rangetypes
ok 2         + boolean                                    14 ms
ok 3         + char                                       16 ms
ok 4         + name                                       19 ms
ok 5         + varchar                                    18 ms
ok 6         + text                                       19 ms
ok 7         + int2                                       17 ms
ok 8         + int4                                       23 ms
ok 9         + int8                                       21 ms
ok 10        + oid                                        13 ms
ok 11        + float4                                     17 ms
ok 12        + float8                                     24 ms
ok 13        + bit                                        28 ms
ok 14        + numeric                                   115 ms
ok 15        + txid                                       23 ms
ok 16        + uuid                                       49 ms
ok 17        + enum                                       38 ms
ok 18        + money                                      18 ms
ok 19        + rangetypes                                149 ms
ok 20        + pg_lsn                                     21 ms
ok 21        + regproc                                    20 ms
# parallel group (20 tests):  lseg path line circle time md5 numerology point date timetz macaddr macaddr8 inet interval strings box polygon multirangetypes timestamp timestamptz
ok 22        + strings                                    47 ms
ok 23        + md5                                        12 ms
ok 24        + numerology                                 18 ms
ok 25        + point                                      18 ms
ok 26        + lseg                                        6 ms
ok 27        + line                                        9 ms
ok 28        + box                                        83 ms
ok 29        + path                                        8 ms
ok 30        + polygon                                    89 ms
ok 31        + circle                                     10 ms
ok 32        + date                                       20 ms
ok 33        + time                                       10 ms
ok 34        + timetz                                     20 ms
ok 35        + timestamp                                 330 ms
ok 36        + timestamptz                               342 ms
ok 37        + interval                                   40 ms
ok 38        + inet                                       22 ms
ok 39        + macaddr                                    19 ms
ok 40        + macaddr8                                   20 ms
ok 41        + multirangetypes                           101 ms
# parallel group (19 tests):  comments unicode euc_kr misc_sanity oid8 pg_ndistinct pg_dependencies tstypes expressions xid mvcc encoding horology geometry type_sanity stats_import database opr_sanity regex
ok 42        + geometry                                   44 ms
ok 43        + horology                                   42 ms
ok 44        + tstypes                                    22 ms
ok 45        + regex                                     175 ms
ok 46        + type_sanity                                48 ms
ok 47        + opr_sanity                                138 ms
ok 48        + misc_sanity                                10 ms
ok 49        + comments                                    8 ms
ok 50        + expressions                                23 ms
ok 51        + unicode                                     8 ms
ok 52        + xid                                        26 ms
ok 53        + mvcc                                       28 ms
ok 54        + database                                  130 ms
ok 55        + stats_import                               79 ms
ok 56        + pg_ndistinct                               15 ms
ok 57        + pg_dependencies                            16 ms
ok 58        + oid8                                       10 ms
ok 59        + encoding                                   28 ms
ok 60        + euc_kr                                      6 ms
# parallel group (6 tests):  copyencoding copyselect copydml copy insert_conflict insert
ok 61        + copy                                       36 ms
ok 62        + copyselect                                 12 ms
ok 63        + copydml                                    14 ms
ok 64        + copyencoding                                8 ms
ok 65        + insert                                    117 ms
ok 66        + insert_conflict                            57 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_misc create_schema create_procedure create_table
ok 67        + create_function_c                           5 ms
ok 68        + create_misc                                16 ms
ok 69        + create_operator                             8 ms
ok 70        + create_procedure                           32 ms
ok 71        + create_table                              125 ms
ok 72        + create_type                                11 ms
ok 73        + create_schema                              19 ms
# parallel group (5 tests):  index_including_gist index_including create_view create_index_spgist create_index
ok 74        + create_index                              268 ms
ok 75        + create_index_spgist                       181 ms
ok 76        + create_view                               128 ms
ok 77        + index_including                            84 ms
ok 78        + index_including_gist                       78 ms
# parallel group (16 tests):  create_cast roleattributes errors create_aggregate hash_func drop_if_exists select create_function_sql typed_table create_am infinite_recurse vacuum constraints updatable_views triggers inherit
ok 79        + create_aggregate                           21 ms
ok 80        + create_function_sql                        55 ms
ok 81        + create_cast                                10 ms
ok 82        + constraints                               247 ms
ok 83        + triggers                                  310 ms
ok 84        + select                                     29 ms
ok 85        + inherit                                   384 ms
ok 86        + typed_table                                60 ms
ok 87        + vacuum                                    147 ms
ok 88        + drop_if_exists                             27 ms
ok 89        + updatable_views                           248 ms
ok 90        + roleattributes                             17 ms
ok 91        + create_am                                  67 ms
ok 92        + hash_func                                  20 ms
ok 93        + errors                                     17 ms
ok 94        + infinite_recurse                           83 ms
ok 95        - sanity_check                               44 ms
# parallel group (20 tests):  select_having case delete select_distinct_on select_implicit random namespace prepared_xacts select_into portals select_distinct transactions union subselect arrays update aggregates join hash_index btree_index
ok 96        + select_into                                47 ms
ok 97        + select_distinct                            63 ms
ok 98        + select_distinct_on                         30 ms
ok 99        + select_implicit                            32 ms
ok 100       + select_having                              18 ms
ok 101       + subselect                                 131 ms
ok 102       + union                                      81 ms
ok 103       + case                                       25 ms
ok 104       + join                                      248 ms
ok 105       + aggregates                                244 ms
ok 106       + transactions                               62 ms
ok 107       + random                                     35 ms
ok 108       + portals                                    57 ms
ok 109       + arrays                                    140 ms
ok 110       + btree_index                               461 ms
ok 111       + hash_index                                250 ms
ok 112       + update                                    163 ms
ok 113       + delete                                     24 ms
ok 114       + namespace                                  37 ms
ok 115       + prepared_xacts                             38 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock object_address password collate replica_identity groupingsets identity matview generated_stored rowsecurity spgist gin gist join_hash brin privileges
ok 116       + brin                                      584 ms
ok 117       + gin                                       361 ms
ok 118       + gist                                      363 ms
ok 119       + spgist                                    306 ms
ok 120       + privileges                                712 ms
ok 121       + init_privs                                  8 ms
ok 122       + security_label                             34 ms
ok 123       + collate                                   100 ms
ok 124       + matview                                   226 ms
ok 125       + lock                                       49 ms
ok 126       + replica_identity                          125 ms
ok 127       + rowsecurity                               285 ms
ok 128       + object_address                             85 ms
ok 129       + tablesample                                42 ms
ok 130       + groupingsets                              161 ms
ok 131       + drop_operator                              13 ms
ok 132       + password                                   88 ms
ok 133       + identity                                  217 ms
ok 134       + generated_stored                          253 ms
ok 135       + join_hash                                 577 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 41 ms
ok 137       + brin_multi                                 82 ms
# parallel group (20 tests):  async dbsize nls tid tidscan tsrf collate.utf8 sysviews alter_operator create_role misc_functions alter_generic misc tidrangescan incremental_sort merge collate.icu.utf8 create_table_like generated_virtual without_overlaps
ok 138       + create_table_like                         189 ms
ok 139       + alter_generic                              61 ms
ok 140       + alter_operator                             35 ms
ok 141       + misc                                       61 ms
ok 142       + async                                       6 ms
ok 143       + dbsize                                     12 ms
ok 144       + merge                                     135 ms
ok 145       + misc_functions                             59 ms
ok 146       + nls                                        12 ms
ok 147       + sysviews                                   31 ms
ok 148       + tsrf                                       29 ms
ok 149       + tid                                        23 ms
ok 150       + tidscan                                    27 ms
ok 151       + tidrangescan                               64 ms
ok 152       + collate.utf8                               30 ms
ok 153       + collate.icu.utf8                          176 ms
ok 154       + incremental_sort                           73 ms
ok 155       + create_role                                39 ms
ok 156       + without_overlaps                          262 ms
ok 157       + generated_virtual                         193 ms
# parallel group (8 tests):  collate.windows.win1252 collate.linux.utf8 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     158 ms
ok 159       + psql                                      258 ms
ok 160       + psql_crosstab                              14 ms
ok 161       + psql_pipeline                              14 ms
ok 162       + amutils                                     8 ms
ok 163       + stats_ext                                 601 ms
ok 164       + collate.linux.utf8                          5 ms
ok 165       + collate.windows.win1252                     5 ms
ok 166       - select_parallel                           336 ms
ok 167       - write_parallel                             37 ms
ok 168       - vacuum_parallel                            39 ms
ok 169       - maintain_every                             11 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               155 ms
ok 171       + subscription                               37 ms
# parallel group (18 tests):  portals_p2 advisory_lock combocid xmlmap tsdicts guc functional_deps equivclass dependency select_views stats_rewrite window tsearch cluster bitmapops indirect_toast foreign_data foreign_key
ok 172       + select_views                               61 ms
ok 173       + portals_p2                                 13 ms
ok 174       + foreign_key                               468 ms
ok 175       + cluster                                   131 ms
ok 176       + dependency                                 51 ms
ok 177       + guc                                        33 ms
ok 178       + bitmapops                                 132 ms
ok 179       + combocid                                   18 ms
ok 180       + tsearch                                   130 ms
ok 181       + tsdicts                                    31 ms
ok 182       + foreign_data                              250 ms
ok 183       + window                                    114 ms
ok 184       + xmlmap                                     25 ms
ok 185       + functional_deps                            38 ms
ok 186       + advisory_lock                              15 ms
ok 187       + indirect_toast                            169 ms
ok 188       + equivclass                                 48 ms
ok 189       + stats_rewrite                              61 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson sqljson_jsontable json sqljson_queryfuncs jsonb_jsonpath jsonb
ok 190       + json                                       36 ms
ok 191       + jsonb                                      98 ms
ok 192       + json_encoding                               7 ms
ok 193       + jsonpath                                   17 ms
ok 194       + jsonpath_encoding                           6 ms
ok 195       + jsonb_jsonpath                             44 ms
ok 196       + sqljson                                    28 ms
ok 197       + sqljson_queryfuncs                         36 ms
ok 198       + sqljson_jsontable                          34 ms
# parallel group (18 tests):  prepare limit xml plancache conversion returning polymorphism rowtypes copy2 sequence rangefuncs largeobject temp truncate with domain plpgsql alter_table
ok 199       + plancache                                  74 ms
ok 200       + limit                                      45 ms
ok 201       + plpgsql                                   230 ms
ok 202       + copy2                                     105 ms
ok 203       + temp                                      124 ms
ok 204       + domain                                    153 ms
ok 205       + rangefuncs                                122 ms
ok 206       + prepare                                    23 ms
ok 207       + conversion                                 75 ms
ok 208       + truncate                                  124 ms
ok 209       + alter_table                               638 ms
ok 210       + sequence                                  106 ms
ok 211       + polymorphism                               89 ms
ok 212       + rowtypes                                   97 ms
ok 213       + returning                                  77 ms
ok 214       + largeobject                               122 ms
ok 215       + with                                      124 ms
ok 216       + xml                                        44 ms
# parallel group (18 tests):  numa compression_lz4 hash_part reloptions explain predicate partition_info memoize compression eager_aggregate partition_merge partition_split partition_aggregate partition_prune partition_join tuplesort indexing stats
ok 217       + partition_merge                           238 ms
ok 218       + partition_split                           266 ms
ok 219       + partition_join                            394 ms
ok 220       + partition_prune                           377 ms
ok 221       + reloptions                                 37 ms
ok 222       + hash_part                                  21 ms
ok 223       + indexing                                  450 ms
ok 224       + partition_aggregate                       337 ms
ok 225       + partition_info                             52 ms
ok 226       + tuplesort                                 404 ms
ok 227       + explain                                    44 ms
ok 228       + compression                                93 ms
ok 229       + compression_lz4                             8 ms
ok 230       + memoize                                    87 ms
ok 231       + stats                                     497 ms
ok 232       + predicate                                  45 ms
ok 233       + numa                                        7 ms
ok 234       + eager_aggregate                           146 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                   84 ms
ok 236       + event_trigger                              98 ms
ok 237       - event_trigger_login                        17 ms
ok 238       - fast_default                               47 ms
ok 239       - tablespace                                183 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
